#include "tris_server.h"
#include <string.h>

extern server_state_t server_state;

void handle_lobby_commands(player_t *player, const char *cmd, const char *arg) {
    if (!player || !cmd) return;

    if (strcmp(cmd, "LIST") == 0) {
        handle_list_command(player);
    } 
    else if (strcmp(cmd, "CREATE") == 0) {
        handle_create_command(player);
    } 
    else if (strcmp(cmd, "JOIN") == 0) {
        if (!arg) {
            send_response(player->socket, "ERR Missing game ID\n");
            return;
        }
        handle_join_command(player, atoi(arg));
    }
    else {
        send_response(player->socket, "ERR Unknown command\n");
    }
}

void handle_list_command(player_t *player) {
    char *game_list = get_game_list();
    char *player_list = get_player_list();
    
    char response[1024];
    snprintf(response, sizeof(response), 
             "GAMES:\n%s\nPLAYERS:\n%s\n", 
             game_list ? game_list : "No games available",
             player_list ? player_list : "No players connected");
    
    send_response(player->socket, response);
    
    free(game_list);
    free(player_list);
}

void handle_create_command(player_t *player) {
    pthread_mutex_lock(&server_state.lock);
    
    
    if (player->game_id != -1) {
        pthread_mutex_unlock(&server_state.lock);
        send_response(player->socket, "ERR You are already in a game\n");
        return;
    }
    
    
    game_t *new_game = malloc(sizeof(game_t));
    if (!new_game) {
        pthread_mutex_unlock(&server_state.lock);
        send_response(player->socket, "ERR Server error\n");
        return;
    }
    
    memset(new_game, 0, sizeof(game_t));
    initialize_board(new_game->board);
    new_game->players[0] = player;
    new_game->state = GAME_WAITING;
    new_game->game_id = server_state.next_game_id++;
    new_game->start_time = get_current_time();
    
    
    int slot = -1;
    for (int i = 0; i < MAX_GAMES; i++) {
        if (!server_state.games[i]) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        free(new_game);
        pthread_mutex_unlock(&server_state.lock);
        send_response(player->socket, "ERR Server is full\n");
        return;
    }
    
    server_state.games[slot] = new_game;
    player->game_id = new_game->game_id;
    player->state = PLAYER_WAITING;
    
    pthread_mutex_unlock(&server_state.lock);
    
    char response[128];
    snprintf(response, sizeof(response), "OK Game %d created. Waiting for opponent...\n", new_game->game_id);
    send_response(player->socket, response);
}

void handle_join_command(player_t *player, int game_id) {
    pthread_mutex_lock(&server_state.lock);
    
    
    if (player->game_id != -1) {
        pthread_mutex_unlock(&server_state.lock);
        send_response(player->socket, "ERR You are already in a game\n");
        return;
    }
    
    
    game_t *game = NULL;
    int game_slot = -1;
    
    for (int i = 0; i < MAX_GAMES; i++) {
        if (server_state.games[i] && server_state.games[i]->game_id == game_id) {
            game = server_state.games[i];
            game_slot = i;
            break;
        }
    }
    
    if (!game || game->state != GAME_WAITING) {
        pthread_mutex_unlock(&server_state.lock);
        send_response(player->socket, "ERR Invalid game ID\n");
        return;
    }
    
    
    game->players[1] = player;
    game->state = GAME_ACTIVE;
    game->current_turn = 0; 
    
    player->game_id = game->game_id;
    player->state = PLAYER_IN_GAME;
    
    
    game->players[0]->symbol = 'X';
    game->players[1]->symbol = 'O';
    
    pthread_mutex_unlock(&server_state.lock);
    
    
    notify_game_start(game);
    broadcast_game_state(game);
}





char *get_game_list(void) {
    char *list = malloc(1024);
    if (!list) return NULL;
    
    list[0] = '\0';
    char buffer[128];
    
    pthread_mutex_lock(&server_state.lock);
    
    for (int i = 0; i < MAX_GAMES; i++) {
        if (server_state.games[i]) {
            const char *status;
            switch (server_state.games[i]->state) {
                case GAME_WAITING: status = "Waiting"; break;
                case GAME_ACTIVE: status = "Active"; break;
                case GAME_OVER: status = "Finished"; break;
                default: status = "Unknown";
            }
            
            snprintf(buffer, sizeof(buffer),
                     "%d: %s vs %s [%s]\n",
                     server_state.games[i]->game_id,
                     server_state.games[i]->players[0] ? server_state.games[i]->players[0]->name : "?",
                     server_state.games[i]->players[1] ? server_state.games[i]->players[1]->name : "?",
                     status);
            
            strcat(list, buffer);
        }
    }
    
    pthread_mutex_unlock(&server_state.lock);
    
    if (strlen(list) == 0) {
        free(list);
        return NULL;
    }
    
    return list;
}

