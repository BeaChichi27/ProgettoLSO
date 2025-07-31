#ifndef TRIS_SERVER_H
#define TRIS_SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>

#define MAX_PLAYERS             20      
#define MAX_GAMES              10      
#define MAX_NAME_LEN           50      
#define MAX_MSG_LEN            256     
#define SERVER_PORT            8080    
#define INACTIVITY_TIMEOUT     300     

typedef enum {
    PLAYER_DISCONNECTED = 0,
    PLAYER_IN_LOBBY,
    PLAYER_IN_GAME,
    PLAYER_WAITING
} player_state_t;

typedef enum {
    GAME_WAITING = 0,   
    GAME_ACTIVE,        
    GAME_OVER           
} game_state_t;

typedef struct player_t {
    int socket;                 
    char name[MAX_NAME_LEN];    
    int udp_port;               
    player_state_t state;       
    int game_id;                
    time_t last_activity;       
    struct sockaddr_in addr;    
    char symbol;                
} player_t;

typedef struct game_t {
    char board[9];              
    player_t *players[2];       
    game_state_t state;         
    int current_turn;           
    int game_id;                
    time_t start_time;          
} game_t;

typedef struct {
    player_t *players[MAX_PLAYERS]; 
    game_t *games[MAX_GAMES];       
    pthread_mutex_t lock;           
    int next_game_id;               
} server_state_t;


struct game_t;
struct player_t;


void init_server(void);
void run_server(void);
void cleanup_server(void);


int setup_server_socket(int port);
void handle_new_connection(int server_fd);
void set_socket_nonblocking(int sock);
void close_socket(int *sock);


void *handle_client(void *arg);
void process_client_command(player_t *player, const char *command);
int send_response(int sock, const char *response);


player_t *add_player(int sock, struct sockaddr_in addr);
void remove_player(player_t *player);
void check_inactive_players(void);
void cleanup_finished_games(void);


void handle_lobby_commands(player_t *player, const char *cmd, const char *arg);
void handle_list_command(player_t *player);
void handle_create_command(player_t *player);
void handle_join_command(player_t *player, int game_id);
char *get_game_list(void);
char *get_player_list(void);


void handle_game_commands(player_t *player, const char *cmd, const char *arg);
void process_move(game_t *game, int player_idx, int position);
void notify_game_start(game_t *game);
void notify_game_end(game_t *game, int winner);
int broadcast_game_state(game_t *game);


time_t get_current_time(void);
void trim_whitespace(char *str);
int is_valid_name(const char *name);
void log_message(const char *message);
void log_error(const char *error);


void initialize_board(char *board);
int is_valid_move(const char *board, int position);
int check_winner(const char *board);
int is_board_full(const char *board);
char *format_board(const char *board);
char *format_game_info(const game_t *game);

#endif