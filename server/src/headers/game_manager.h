#ifndef GAME_MANAGER_H
#define GAME_MANAGER_H

#include "network.h"
#include <time.h>

#define MAX_GAMES 50

typedef enum {
    GAME_STATE_WAITING,
    GAME_STATE_PENDING_APPROVAL,
    GAME_STATE_PLAYING,
    GAME_STATE_OVER,
    GAME_STATE_REMATCH_REQUESTED
} GameState;

typedef enum {
    PLAYER_NONE = ' ',
    PLAYER_X = 'X',
    PLAYER_O = 'O'
} PlayerSymbol;

// Cross-platform mutex definition (già definito in network.h)
// Usa la definizione di mutex_t dal network.h

typedef struct {
    int game_id;
    Client* player1;
    Client* player2;
    Client* pending_player;  // Giocatore in attesa di approvazione
    char board[3][3];
    PlayerSymbol current_player;
    GameState state;
    PlayerSymbol winner;
    int is_draw;
    int rematch_requests;    // Bit field: 1=player1 wants rematch, 2=player2 wants rematch
    int rematch_declined;    // Bit field: 1=player1 declined, 2=player2 declined
    Client* rematch_requester; // Chi ha richiesto per primo il rematch (avrà X)
    time_t creation_time;
    mutex_t mutex;
} Game;

int mutex_init(mutex_t *mutex);
int mutex_lock(mutex_t *mutex);
int mutex_unlock(mutex_t *mutex);
int mutex_destroy(mutex_t *mutex);

int game_manager_init();
void game_manager_cleanup();

int game_create_new(Client *creator);
int game_join(Client *client, int game_id);
int game_approve_join(Client *creator, int approve);
int game_request_rematch(Client *client);
int game_cancel_rematch(Client *client); // Aggiunta nuova funzione
int game_decline_rematch(Client *client); // Rifiuta il rematch
void game_leave(Client *client);

int game_make_move(int game_id, Client *client, int row, int col);
void game_reset(int game_id);

Game* game_find_by_id(int game_id);
void game_list_available(char *response, size_t max_len);
void game_broadcast_to_all_clients(const char *message);

void game_init_board(Game *game);
int game_check_winner(Game *game);
int game_is_valid_move(Game *game, int row, int col);
int game_is_board_full(Game *game);

#endif