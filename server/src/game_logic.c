#include "tris_server.h"
#include <string.h>
#include <stdlib.h>  




int is_valid_move(const char *board, int position) {
    if (!board || position < 0 || position >= 9) {
        return 0;
    }
    return board[position] == ' ';
}

int check_winner(const char *board) {
    
    for (int i = 0; i < 9; i += 3) {
        if (board[i] != ' ' && board[i] == board[i+1] && board[i] == board[i+2]) {
            return 1;
        }
    }

    
    for (int i = 0; i < 3; i++) {
        if (board[i] != ' ' && board[i] == board[i+3] && board[i] == board[i+6]) {
            return 1;
        }
    }

    
    if (board[0] != ' ' && board[0] == board[4] && board[0] == board[8]) {
        return 1;
    }
    if (board[2] != ' ' && board[2] == board[4] && board[2] == board[6]) {
        return 1;
    }

    return 0;
}

int is_board_full(const char *board) {
    for (int i = 0; i < 9; i++) {
        if (board[i] == ' ') {
            return 0;
        }
    }
    return 1;
}

void initialize_board(char *board) {
    if (!board) return;
    memset(board, ' ', 9);
}

char *format_board(const char *board) {
    if (!board) return NULL;

    char *formatted = malloc(256);
    if (!formatted) {
        log_error("Memory allocation failed in format_board");
        return NULL;
    }

    snprintf(formatted, 256,
        " %c | %c | %c \n"
        "---+---+---\n"
        " %c | %c | %c \n"
        "---+---+---\n"
        " %c | %c | %c \n",
        board[0], board[1], board[2],
        board[3], board[4], board[5],
        board[6], board[7], board[8]);

    return formatted;
}

void process_move(game_t *game, int player_idx, int position) {
    if (!game || position < 0 || position >= 9) return;

    if (!is_valid_move(game->board, position)) {
        send_response(game->players[player_idx]->socket, "ERR Position already taken\n");
        return;
    }

    
    game->board[position] = game->players[player_idx]->symbol;  

    
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

void notify_game_start(game_t *game) {
    if (!game || !game->players[0] || !game->players[1]) return;

    char message[256];
    snprintf(message, sizeof(message), 
             "START %d %s (%c) vs %s (%c)\n",  
             game->game_id,
             game->players[0]->name, game->players[0]->symbol,
             game->players[1]->name, game->players[1]->symbol);

    send_response(game->players[0]->socket, message);
    send_response(game->players[1]->socket, message);
}

void notify_game_end(game_t *game, int winner) {
    if (!game || !game->players[0] || !game->players[1]) return;

    char message[256];
    if (winner >= 0 && winner < 2) {
        snprintf(message, sizeof(message), 
                 "END WIN %s (%c)\n",  
                 game->players[winner]->name, 
                 game->players[winner]->symbol);
    } else {
        snprintf(message, sizeof(message), "END DRAW\n");
    }

    send_response(game->players[0]->socket, message);
    send_response(game->players[1]->socket, message);
}