#include "headers/game_manager.h"
#include "headers/network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Game games[MAX_GAMES];
static mutex_t games_mutex;  // CAMBIATO: era CRITICAL_SECTION
static int next_game_id = 1;

int mutex_init(mutex_t *mutex) {
    #ifdef _WIN32
        InitializeCriticalSection(&mutex->win_mutex);
        return 0;
    #else
        return pthread_mutex_init(&mutex->nix_mutex, NULL);
    #endif
}

int mutex_lock(mutex_t *mutex) {
    #ifdef _WIN32
        EnterCriticalSection(&mutex->win_mutex);
        return 0;
    #else
        return pthread_mutex_lock(&mutex->nix_mutex);
    #endif
}

int mutex_unlock(mutex_t *mutex) {
    #ifdef _WIN32
        LeaveCriticalSection(&mutex->win_mutex);
        return 0;
    #else
        return pthread_mutex_unlock(&mutex->nix_mutex);
    #endif
}

int mutex_destroy(mutex_t *mutex) {
    #ifdef _WIN32
        DeleteCriticalSection(&mutex->win_mutex);
        return 0;
    #else
        return pthread_mutex_destroy(&mutex->nix_mutex);
    #endif
}

int game_manager_init() {
    if (mutex_init(&games_mutex) != 0) return 0;
    
    if (mutex_lock(&games_mutex) != 0) return 0;
    
    for (int i = 0; i < MAX_GAMES; i++) {
        memset(&games[i], 0, sizeof(Game));
        games[i].game_id = -1;
        games[i].state = GAME_STATE_WAITING;
        if (mutex_init(&games[i].mutex) != 0) {
            mutex_unlock(&games_mutex);
            return 0;
        }
    }
    
    if (mutex_unlock(&games_mutex) != 0) return 0;
    
    printf("Game Manager inizializzato\n");
    return 1;
}

void game_manager_cleanup() {
    if (mutex_lock(&games_mutex) != 0) return;
    
    for (int i = 0; i < MAX_GAMES; i++) {
        mutex_destroy(&games[i].mutex);
    }
    
    mutex_unlock(&games_mutex);
    mutex_destroy(&games_mutex);
    
    printf("Game Manager pulito\n");
}

Game* game_find_by_id(int game_id) {
    mutex_lock(&games_mutex);  // CAMBIATO: era EnterCriticalSection
    for (int i = 0; i < MAX_GAMES; i++) {
        if (games[i].game_id == game_id) {
            mutex_unlock(&games_mutex);  // CAMBIATO: era LeaveCriticalSection
            return &games[i];
        }
    }
    mutex_unlock(&games_mutex);  // CAMBIATO: era LeaveCriticalSection
    return NULL;
}

static Game* game_find_free_slot() {
    for (int i = 0; i < MAX_GAMES; i++) {
        if (games[i].game_id == -1) {
            return &games[i];
        }
    }
    return NULL;
}

void game_init_board(Game *game) {
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            game->board[i][j] = PLAYER_NONE;
        }
    }
}

int game_create_new(Client *creator) {
    if (!creator) return -1;
    
    mutex_lock(&games_mutex);  // CAMBIATO: era EnterCriticalSection
    Game *game = game_find_free_slot();
    if (!game) {
        mutex_unlock(&games_mutex);  // CAMBIATO: era LeaveCriticalSection
        return -1;
    }
    
    game->game_id = next_game_id++;
    game->player1 = creator;
    game->player2 = NULL;
    game->current_player = PLAYER_X;
    game->state = GAME_STATE_WAITING;
    game->winner = PLAYER_NONE;
    game->is_draw = 0;
    game->creation_time = time(NULL);  // AGGIUNTO: era mancante
    game_init_board(game);
    
    creator->game_id = game->game_id;
    creator->symbol = 'X';
    
    mutex_unlock(&games_mutex);  // CAMBIATO: era LeaveCriticalSection
    
    printf("Partita %d creata da %s\n", game->game_id, creator->name);
    return game->game_id;
}

