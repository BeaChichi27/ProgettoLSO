
#include "protocol.h"
#include "tris_client.h"
#include "utils.h"


int parse_message(const char* raw_message, protocol_message_t* msg) {
    if (!raw_message || !msg) return -1;

    
    const char* space = strchr(raw_message, ' ');
    if (!space) {
        
        strncpy(msg->command, raw_message, sizeof(msg->command) - 1);
        msg->data[0] = '\0';
    } else {
        
        size_t cmd_len = space - raw_message;
        strncpy(msg->command, raw_message, MIN(cmd_len, sizeof(msg->command) - 1));
        msg->command[cmd_len] = '\0';

        
        strncpy(msg->data, space + 1, sizeof(msg->data) - 1);
        msg->data[sizeof(msg->data) - 1] = '\0';
    }

    return 0;
}


void handle_menu_message(client_context_t* ctx, const protocol_message_t* msg) {
    if (!ctx || !msg) return;

    if (strcmp(msg->command, MSG_SERVER_MENU) == 0) {
        
        printf("Menu ricevuto: %s\n", msg->data);
    } else if (strcmp(msg->command, MSG_SERVER_GAME_LIST) == 0) {
        
        display_game_list(msg->data);
    }
}


void handle_lobby_message(client_context_t* ctx, const protocol_message_t* msg) {
    if (!ctx || !msg) return;

    if (strcmp(msg->command, MSG_SERVER_WAITING_PLAYER) == 0) {
        printf("In attesa di un avversario...\n");
    } else if (strcmp(msg->command, MSG_SERVER_START) == 0) {
        
        ctx->state = STATE_PLAYING;
        printf("Partita iniziata!\n");
    }
}


void handle_game_message(client_context_t* ctx, const protocol_message_t* msg) {
    if (!ctx || !msg) return;

    if (strcmp(msg->command, MSG_SERVER_BOARD) == 0) {
        
        memcpy(ctx->board, msg->data, 9);
    } else if (strcmp(msg->command, MSG_SERVER_YOUR_TURN) == 0) {
        ctx->is_my_turn = 1;
    } else if (strcmp(msg->command, MSG_SERVER_OPPONENT_TURN) == 0) {
        ctx->is_my_turn = 0;
    }
}