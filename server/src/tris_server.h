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

typedef struct {
    int socket;                 
    char name[MAX_NAME_LEN];    
    int udp_port;               
    player_state_t state;       
    int game_id;                
    time_t last_activity;       
    struct sockaddr_in addr;    
} player_t;

typedef struct {
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






void init_server(void);
void run_server(void);
void cleanup_server(void);


void *handle_client(void *arg);
void disconnect_player(player_t *player);


player_t *add_player(int sock, struct sockaddr_in addr);
void remove_player(player_t *player);


int create_game(player_t *player);
int join_game(player_t *player, int game_id);
void end_game(game_t *game, int winner);
void process_move(game_t *game, int player_idx, int position);


char *get_game_list(void);
char *get_player_list(void);


int send_response(int sock, const char *response);
int broadcast_game_state(game_t *game);
time_t get_current_time(void);


void log_message(const char *message);
void log_error(const char *error);

#endif 