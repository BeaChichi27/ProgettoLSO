#include "tris_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

time_t get_current_time(void) {
    return time(NULL);
}

int is_valid_name(const char *name) {
    if (!name || strlen(name) == 0 || strlen(name) >= MAX_NAME_LEN) {
        return 0;
    }

    
    for (size_t i = 0; i < strlen(name); i++) {
        if (!isalnum(name[i]) && name[i] != ' ') {
            return 0;
        }
    }

    return 1;
}

void trim_whitespace(char *str) {
    if (!str) return;

    char *end;
    
    
    while(isspace((unsigned char)*str)) str++;

    if(*str == 0) {  
        *str = '\0';
        return;
    }

    
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;

    
    *(end+1) = '\0';
}

int send_response(int sock, const char *response) {
    if (sock < 0 || !response) {
        log_error("Invalid socket or response in send_response");
        return -1;
    }

    size_t len = strlen(response);
    if (send(sock, response, len, 0) != len) {
        log_error("Failed to send complete response");
        return -1;
    }

    return 0;
}





void initialize_board(char *board) {
    if (!board) return;
    memset(board, ' ', 9);
}

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





void log_message(const char *message) {
    if (!message) return;

    time_t now = time(NULL);
    char *time_str = ctime(&now);
    time_str[strlen(time_str)-1] = '\0'; 

    printf("[%s] %s\n", time_str, message);
}

void log_error(const char *error) {
    if (!error) return;

    time_t now = time(NULL);
    char *time_str = ctime(&now);
    time_str[strlen(time_str)-1] = '\0'; 

    fprintf(stderr, "[%s] ERROR: %s\n", time_str, error);
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

char *format_game_info(const game_t *game) {
    if (!game) return NULL;

    char *info = malloc(512);
    if (!info) {
        log_error("Memory allocation failed in format_game_info");
        return NULL;
    }

    const char *status;
    switch (game->state) {
        case GAME_WAITING: status = "In attesa"; break;
        case GAME_ACTIVE: status = "In corso"; break;
        case GAME_OVER: status = "Terminata"; break;
        default: status = "Sconosciuto";
    }

    snprintf(info, 512,
        "Partita ID: %d\n"
        "Stato: %s\n"
        "Giocatore 1: %s\n"
        "Giocatore 2: %s\n"
        "Turno corrente: %s\n",
        game->game_id,
        status,
        game->players[0] ? game->players[0]->name : "Nessuno",
        game->players[1] ? game->players[1]->name : "Nessuno",
        game->players[game->current_turn] ? game->players[game->current_turn]->name : "Nessuno");

    return info;
}