#include "headers/game_logic.h"
#include <stdio.h>
#include <string.h>

void game_init(Game *game) {
    memset(game, 0, sizeof(Game));
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            game->board[i][j] = PLAYER_NONE;
        }
    }
    
    game->current_player = PLAYER_X;
    game->state = GAME_STATE_WAITING;
    game->winner = PLAYER_NONE;
    game->is_draw = 0;
}

int game_make_move(Game *game, int row, int col) {
    if (!game_is_valid_move(game, row, col)) {
        return 0;
    }
    
    game->board[row][col] = (char)game->current_player;
    
    game_check_winner(game);
    
    if (game->state == GAME_STATE_PLAYING) {
        game->current_player = (game->current_player == PLAYER_X) ? PLAYER_O : PLAYER_X;
    }
    
    return 1;
}

int game_is_valid_move(const Game *game, int row, int col) {
    if (row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE) {
        return 0;
    }
    
    return (game->board[row][col] == PLAYER_NONE);
}

void game_check_winner(Game *game) {
    for (int i = 0; i < BOARD_SIZE; i++) {
        if (game->board[i][0] != PLAYER_NONE &&
            game->board[i][0] == game->board[i][1] && 
            game->board[i][1] == game->board[i][2]) {
            game->winner = (PlayerSymbol)game->board[i][0];
            game->state = GAME_STATE_OVER;
            return;
        }
    }
    
    for (int j = 0; j < BOARD_SIZE; j++) {
        if (game->board[0][j] != PLAYER_NONE &&
            game->board[0][j] == game->board[1][j] && 
            game->board[1][j] == game->board[2][j]) {
            game->winner = (PlayerSymbol)game->board[0][j];
            game->state = GAME_STATE_OVER;
            return;
        }
    }
    
    if (game->board[0][0] != PLAYER_NONE &&
        game->board[0][0] == game->board[1][1] && 
        game->board[1][1] == game->board[2][2]) {
        game->winner = (PlayerSymbol)game->board[0][0];
        game->state = GAME_STATE_OVER;
        return;
    }
    
    if (game->board[0][2] != PLAYER_NONE &&
        game->board[0][2] == game->board[1][1] && 
        game->board[1][1] == game->board[2][0]) {
        game->winner = (PlayerSymbol)game->board[0][2];
        game->state = GAME_STATE_OVER;
        return;
    }
    
    if (game_is_board_full(game)) {
        game->is_draw = 1;
        game->state = GAME_STATE_OVER;
    }
}

void game_reset(Game *game) {
    game_init(game);
    game->state = GAME_STATE_PLAYING;
}

void game_print_board(const Game *game) {
    printf("\n");
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            printf(" %c ", game->board[i][j]);
            if (j < BOARD_SIZE - 1) printf("|");
        }
        printf("\n");
        
        if (i < BOARD_SIZE - 1) {
            for (int j = 0; j < BOARD_SIZE; j++) {
                printf("---");
                if (j < BOARD_SIZE - 1) printf("+");
            }
            printf("\n");
        }
    }
    printf("\n");
}

int game_is_board_full(const Game *game) {
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (game->board[i][j] == PLAYER_NONE) {
                return 0;
            }
        }
    }
    return 1;
}

// Sostituisci la funzione game_process_network_message in game_logic.c

int game_process_network_message(Game *game, const char *message) {
    if (!game || !message) return 0;

    printf("Processando messaggio: %s\n", message);

    if (strncmp(message, "MOVE:", 5) == 0) {
        // Formato: "MOVE:<row>,<col>:<symbol>"
        int row, col;
        char symbol;
        if (sscanf(message + 5, "%d,%d:%c", &row, &col, &symbol) == 3) {
            printf("Mossa estratta: riga=%d, col=%d, simbolo=%c\n", row, col, symbol);
            
            if (row >= 0 && row < BOARD_SIZE && col >= 0 && col < BOARD_SIZE) {
                // Aggiorna il board
                game->board[row][col] = symbol;
                printf("Board aggiornato alla posizione [%d][%d] = %c\n", row, col, symbol);
                
                // Controlla vincitore
                game_check_winner(game);
                
                // Cambia turno solo se il gioco continua
                if (game->state == GAME_STATE_PLAYING) {
                    game->current_player = (game->current_player == PLAYER_X) ? PLAYER_O : PLAYER_X;
                    printf("Turno cambiato. Ora tocca a: %c\n", game->current_player);
                }
                
                return 1;
            } else {
                printf("Coordinate non valide: row=%d, col=%d\n", row, col);
            }
        } else {
            printf("Formato messaggio MOVE non valido: %s\n", message);
        }
    }
    else if (strcmp(message, "GAME_RESET") == 0 || strcmp(message, "RESET") == 0) {
        printf("Reset del gioco ricevuto\n");
        game_reset(game);
        return 1;
    }
    else if (strcmp(message, "REMATCH") == 0) {
        printf("Richiesta rematch ricevuta\n");
        game->state = GAME_STATE_REMATCH;
        return 1;
    }
    else if (strncmp(message, "GAME_OVER:", 10) == 0) {
        printf("Messaggio fine gioco ricevuto: %s\n", message);
        
        if (strstr(message, "WINNER:")) {
            char winner;
            if (sscanf(message, "GAME_OVER:WINNER:%c", &winner) == 1) {
                printf("Vincitore identificato: %c\n", winner);
                game->winner = (PlayerSymbol)winner;
                game->state = GAME_STATE_OVER;
                return 1;
            }
        } else if (strstr(message, "DRAW")) {
            printf("Pareggio identificato\n");
            game->is_draw = 1;
            game->state = GAME_STATE_OVER;
            return 1;
        }
    }
    
    printf("Messaggio non riconosciuto: %s\n", message);
    return 0;
}