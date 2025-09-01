#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

/*
 * HEADER GAME_LOGIC - GESTIONE LOGICA DEL GIOCO TRIS
 *
 * Questo header definisce tutte le strutture dati e funzioni necessarie
 * per gestire la logica del gioco Tris (Tic-Tac-Toe).
 * 
 * Include la gestione del tabellone, validazione delle mosse,
 * controllo delle vittorie e gestione degli stati di gioco.
 *
 * Supporta modalità multiplayer con comunicazione di rete.
 */

// ========== COSTANTI ==========

#define BOARD_SIZE 3                    // Dimensione del tabellone (3x3)

// ========== ENUMERAZIONI ==========

/**
 * Stati possibili del gioco durante una partita.
 */
typedef enum {
    GAME_STATE_WAITING,     // In attesa di avversario o inizio partita
    GAME_STATE_PLAYING,     // Partita in corso
    GAME_STATE_OVER,        // Partita terminata (vittoria/pareggio)
    GAME_STATE_REMATCH      // In attesa di rematch
} GameState;

/**
 * Simboli dei giocatori e celle vuote del tabellone.
 */
typedef enum {
    PLAYER_NONE = ' ',      // Cella vuota
    PLAYER_X = 'X',         // Giocatore X (primo giocatore)
    PLAYER_O = 'O'          // Giocatore O (secondo giocatore)
} PlayerSymbol;

// ========== STRUTTURE DATI ==========

/**
 * Struttura principale che rappresenta lo stato completo di una partita.
 * Contiene il tabellone, i giocatori e tutte le informazioni necessarie.
 */
typedef struct {
    char board[BOARD_SIZE][BOARD_SIZE];  // Tabellone di gioco 3x3
    PlayerSymbol current_player;         // Giocatore che deve fare la mossa
    GameState state;                     // Stato attuale del gioco
    PlayerSymbol winner;                 // Vincitore della partita (se presente)
    int is_draw;                        // Flag per pareggio
} Game;

// ========== FUNZIONI DI INIZIALIZZAZIONE ==========

void game_init(Game *game);
void game_init_board(Game *game);

int game_make_move(Game *game, int row, int col);
int game_is_valid_move(const Game *game, int row, int col);
void game_check_winner(Game *game);
void game_reset(Game *game);

// ========== FUNZIONI DI VISUALIZZAZIONE ==========

void game_print_board(const Game *game);
int game_is_board_full(const Game *game);

// ========== FUNZIONI DI COMUNICAZIONE ==========

int game_process_network_message(Game *game, const char *message);

#endif 