int game_join(Client *client, int game_id) {
    if (!client) return 0;
    Game *game = game_find_by_id(game_id);
    if (!game) return 0;
    
    mutex_lock(&game->mutex);  // CAMBIATO: era EnterCriticalSection
    if (game->state != GAME_STATE_WAITING || !game->player1 || game->player2) {
        mutex_unlock(&game->mutex);  // CAMBIATO: era LeaveCriticalSection
        return 0;
    }
    
    game->player2 = client;
    game->state = GAME_STATE_PLAYING;
    client->game_id = game_id;
    client->symbol = 'O';
    
    mutex_unlock(&game->mutex);  // CAMBIATO: era LeaveCriticalSection
    
    printf("Client %s si e' unito alla partita %d\n", client->name, game_id);
    network_send_to_client(client, "JOIN_ACCEPTED:O");
    network_send_to_client(game->player1, "OPPONENT_JOINED");
    network_send_to_client(game->player1, "GAME_START:X");
    network_send_to_client(game->player2, "GAME_START:O");
    return 1;
}

void game_leave(Client *client) {
    if (!client || client->game_id <= 0) return;
    Game *game = game_find_by_id(client->game_id);
    if (!game) return;
    
    mutex_lock(&game->mutex);  // CAMBIATO: era EnterCriticalSection
    Client *opponent = NULL;
    if (game->player1 == client) {
        opponent = game->player2;
        game->player1 = NULL;
    } else if (game->player2 == client) {
        opponent = game->player1;
        game->player2 = NULL;
    }
    
    if (opponent) {
        network_send_to_client(opponent, "OPPONENT_LEFT");
        opponent->game_id = -1;
    }
    
    game->game_id = -1;
    game->state = GAME_STATE_WAITING;
    printf("Partita %d cancellata\n", client->game_id);
    
    client->game_id = -1;
    mutex_unlock(&game->mutex);  // CAMBIATO: era LeaveCriticalSection
}

int game_check_winner(Game *game) {
    // Controllo righe
    for (int i = 0; i < 3; i++) {
        if (game->board[i][0] != PLAYER_NONE &&
            game->board[i][0] == game->board[i][1] && 
            game->board[i][1] == game->board[i][2]) {
            return game->board[i][0];
        }
    }
    
    // Controllo colonne
    for (int j = 0; j < 3; j++) {
        if (game->board[0][j] != PLAYER_NONE &&
            game->board[0][j] == game->board[1][j] && 
            game->board[1][j] == game->board[2][j]) {
            return game->board[0][j];
        }
    }
    
    // Controllo diagonale principale
    if (game->board[0][0] != PLAYER_NONE &&
        game->board[0][0] == game->board[1][1] && 
        game->board[1][1] == game->board[2][2]) {
        return game->board[0][0];
    }
    
    // Controllo diagonale secondaria
    if (game->board[0][2] != PLAYER_NONE &&
        game->board[0][2] == game->board[1][1] && 
        game->board[1][1] == game->board[2][0]) {
        return game->board[0][2];
    }
    
    return PLAYER_NONE;
}

int game_is_board_full(Game *game) {
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            if (game->board[i][j] == PLAYER_NONE) {
                return 0;
            }
        }
    }
    return 1;
}

int game_is_valid_move(Game *game, int row, int col) {
    if (row < 0 || row > 2 || col < 0 || col > 2) {
        return 0;
    }
    return game->board[row][col] == PLAYER_NONE;
}

