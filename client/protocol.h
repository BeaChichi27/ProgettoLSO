#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "tris_client.h"


#define PROTOCOL_VERSION "1.0"
#define MAX_GAMES 10
#define TIMEOUT_SECONDS 30


#define MSG_SERVER_REGISTRATION_OK    "REGISTRATION_OK"
#define MSG_SERVER_REGISTRATION_ERROR "REGISTRATION_ERROR"
#define MSG_SERVER_WAITING_PLAYER     "WAITING_PLAYER"
#define MSG_SERVER_OPPONENT_LEFT      "OPPONENT_LEFT"
#define MSG_SERVER_JOIN_REQUEST       "JOIN_REQUEST"
#define MSG_SERVER_REMATCH_REQUEST    "REMATCH_REQUEST"


typedef struct {
    char command[32];
    char data[MAX_BUFFER - 32];
} protocol_message_t;


int parse_message(const char* raw_message, protocol_message_t* msg);
int build_message(char* buffer, size_t size, const char* command, const char* data);
void handle_menu_message(client_context_t* ctx, const protocol_message_t* msg);
void handle_game_message(client_context_t* ctx, const protocol_message_t* msg);
void handle_lobby_message(client_context_t* ctx, const protocol_message_t* msg);

#endif 