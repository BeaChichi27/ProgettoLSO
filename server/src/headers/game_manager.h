#ifndef GAME_MANAGER_H
#define GAME_MANAGER_H

/*
 * HEADER GAME_MANAGER - GESTIONE PARTITE SERVER TRIS
 *
 * Questo header definisce tutte le strutture dati e funzioni necessarie
 * per la gestione delle partite sul lato server del gioco Tris multiplayer.
 * 
 * Include la gestione completa del ciclo di vita delle partite:
 * creazione, join, approvazione, gameplay, rematch e cleanup.
 *
 * Supporta fino a MAX_GAMES partite simultanee con gestione thread-safe
 * tramite mutex per operazioni concorrenti.
 *
 * Compatibile con Windows e sistemi Unix/Linux.
 */

#include "network.h"
#include <time.h>

// ========== CONFIGURAZIONI ==========

#define MAX_GAMES 50                    // Numero massimo di partite simultanee

// ========== ENUMERAZIONI ==========

/**
 * Stati possibili di una partita durante il suo ciclo di vita.
 */
typedef enum {
    GAME_STATE_WAITING,             // In attesa di un secondo giocatore
    GAME_STATE_PENDING_APPROVAL,    // In attesa dell'approvazione del creatore
    GAME_STATE_PLAYING,             // Partita in corso
    GAME_STATE_OVER,                // Partita terminata
    GAME_STATE_REMATCH_REQUESTED    // Richiesta di rematch in corso
} GameState;

/**
 * Simboli dei giocatori e celle vuote del tabellone.
 */
typedef enum {
    PLAYER_NONE = ' ',              // Cella vuota
    PLAYER_X = 'X',                 // Giocatore X (primo giocatore/creatore)
    PLAYER_O = 'O'                  // Giocatore O (secondo giocatore)
} PlayerSymbol;

// ========== STRUTTURE DATI ==========

// Cross-platform mutex definition (già definito in network.h)
// Usa la definizione di mutex_t dal network.h

/**
 * Struttura principale che rappresenta una partita completa.
 * Contiene tutti i dati necessari per gestire il gameplay e lo stato.
 */
typedef struct {
    int game_id;                    // ID univoco della partita
    Client* player1;                // Primo giocatore (creatore, simbolo X)
    Client* player2;                // Secondo giocatore (partecipante, simbolo O)
    Client* pending_player;         // Giocatore in attesa di approvazione
    char board[3][3];              // Tabellone di gioco 3x3
    PlayerSymbol current_player;    // Giocatore che deve fare la mossa corrente
    GameState state;               // Stato attuale della partita
    PlayerSymbol winner;           // Vincitore della partita (se presente)
    int is_draw;                   // Flag per pareggio
    int rematch_requests;          // Bit field: 1=player1 vuole rematch, 2=player2 vuole rematch
    int rematch_declined;          // Bit field: 1=player1 ha rifiutato, 2=player2 ha rifiutato
    Client* rematch_requester;     // Chi ha richiesto per primo il rematch (avrà simbolo X)
    time_t creation_time;          // Timestamp di creazione della partita
    mutex_t mutex;                 // Mutex per accesso thread-safe
} Game;

// ========== FUNZIONI DI GESTIONE MUTEX ==========

int mutex_init(mutex_t *mutex);
int mutex_lock(mutex_t *mutex);
int mutex_unlock(mutex_t *mutex);
int mutex_destroy(mutex_t *mutex);

// ========== FUNZIONI DI INIZIALIZZAZIONE ==========

int game_manager_init();
void game_manager_cleanup();

// ========== FUNZIONI DI GESTIONE PARTITE ==========

int game_create_new(Client *creator);
int game_join(Client *client, int game_id);
int game_approve_join(Client *creator, int approve);
int game_request_rematch(Client *client);
int game_cancel_rematch(Client *client); // Aggiunta nuova funzione
int game_decline_rematch(Client *client); // Rifiuta il rematch
void game_leave(Client *client);

// ========== FUNZIONI DI GAMEPLAY ==========


int game_make_move(int game_id, Client *client, int row, int col);
void game_reset(int game_id);

// ========== FUNZIONI DI RICERCA E UTILITY ==========

Game* game_find_by_id(int game_id);
void game_list_available(char *response, size_t max_len);
void game_broadcast_to_all_clients(const char *message);

// ========== FUNZIONI DI LOGICA DI GIOCO ==========

void game_init_board(Game *game);
int game_check_winner(Game *game);
int game_is_valid_move(Game *game, int row, int col);
int game_is_board_full(Game *game);

#endif