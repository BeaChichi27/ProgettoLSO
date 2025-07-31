#ifndef TRIS_CLIENT_H
#define TRIS_CLIENT_H


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/time.h>
#include <ctype.h>
#include <stdarg.h>


#define MAX_BUFFER 4096
#define MAX_NAME 50
#define MAX_CMD 256
#define SERVER_PORT 8080
#define SERVER_IP "127.0.0.1"


#define CLEAR_SCREEN() printf("\033[H\033[J")



#define MSG_CLIENT_REGISTER     "REGISTER"
#define MSG_CLIENT_CREATE       "CREATE"
#define MSG_CLIENT_JOIN         "JOIN"
#define MSG_CLIENT_MOVE         "MOVE"
#define MSG_CLIENT_QUIT         "QUIT"
#define MSG_CLIENT_ACCEPT       "ACCEPT"
#define MSG_CLIENT_REFUSE       "REFUSE"
#define MSG_CLIENT_REMATCH      "REMATCH"


#define MSG_SERVER_MENU         "MENU"
#define MSG_SERVER_GAME_LIST    "GAME_LIST"
#define MSG_SERVER_BOARD        "BOARD"
#define MSG_SERVER_YOUR_TURN    "YOUR_TURN"
#define MSG_SERVER_OPPONENT_TURN "OPPONENT_TURN"
#define MSG_SERVER_WIN          "WIN"
#define MSG_SERVER_LOSE         "LOSE"
#define MSG_SERVER_DRAW         "DRAW"
#define MSG_SERVER_INVALID_MOVE "INVALID_MOVE"
#define MSG_SERVER_START        "START"
#define MSG_SERVER_ERROR        "ERROR"


typedef enum {
    STATE_DISCONNECTED,
    STATE_MENU,
    STATE_LOBBY,
    STATE_PLAYING,
    STATE_GAME_OVER
} client_state_t;

typedef enum {
    PLAYER_NONE = ' ',
    PLAYER_X = 'X',
    PLAYER_O = 'O'
} player_symbol_t;

typedef struct {
    int socket_fd;
    client_state_t state;
    player_symbol_t symbol;
    char player_name[MAX_NAME];
    char opponent_name[MAX_NAME];
    char board[9];
    int is_my_turn;
    int game_id;
} client_context_t;


extern client_context_t g_client;
extern volatile int g_running;




void signal_handler(int sig);
int main_loop(void);


int initialize_client(client_context_t* ctx);
int connect_to_server(client_context_t* ctx);
int send_message(client_context_t* ctx, const char* message);
int receive_message(client_context_t* ctx, char* buffer, size_t size);
int send_formatted_message(client_context_t* ctx, const char* format, ...);
void cleanup_client(client_context_t* ctx);


void handle_server_message(client_context_t* ctx, const char* message);
void process_menu_state(client_context_t* ctx);
void process_lobby_state(client_context_t* ctx);
void process_playing_state(client_context_t* ctx);


void display_main_menu(void);
void display_game_board(const char board[9]);
void display_game_list(const char* games);
int get_user_choice(const char* prompt, const char* valid_choices);
int get_move_input(void);
void get_player_name(char* name, size_t max_len);
void print_error(const char* message);
void print_info(const char* message);
void wait_for_enter(void);


void initialize_board(char board[9]);
int is_valid_move(const char board[9], int position);
void make_move(char board[9], int position, char symbol);
int check_winner(const char board[9]);
int is_board_full(const char board[9]);
char get_symbol_at(const char board[9], int position);

#endif 