char *get_player_list(void) {
    char *list = malloc(1024);
    if (!list) return NULL;
    
    list[0] = '\0';
    char buffer[128];
    
    pthread_mutex_lock(&server_state.lock);
    
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (server_state.players[i]) {
            const char *status;
            switch (server_state.players[i]->state) {
                case PLAYER_IN_LOBBY: status = "Lobby"; break;
                case PLAYER_IN_GAME: status = "In-Game"; break;
                case PLAYER_WAITING: status = "Waiting"; break;
                default: status = "Unknown";
            }
            
            snprintf(buffer, sizeof(buffer),
                     "%s [%s]%s\n",
                     server_state.players[i]->name,
                     status,
                     server_state.players[i]->game_id != -1 ? 
                        " (Game ID)" : "");
            
            strcat(list, buffer);
        }
    }
    
    pthread_mutex_unlock(&server_state.lock);
    
    if (strlen(list) == 0) {
        free(list);
        return NULL;
    }
    
    return list;
}





void handle_game_commands(player_t *player, const char *cmd, const char *arg) {
    if (!player || !cmd) return;

    pthread_mutex_lock(&server_state.lock);
    
    
    game_t *game = NULL;
    for (int i = 0; i < MAX_GAMES; i++) {
        if (server_state.games[i] && server_state.games[i]->game_id == player->game_id) {
            game = server_state.games[i];
            break;
        }
    }
    
    if (!game) {
        pthread_mutex_unlock(&server_state.lock);
        player->state = PLAYER_IN_LOBBY;
        player->game_id = -1;
        send_response(player->socket, "ERR Game not found\n");
        return;
    }
    
    int player_idx = (game->players[0] == player) ? 0 : 1;
    
    if (strcmp(cmd, "MOVE") == 0) {
        if (!arg) {
            pthread_mutex_unlock(&server_state.lock);
            send_response(player->socket, "ERR Missing move\n");
            return;
        }
        
        if (game->current_turn != player_idx) {
            pthread_mutex_unlock(&server_state.lock);
            send_response(player->socket, "ERR Not your turn\n");
            return;
        }
        
        int move = atoi(arg);
        if (move < 1 || move > 9) {
            pthread_mutex_unlock(&server_state.lock);
            send_response(player->socket, "ERR Invalid move (1-9)\n");
            return;
        }
        
        
        process_move(game, player_idx, move - 1);
    } 
    else if (strcmp(cmd, "LEAVE") == 0) {
        
        game->state = GAME_OVER;
        notify_game_end(game, 1 - player_idx); 
        
        
        if (game->players[0]) {
            game->players[0]->state = PLAYER_IN_LOBBY;
            game->players[0]->game_id = -1;
        }
        if (game->players[1]) {
            game->players[1]->state = PLAYER_IN_LOBBY;
            game->players[1]->game_id = -1;
        }
    }
    else {
        pthread_mutex_unlock(&server_state.lock);
        send_response(player->socket, "ERR Unknown command\n");
        return;
    }
    
    pthread_mutex_unlock(&server_state.lock);
}

void process_move(game_t *game, int player_idx, int position) {
    if (!game || position < 0 || position >= 9) return;
    
    
    if (game->board[position] != ' ') {
        send_response(game->players[player_idx]->socket, "ERR Position already taken\n");
        return;
    }
    
    
    game->board[position] = (player_idx == 0) ? 'X' : 'O';
    
    
    if (check_winner(game->board)) {
        game->state = GAME_OVER;
        notify_game_end(game, player_idx);
        
        
        if (game->players[0]) {
            game->players[0]->state = PLAYER_IN_LOBBY;
            game->players[0]->game_id = -1;
        }
        if (game->players[1]) {
            game->players[1]->state = PLAYER_IN_LOBBY;
            game->players[1]->game_id = -1;
        }
        return;
    }
    
    
    if (is_board_full(game->board)) {
        game->state = GAME_OVER;
        notify_game_end(game, -1); 
        
        
        if (game->players[0]) {
            game->players[0]->state = PLAYER_IN_LOBBY;
            game->players[0]->game_id = -1;
        }
        if (game->players[1]) {
            game->players[1]->state = PLAYER_IN_LOBBY;
            game->players[1]->game_id = -1;
        }
        return;
    }
    
    
    game->current_turn = 1 - game->current_turn;
    
    broadcast_game_state(game);
}