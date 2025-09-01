#include "headers/game_logic.h"
#include <stdio.h>
#include <string.h>

/**
 * Inizializza una nuova struttura di gioco con valori di default.
 * Imposta la griglia vuota, il primo giocatore e lo stato iniziale.
 * 
 * @param game Puntatore alla struttura Game da inizializzare
 * 
 * @note La griglia viene riempita con PLAYER_NONE (spazi vuoti)
 * @note Il giocatore iniziale è sempre PLAYER_X
 * @note Lo stato iniziale è GAME_STATE_WAITING
 */
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

/**
 * Reinizializza solo la griglia di gioco mantenendo lo stato della partita.
 * Utilizzata per preparare una nuova partita mantenendo le impostazioni.
 * 
 * @param game Puntatore alla struttura Game da reinizializzare
 * 
 * @note Resetta solo il board, non tocca stato, giocatori o altre impostazioni
 * @note Utile per i rematch dove si mantiene la configurazione esistente
 */
void game_init_board(Game *game) {
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            game->board[i][j] = PLAYER_NONE;
        }
    }
}

/**
 * Esegue una mossa nel gioco dopo averla validata.
 * Piazza il simbolo del giocatore corrente, controlla la vittoria e cambia turno.
 * 
 * @param game Puntatore alla struttura Game
 * @param row Riga della mossa (0-2)
 * @param col Colonna della mossa (0-2)
 * @return 1 se la mossa è stata eseguita con successo, 0 se non valida
 * 
 * @note Valida automaticamente la mossa prima di eseguirla
 * @note Controlla condizioni di vittoria/pareggio dopo ogni mossa
 * @note Cambia automaticamente il turno se il gioco continua
 */
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

/**
 * Verifica se una mossa è valida nella posizione specificata.
 * Controlla che le coordinate siano nel range e che la cella sia vuota.
 * 
 * @param game Puntatore alla struttura Game (sola lettura)
 * @param row Riga da verificare (0-2)
 * @param col Colonna da verificare (0-2)
 * @return 1 se la mossa è valida, 0 altrimenti
 * 
 * @note Non modifica lo stato del gioco, solo validazione
 * @note Controlla i bounds della griglia 3x3
 */
int game_is_valid_move(const Game *game, int row, int col) {
    if (row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE) {
        return 0;
    }
    
    return (game->board[row][col] == PLAYER_NONE);
}

/**
 * Controlla se c'è un vincitore o un pareggio e aggiorna lo stato del gioco.
 * Verifica tutte le combinazioni possibili: righe, colonne e diagonali.
 * 
 * @param game Puntatore alla struttura Game da controllare
 * 
 * @note Controlla prima le righe, poi le colonne, infine le diagonali
 * @note Se non c'è vincitore ma il tabellone è pieno, dichiara pareggio
 * @note Aggiorna automaticamente lo stato del gioco a GAME_STATE_OVER se finita
 */
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

/**
 * Resetta completamente il gioco per iniziare una nuova partita.
 * Reinizializza tutti i valori e imposta lo stato a GAME_STATE_PLAYING.
 * 
 * @param game Puntatore alla struttura Game da resettare
 * 
 * @note Chiama game_init() per azzerare tutto
 * @note Imposta lo stato direttamente a GAME_STATE_PLAYING (partita attiva)
 * @note Utilizzata per iniziare una nuova partita o un rematch
 */
void game_reset(Game *game) {
    game_init(game);
    game->state = GAME_STATE_PLAYING;
}

/**
 * Stampa il tabellone di gioco in formato ASCII art nella console.
 * Visualizza una griglia 3x3 con separatori grafici tra le celle.
 * 
 * @param game Puntatore alla struttura Game da visualizzare (sola lettura)
 * 
 * @note Formato di output:
 *       X | O |   
 *       -----------
 *       O | X | X
 *       -----------
 *         | O |   
 * @note Le celle vuote vengono mostrate come spazi
 * @note Aggiunge righe vuote prima e dopo per migliorare la leggibilità
 */
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

/**
 * Verifica se il tabellone di gioco è completamente pieno.
 * Controlla tutte le celle per determinare se ci sono ancora mosse possibili.
 * 
 * @param game Puntatore alla struttura Game da controllare (sola lettura)
 * @return 1 se il tabellone è pieno (nessuna cella vuota), 0 altrimenti
 * 
 * @note Utilizzata per determinare condizioni di pareggio
 * @note Scorre tutte le 9 celle del tabellone 3x3
 * @note Ritorna 0 appena trova la prima cella vuota (ottimizzazione)
 */
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

/**
 * Elabora un messaggio ricevuto dalla rete e aggiorna lo stato del gioco.
 * Gestisce diversi tipi di messaggi: mosse, reset, rematch e fine partita.
 * 
 * @param game Puntatore alla struttura Game da aggiornare
 * @param message Messaggio ricevuto dal server da elaborare
 * @return 1 se il messaggio è stato elaborato con successo, 0 altrimenti
 * 
 * @note Tipi di messaggi supportati:
 *       - "MOVE:row,col:symbol" - Mossa dell'avversario
 *       - "GAME_RESET" o "RESET" - Reset della partita
 *       - "REMATCH" - Richiesta di rematch
 *       - "GAME_OVER:WINNER:X/O" - Fine partita con vincitore
 *       - "GAME_OVER:DRAW" - Fine partita con pareggio
 * @note Stampa messaggi di debug per tracciare l'elaborazione
 * @note Aggiorna automaticamente il turno dopo le mosse valide
 */
int game_process_network_message(Game *game, const char *message) {
    if (!game || !message) return 0;

    printf("Processando messaggio: %s\n", message);

    if (strncmp(message, "MOVE:", 5) == 0) {
        
        int row, col;
        char symbol;
        if (sscanf(message + 5, "%d,%d:%c", &row, &col, &symbol) == 3) {
            printf("Mossa estratta: riga=%d, col=%d, simbolo=%c\n", row, col, symbol);
            
            if (row >= 0 && row < BOARD_SIZE && col >= 0 && col < BOARD_SIZE) {
                
                game->board[row][col] = symbol;
                printf("Board aggiornato alla posizione [%d][%d] = %c\n", row, col, symbol);
                
                
                game_check_winner(game);
                
                
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