int game_make_move(int game_id, Client *client, int row, int col) {
    Game *game = game_find_by_id(game_id);
    if (!game || !client) return 0;
    
    mutex_lock(&game->mutex);  // CAMBIATO: era EnterCriticalSection
    
    PlayerSymbol expected_player = (game->current_player == PLAYER_X) ? PLAYER_X : PLAYER_O;
    if ((char)client->symbol != (char)expected_player) {
        network_send_to_client(client, "ERROR:Non e' il tuo turno");
        mutex_unlock(&game->mutex);  // CAMBIATO: era LeaveCriticalSection
        return 0;
    }
    
    if (!game_is_valid_move(game, row, col)) {
        network_send_to_client(client, "ERROR:Mossa non valida");
        mutex_unlock(&game->mutex);  // CAMBIATO: era LeaveCriticalSection
        return 0;
    }
    
    game->board[row][col] = (char)client->symbol;
    
    PlayerSymbol winner = (PlayerSymbol)game_check_winner(game);
    if (winner != PLAYER_NONE) {
        game->state = GAME_STATE_OVER;
        game->winner = winner;
        char msg[100];
        sprintf(msg, "GAME_OVER:WINNER:%c", winner);
        network_send_to_client(game->player1, msg);
        network_send_to_client(game->player2, msg);
        printf("Partita %d terminata - Vincitore: %c\n", game_id, winner);
    }
    else if (game_is_board_full(game)) {
        game->state = GAME_STATE_OVER;
        game->is_draw = 1;
        network_send_to_client(game->player1, "GAME_OVER:DRAW");
        network_send_to_client(game->player2, "GAME_OVER:DRAW");
        printf("Partita %d terminata - Pareggio\n", game_id);
    }
    else {
        game->current_player = (game->current_player == PLAYER_X) ? PLAYER_O : PLAYER_X;
        char move_msg[100];
        sprintf(move_msg, "MOVE:%d,%d:%c", row, col, client->symbol);
        network_send_to_client(game->player1, move_msg);
        network_send_to_client(game->player2, move_msg);
    }
    
    mutex_unlock(&game->mutex);  // CAMBIATO: era LeaveCriticalSection
    return 1;
}

void game_reset(int game_id) {
    Game *game = game_find_by_id(game_id);
    if (!game) return;
    
    mutex_lock(&game->mutex);  // CAMBIATO: era EnterCriticalSection
    game_init_board(game);
    game->current_player = PLAYER_X;
    game->state = GAME_STATE_PLAYING;
    game->winner = PLAYER_NONE;
    game->is_draw = 0;
    
    network_send_to_client(game->player1, "GAME_RESET");
    network_send_to_client(game->player2, "GAME_RESET");
    
    mutex_unlock(&game->mutex);  // CAMBIATO: era LeaveCriticalSection
    printf("Partita %d resettata\n", game_id);
}

void game_list_available(char *response, size_t max_len) {
    strcpy(response, "GAMES:");
    
    mutex_lock(&games_mutex);  // CAMBIATO: era EnterCriticalSection
    for (int i = 0; i < MAX_GAMES; i++) {
        if (games[i].game_id > 0 && games[i].state == GAME_STATE_WAITING && 
            games[i].player1 && !games[i].player2) {
            char game_info[100];
            sprintf(game_info, "[%d]%s ", games[i].game_id, games[i].player1->name);
            if (strlen(response) + strlen(game_info) < max_len - 1) {
                strcat(response, game_info);
            }
        }
    }
    mutex_unlock(&games_mutex);  // CAMBIATO: era LeaveCriticalSection
    
    if (strlen(response) == 6) {
        strcat(response, "Nessuna partita disponibile");
    }
}

void game_check_timeouts() {
    mutex_lock(&games_mutex);  // CAMBIATO: era EnterCriticalSection
    time_t now = time(NULL);
    
    for (int i = 0; i < MAX_GAMES; i++) {
        if (games[i].game_id != -1 && 
            games[i].state == GAME_STATE_WAITING &&
            difftime(now, games[i].creation_time) > 300) { 
            
            printf("Partita %d cancellata per timeout\n", games[i].game_id);
            if (games[i].player1) {
                network_send_to_client(games[i].player1, "ERROR:Timeout - Nessun avversario");
                games[i].player1->game_id = -1;
            }
            games[i].game_id = -1;
        }
    }
    mutex_unlock(&games_mutex);  // CAMBIATO: era LeaveCriticalSection
}