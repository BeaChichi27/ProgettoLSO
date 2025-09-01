#define _GNU_SOURCE
#include "headers/game_manager.h"
#include "headers/network.h"
#include "headers/lobby.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static Game games[MAX_GAMES];
static mutex_t games_mutex;  // CAMBIATO: era CRITICAL_SECTION
static int next_game_id = 1;

/**
 * Inizializza un mutex per la sincronizzazione cross-platform.
 * 
 * @param mutex Puntatore al mutex da inizializzare
 * @return 0 in caso di successo, valore diverso in caso di errore
 */
int mutex_init(mutex_t *mutex) {
    #ifdef _WIN32
        return InitializeCriticalSection(mutex); // Corretto: Inizializza l'intera struttura
    #else
        return pthread_mutex_init(mutex, NULL); // Corretto: Inizializza l'intera struttura
    #endif
}

/**
 * Acquisisce il lock di un mutex per l'accesso esclusivo a una risorsa condivisa.
 * 
 * @param mutex Puntatore al mutex da bloccare
 * @return 0 in caso di successo, valore diverso in caso di errore
 */
int mutex_lock(mutex_t *mutex) {
    #ifdef _WIN32
        EnterCriticalSection(mutex); // Corretto: Blocca l'intera struttura
        return 0;
    #else
        return pthread_mutex_lock(mutex); // Corretto: Blocca l'intera struttura
    #endif
}

/**
 * Rilascia il lock di un mutex per consentire l'accesso ad altri thread.
 * 
 * @param mutex Puntatore al mutex da sbloccare
 * @return 0 in caso di successo, valore diverso in caso di errore
 */
int mutex_unlock(mutex_t *mutex) {
    #ifdef _WIN32
        LeaveCriticalSection(mutex); // Corretto: Sblocca l'intera struttura
        return 0;
    #else
        return pthread_mutex_unlock(mutex); // Corretto: Sblocca l'intera struttura
    #endif
}

/**
 * Distrugge un mutex e libera le risorse associate.
 * 
 * @param mutex Puntatore al mutex da distruggere
 * @return 0 in caso di successo, valore diverso in caso di errore
 */
int mutex_destroy(mutex_t *mutex) {
    #ifdef _WIN32
        DeleteCriticalSection(mutex); // Corretto: Distrugge l'intera struttura
        return 0;
    #else
        return pthread_mutex_destroy(mutex); // Corretto: Distrugge l'intera struttura
    #endif
}

/**
 * Inizializza il gestore delle partite, preparando tutte le strutture dati necessarie.
 * Inizializza l'array di partite, i mutex associati e imposta i valori di default.
 * 
 * @return 1 in caso di successo, 0 in caso di errore
 */
int game_manager_init() {
    if (mutex_init(&games_mutex) != 0) return 0;
    
    if (mutex_lock(&games_mutex) != 0) return 0;
    
    for (int i = 0; i < MAX_GAMES; i++) {
        memset(&games[i], 0, sizeof(Game));
        games[i].game_id = -1;
        games[i].state = GAME_STATE_WAITING;
        if (mutex_init(&games[i].mutex) != 0) {
            // Aggiungi cleanup per i mutex già inizializzati
            for (int j = 0; j < i; j++) {
                mutex_destroy(&games[j].mutex);
            }
            mutex_unlock(&games_mutex);
            mutex_destroy(&games_mutex);
            return 0;
        }
    }
    
    if (mutex_unlock(&games_mutex) != 0) return 0;
    
    printf("Game Manager inizializzato\n");
    return 1;
}

/**
 * Pulisce tutte le risorse del game manager e distrugge i mutex.
 * Deve essere chiamata prima della terminazione del server per evitare memory leak.
 */
void game_manager_cleanup() {
    if (mutex_lock(&games_mutex) != 0) return;
    
    for (int i = 0; i < MAX_GAMES; i++) {
        mutex_destroy(&games[i].mutex);
    }
    
    mutex_unlock(&games_mutex);
    mutex_destroy(&games_mutex);
    
    printf("Game Manager pulito\n");
}

/**
 * Cerca una partita nell'array delle partite utilizzando l'ID.
 * La ricerca è thread-safe grazie al mutex globale.
 * 
 * @param game_id ID della partita da cercare
 * @return Puntatore alla partita se trovata, NULL altrimenti
 */
Game* game_find_by_id(int game_id) {
    mutex_lock(&games_mutex);  // CAMBIATO: era EnterCriticalSection
    for (int i = 0; i < MAX_GAMES; i++) {
        if (games[i].game_id == game_id) {
            mutex_unlock(&games_mutex);  // CAMBIATO: era LeaveCriticalSection
            return &games[i];
        }
    }
    mutex_unlock(&games_mutex);  // CAMBIATO: era LeaveCriticalSection
    return NULL;
}

/**
 * Trova il primo slot libero nell'array delle partite.
 * NOTA: Questa funzione non è thread-safe, deve essere chiamata con il mutex acquisito.
 * 
 * @return Puntatore al primo slot libero, NULL se non ci sono slot disponibili
 */
static Game* game_find_free_slot() {
    for (int i = 0; i < MAX_GAMES; i++) {
        if (games[i].game_id == -1) {
            return &games[i];
        }
    }
    return NULL;
}

/**
 * Inizializza la griglia di gioco di una partita, impostando tutte le celle a PLAYER_NONE.
 * 
 * @param game Puntatore alla partita di cui inizializzare la griglia
 */
void game_init_board(Game *game) {
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            game->board[i][j] = PLAYER_NONE;
        }
    }
}

/**
 * Crea una nuova partita con il client specificato come creatore.
 * Il creatore diventa automaticamente il giocatore X e la partita entra in stato di attesa.
 * 
 * @param creator Client che crea la partita
 * @return ID della partita creata, -1 in caso di errore
 */
int game_create_new(Client *creator) {
    if (!creator) return -1;
    
    mutex_lock(&games_mutex);  // CAMBIATO: era EnterCriticalSection
    Game *game = game_find_free_slot();
    if (!game) {
        mutex_unlock(&games_mutex);  // CAMBIATO: era LeaveCriticalSection
        return -1;
    }
    
    game->game_id = next_game_id++;
    game->player1 = creator;
    game->player2 = NULL;
    game->pending_player = NULL;
    game->current_player = PLAYER_X;
    game->state = GAME_STATE_WAITING;
    game->winner = PLAYER_NONE;
    game->is_draw = 0;
    game->rematch_requests = 0;
    game->rematch_declined = 0;
    game->creation_time = time(NULL);  // AGGIUNTO: era mancante
    game_init_board(game);
    
    creator->game_id = game->game_id;
    creator->symbol = 'X';
    
    mutex_unlock(&games_mutex);  // CAMBIATO: era LeaveCriticalSection
    
    printf("Partita %d creata da %s\n", game->game_id, creator->name);
    return game->game_id;
}

/**
 * Gestisce la richiesta di un client di unirsi a una partita esistente.
 * Implementa un sistema di approvazione dove il creatore deve accettare il join.
 * 
 * @param client Client che vuole unirsi alla partita
 * @param game_id ID della partita a cui unirsi
 * @return 1 in caso di successo, 0 in caso di errore
 */
int game_join(Client *client, int game_id) {
    if (!client) return 0;

    // Controllo migliorato per client già in partita
    if (client->game_id > 0) {
        char error_msg[50];
        Game *current_game = game_find_by_id(client->game_id);
        
        if (current_game && current_game->game_id == game_id) {
            snprintf(error_msg, sizeof(error_msg), "ERROR:Sei già in questa partita (ID: %d)", game_id);
        } else {
            snprintf(error_msg, sizeof(error_msg), "ERROR:Sei già in un'altra partita (ID: %d)", client->game_id);
        }
        
        network_send_to_client(client, error_msg);
        return 0;
    }

    Game *game = game_find_by_id(game_id);
    if (!game) {
        network_send_to_client(client, "ERROR:Partita non trovata");
        return 0;
    }

    mutex_lock(&game->mutex);

    // Controllo migliorato per join alla propria partita
    if (game->player1 == client) {
        network_send_to_client(client, "ERROR:Non puoi unirti alla tua partita");
        mutex_unlock(&game->mutex);
        return 0;
    }

    if (game->state != GAME_STATE_WAITING || !game->player1 || game->player2) {
        network_send_to_client(client, "ERROR:Partita non disponibile");
        mutex_unlock(&game->mutex);
        return 0;
    }

    // Sistema di approvazione - il join non è più automatico
    if (game->pending_player) {
        network_send_to_client(client, "ERROR:C'è già un giocatore in attesa di approvazione");
        mutex_unlock(&game->mutex);
        return 0;
    }

    // Metti il giocatore in attesa di approvazione
    game->pending_player = client;
    game->state = GAME_STATE_PENDING_APPROVAL;
    client->game_id = game_id;  // Temporaneo, sarà confermato se approvato

    mutex_unlock(&game->mutex);

    // Notifica al creatore della richiesta di join
    char join_request[128];
    snprintf(join_request, sizeof(join_request), "JOIN_REQUEST:%s:%d", client->name, game_id);
    network_send_to_client(game->player1, join_request);
    
    // Notifica al richiedente che è in attesa
    network_send_to_client(client, "JOIN_PENDING:In attesa di approvazione dal creatore");

    printf("Client %s richiede di unirsi alla partita %d, in attesa di approvazione\n", client->name, game_id);
    return 1;
}

/**
 * Gestisce l'approvazione o il rifiuto di una richiesta di join da parte del creatore.
 * Se approvata, il giocatore diventa player2 e la partita inizia.
 * 
 * @param creator Client creatore della partita che deve approvare
 * @param approve 1 per approvare, 0 per rifiutare
 * @return 1 in caso di successo, 0 in caso di errore
 */
int game_approve_join(Client *creator, int approve) {
    if (!creator || creator->game_id <= 0) return 0;
    
    Game *game = game_find_by_id(creator->game_id);
    if (!game || game->player1 != creator) {
        network_send_to_client(creator, "ERROR:Non sei il creatore di questa partita");
        return 0;
    }
    
    mutex_lock(&game->mutex);
    
    if (game->state != GAME_STATE_PENDING_APPROVAL || !game->pending_player) {
        network_send_to_client(creator, "ERROR:Nessuna richiesta di join in attesa");
        mutex_unlock(&game->mutex);
        return 0;
    }
    
    Client *pending = game->pending_player;
    
    if (approve) {
        // Approva il join
        game->player2 = pending;
        game->pending_player = NULL;
        game->state = GAME_STATE_PLAYING;
        pending->symbol = 'O';
        
        printf("Join approvato: %s si unisce alla partita %d\n", pending->name, game->game_id);
        
        // Notifica approvazione
        network_send_to_client(pending, "JOIN_APPROVED:O");
        network_send_to_client(creator, "JOIN_APPROVED_BY_YOU");
        
        // Breve pausa per garantire l'ordine dei messaggi
#ifdef _WIN32
        Sleep(50);
#else
        usleep(50000);
#endif
        
        // Avvia la partita
        network_send_to_client(game->player1, "GAME_START:X");
        network_send_to_client(game->player2, "GAME_START:O");
        
        // Aggiorna la lista giochi (rimuove la partita dalle disponibili)
        char list_update[64];
        snprintf(list_update, sizeof(list_update), "LIST_UPDATE:%d:REMOVE", game->game_id);
        lobby_broadcast_message(list_update, NULL);
        
    } else {
        // Rifiuta il join
        game->pending_player = NULL;
        game->state = GAME_STATE_WAITING;
        pending->game_id = -1;
        
        printf("Join rifiutato: %s non può unirsi alla partita %d\n", pending->name, game->game_id);
        
        // Notifica rifiuto
        network_send_to_client(pending, "JOIN_REJECTED:Richiesta rifiutata dal creatore");
        network_send_to_client(creator, "JOIN_REJECTED_BY_YOU");
        
        // Aggiorna la lista giochi (la partita rimane disponibile)
        lobby_broadcast_game_list();
    }
    
    mutex_unlock(&game->mutex);
    return 1;
}


/**
 * Gestisce l'uscita di un client da una partita.
 * Notifica l'avversario, pulisce la partita e aggiorna la lista giochi.
 * 
 * @param client Client che vuole lasciare la partita
 */
void game_leave(Client *client) {
    if (!client || client->game_id <= 0) return;
    Game *game = game_find_by_id(client->game_id);
    if (!game) return;
    
    mutex_lock(&game->mutex);
    
    Client *opponent = NULL;
    
    // Gestione diversa in base al ruolo del client
    if (game->player1 == client) {
        opponent = game->player2;
        game->player1 = NULL;
    } else if (game->player2 == client) {
        opponent = game->player1;
        game->player2 = NULL;
    } else if (game->pending_player == client) {
        // Il giocatore in attesa di approvazione lascia
        game->pending_player = NULL;
        game->state = GAME_STATE_WAITING;
        client->game_id = -1;
        
        // Notifica al creatore
        if (game->player1) {
            char msg[100];
            snprintf(msg, sizeof(msg), "JOIN_CANCELLED:%s ha annullato la richiesta", client->name);
            network_send_to_client(game->player1, msg);
        }
        
        mutex_unlock(&game->mutex);
        printf("Client %s ha annullato la richiesta di join per partita %d\n", client->name, client->game_id);
        return;
    }
    
    if (opponent) {
        char msg[100];
        
        // Gestione speciale per disconnessione durante rematch
        if (game->state == GAME_STATE_REMATCH_REQUESTED && game->rematch_requests > 0) {
            snprintf(msg, sizeof(msg), "REMATCH_CANCELLED:%s si è disconnesso", client->name);
            printf("Client %s si è disconnesso durante richiesta rematch, notificando avversario\n", client->name);
        } else {
            snprintf(msg, sizeof(msg), "OPPONENT_LEFT:%s ha abbandonato", client->name);
        }
        
        network_send_to_client(opponent, msg);
        opponent->game_id = -1;
    }
    
    // Pulisci anche eventuali pending players
    if (game->pending_player) {
        network_send_to_client(game->pending_player, "GAME_CANCELLED:La partita è stata cancellata");
        game->pending_player->game_id = -1;
        game->pending_player = NULL;
    }
    
    game->game_id = -1;
    game->state = GAME_STATE_WAITING;
    game->rematch_requests = 0;
    printf("Partita %d cancellata\n", client->game_id);
    
    client->game_id = -1;
    mutex_unlock(&game->mutex);
    
    // Aggiorna la lista giochi
    lobby_broadcast_game_list();
}

/**
 * Controlla se c'è un vincitore nella partita.
 * Verifica righe, colonne e diagonali per determinare se qualcuno ha vinto.
 * 
 * @param game Partita da controllare
 * @return Simbolo del vincitore (PLAYER_X o PLAYER_O), PLAYER_NONE se nessun vincitore
 */
int game_check_winner(Game *game) {
    // Controllo righe
    for (int i = 0; i < 3; i++) {
        if (game->board[i][0] != PLAYER_NONE &&
            game->board[i][0] == game->board[i][1] && 
            game->board[i][1] == game->board[i][2]) {
            return game->board[i][0];
        }
    }
    
    // Controllo colonne
    for (int j = 0; j < 3; j++) {
        if (game->board[0][j] != PLAYER_NONE &&
            game->board[0][j] == game->board[1][j] && 
            game->board[1][j] == game->board[2][j]) {
            return game->board[0][j];
        }
    }
    
    // Controllo diagonale principale
    if (game->board[0][0] != PLAYER_NONE &&
        game->board[0][0] == game->board[1][1] && 
        game->board[1][1] == game->board[2][2]) {
        return game->board[0][0];
    }
    
    // Controllo diagonale secondaria
    if (game->board[0][2] != PLAYER_NONE &&
        game->board[0][2] == game->board[1][1] && 
        game->board[1][1] == game->board[2][0]) {
        return game->board[0][2];
    }
    
    return PLAYER_NONE;
}

/**
 * Controlla se la griglia di gioco è completamente piena.
 * 
 * @param game Partita da controllare
 * @return 1 se la griglia è piena, 0 altrimenti
 */
int game_is_board_full(Game *game) {
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            if (game->board[i][j] == PLAYER_NONE) {
                return 0;
            }
        }
    }
    return 1;
}

/**
 * Verifica se una mossa è valida per la posizione specificata.
 * Controlla che le coordinate siano dentro i limiti e che la cella sia libera.
 * 
 * @param game Partita di riferimento
 * @param row Riga della mossa (0-2)
 * @param col Colonna della mossa (0-2)
 * @return 1 se la mossa è valida, 0 altrimenti
 */
int game_is_valid_move(Game *game, int row, int col) {
    if (row < 0 || row > 2 || col < 0 || col > 2) {
        return 0;
    }
    return game->board[row][col] == PLAYER_NONE;
}

/**
 * Esegue una mossa in una partita se valida.
 * Controlla il turno, la validità della mossa, aggiorna la griglia e verifica vittoria/pareggio.
 * 
 * @param game_id ID della partita
 * @param client Client che effettua la mossa
 * @param row Riga della mossa (0-2)
 * @param col Colonna della mossa (0-2)
 * @return 1 se la mossa è stata eseguita con successo, 0 altrimenti
 */
int game_make_move(int game_id, Client *client, int row, int col) {
    Game *game = game_find_by_id(game_id);
    if (!game || !client) return 0;
    
    mutex_lock(&game->mutex);  // CAMBIATO: era EnterCriticalSection
    
    PlayerSymbol expected_player = (game->current_player == PLAYER_X) ? PLAYER_X : PLAYER_O;
    if ((char)client->symbol != (char)expected_player) {
        network_send_to_client(client, "ERROR:Non e' il tuo turno");
        mutex_unlock(&game->mutex);  // CAMBIATO: era LeaveCriticalSection
        return 0;
    }
    
    if (!game_is_valid_move(game, row, col)) {
        network_send_to_client(client, "ERROR:Mossa non valida");
        mutex_unlock(&game->mutex);  // CAMBIATO: era LeaveCriticalSection
        return 0;
    }
    
    game->board[row][col] = (char)client->symbol;
    
    PlayerSymbol winner = (PlayerSymbol)game_check_winner(game);
    if (winner != PLAYER_NONE) {
        game->state = GAME_STATE_OVER;
        game->winner = winner;
        char msg[100];
        sprintf(msg, "GAME_OVER:WINNER:%c", winner);
        network_send_to_client(game->player1, msg);
        network_send_to_client(game->player2, msg);
        printf("Partita %d terminata - Vincitore: %c\n", game_id, winner);
        
        // NON resettare game_id subito - servirà per il rematch
    }
    else if (game_is_board_full(game)) {
        game->state = GAME_STATE_OVER;
        game->is_draw = 1;
        network_send_to_client(game->player1, "GAME_OVER:DRAW");
        network_send_to_client(game->player2, "GAME_OVER:DRAW");
        printf("Partita %d terminata - Pareggio\n", game_id);
        
        // NON resettare game_id subito - servirà per il rematch
    }
    else {
        game->current_player = (game->current_player == PLAYER_X) ? PLAYER_O : PLAYER_X;
        char move_msg[100];
        sprintf(move_msg, "MOVE:%d,%d:%c", row, col, client->symbol);
        network_send_to_client(game->player1, move_msg);
        network_send_to_client(game->player2, move_msg);
    }
    
    mutex_unlock(&game->mutex);  // CAMBIATO: era LeaveCriticalSection
    return 1;
}

/**
 * Resetta una partita al suo stato iniziale per un nuovo round.
 * Pulisce la griglia, resetta il turno e notifica i giocatori.
 * 
 * @param game_id ID della partita da resettare
 */
void game_reset(int game_id) {
    Game *game = game_find_by_id(game_id);
    if (!game) return;
    
    mutex_lock(&game->mutex);  // CAMBIATO: era EnterCriticalSection
    game_init_board(game);
    game->current_player = PLAYER_X;
    game->state = GAME_STATE_PLAYING;
    game->winner = PLAYER_NONE;
    game->is_draw = 0;
    
    network_send_to_client(game->player1, "GAME_RESET");
    network_send_to_client(game->player2, "GAME_RESET");
    
    mutex_unlock(&game->mutex);  // CAMBIATO: era LeaveCriticalSection
    printf("Partita %d resettata\n", game_id);
}

/**
 * Genera una lista delle partite disponibili formattata per l'invio ai client.
 * Include partite in attesa, in corso e con richieste pendenti.
 * 
 * @param response Buffer dove scrivere la lista formattata
 * @param max_len Dimensione massima del buffer
 */
void game_list_available(char *response, size_t max_len) {
    strcpy(response, "GAMES:");
    
    mutex_lock(&games_mutex);
    int count = 0;
    
    for (int i = 0; i < MAX_GAMES; i++) {
        if (games[i].game_id > 0) {
            char game_info[150];
            
            if (games[i].state == GAME_STATE_WAITING && games[i].player1 && !games[i].player2) {
                // Partita disponibile per join
                snprintf(game_info, sizeof(game_info), "[%d]%s(In attesa) ", 
                        games[i].game_id, games[i].player1->name);
                count++;
            } else if (games[i].state == GAME_STATE_PENDING_APPROVAL && games[i].pending_player) {
                // Partita con richiesta in attesa
                snprintf(game_info, sizeof(game_info), "[%d]%s(Richiesta di %s in attesa) ", 
                        games[i].game_id, games[i].player1->name, games[i].pending_player->name);
            } else if (games[i].state == GAME_STATE_PLAYING) {
                // Partita in corso
                snprintf(game_info, sizeof(game_info), "[%d]%s vs %s(In corso) ", 
                        games[i].game_id, 
                        games[i].player1 ? games[i].player1->name : "N/A",
                        games[i].player2 ? games[i].player2->name : "N/A");
            } else if (games[i].state == GAME_STATE_REMATCH_REQUESTED) {
                // Partita con richiesta rematch
                snprintf(game_info, sizeof(game_info), "[%d]%s vs %s(Rematch richiesto) ", 
                        games[i].game_id,
                        games[i].player1 ? games[i].player1->name : "N/A",
                        games[i].player2 ? games[i].player2->name : "N/A");
            } else {
                continue; // Salta partite in altri stati
            }
            
            if (strlen(response) + strlen(game_info) < max_len - 1) {
                strcat(response, game_info);
            }
        }
    }
    mutex_unlock(&games_mutex);
    
    if (count == 0 && strlen(response) == 6) {
        strcat(response, "Nessuna partita disponibile per il join");
    }
}

/**
 * Controlla e rimuove le partite che sono in attesa da troppo tempo.
 * Cancella automaticamente le partite in stato WAITING dopo 5 minuti di inattività.
 */
void game_check_timeouts() {
    mutex_lock(&games_mutex);  // CAMBIATO: era EnterCriticalSection
    time_t now = time(NULL);
    
    for (int i = 0; i < MAX_GAMES; i++) {
        if (games[i].game_id != -1 && 
            games[i].state == GAME_STATE_WAITING &&
            difftime(now, games[i].creation_time) > 300) { 
            
            printf("Partita %d cancellata per timeout\n", games[i].game_id);
            if (games[i].player1) {
                network_send_to_client(games[i].player1, "ERROR:Timeout - Nessun avversario");
                games[i].player1->game_id = -1;
            }
            games[i].game_id = -1;
        }
    }
    mutex_unlock(&games_mutex);  // CAMBIATO: era LeaveCriticalSection
}

/**
 * Gestisce una richiesta di rematch da parte di un giocatore.
 * Se entrambi i giocatori richiedono il rematch, avvia automaticamente una nuova partita
 * alternando i simboli dei giocatori.
 * 
 * @param client Client che richiede il rematch
 * @return 1 in caso di successo, 0 in caso di errore
 */
int game_request_rematch(Client *client) {
    if (!client || client->game_id <= 0) {
        network_send_to_client(client, "ERROR:Non sei in una partita");
        return 0;
    }
    
    Game *game = game_find_by_id(client->game_id);
    if (!game || (game->state != GAME_STATE_OVER && game->state != GAME_STATE_REMATCH_REQUESTED)) {
        network_send_to_client(client, "ERROR:La partita non è terminata o non disponibile per rematch");
        return 0;
    }
    
    mutex_lock(&game->mutex);
    
    // Controlla se qualcuno ha già rifiutato il rematch
    if (game->rematch_declined != 0) {
        mutex_unlock(&game->mutex);
        network_send_to_client(client, "ERROR:Il rematch è stato rifiutato, non è possibile richiederne un altro");
        printf("Richiesta rematch rifiutata per partita %d - qualcuno ha già declinato\n", game->game_id);
        return 0;
    }
    
    // Determina quale giocatore ha fatto la richiesta
    int player_bit = 0;
    Client *opponent = NULL;
    
    if (game->player1 == client) {
        player_bit = 1;
        opponent = game->player2;
    } else if (game->player2 == client) {
        player_bit = 2;
        opponent = game->player1;
    } else {
        mutex_unlock(&game->mutex);
        network_send_to_client(client, "ERROR:Non fai parte di questa partita");
        return 0;
    }
    
    // Controlla se ha già richiesto rematch
    if (game->rematch_requests & player_bit) {
        mutex_unlock(&game->mutex);
        network_send_to_client(client, "ERROR:Hai già richiesto un rematch");
        return 0;
    }
    
    // Segna la richiesta
    game->rematch_requests |= player_bit;
    game->state = GAME_STATE_REMATCH_REQUESTED;
    
    // Traccia chi ha richiesto per primo il rematch (avrà sempre X)
    if (game->rematch_requester == NULL) {
        game->rematch_requester = client;
        printf("Client %s è il primo a richiedere rematch (avrà X)\n", client->name);
    }
    
    // Controlla se entrambi hanno richiesto rematch
    if (game->rematch_requests == 3) { // 1 | 2 = 3
        // Entrambi vogliono giocare di nuovo
        game->rematch_requests = 0;
        game->rematch_declined = 0;  // Resetta anche i decline per la nuova partita
        game_init_board(game);
        game->current_player = PLAYER_X;
        game->state = GAME_STATE_PLAYING;
        game->winner = PLAYER_NONE;
        game->is_draw = 0;
        
        // NUOVA LOGICA: Alterna automaticamente i simboli
        // Se player1 era X nella partita precedente, ora diventa O (e viceversa)
        printf("[DEBUG REMATCH] Prima dell'alternanza - player1=%s, player2=%s\n", 
               game->player1->name, game->player2->name);
        
        // Scambia sempre i ruoli per alternare
        Client* temp = game->player1;
        game->player1 = game->player2;  // Ex player2 diventa player1 (X)
        game->player2 = temp;           // Ex player1 diventa player2 (O)
        
        printf("[DEBUG REMATCH] Dopo l'alternanza - player1=%s(X), player2=%s(O)\n", 
               game->player1->name, game->player2->name);
        
        network_send_to_client(game->player1, "REMATCH_ACCEPTED:Nuova partita iniziata!GAME_START:X");
        network_send_to_client(game->player2, "REMATCH_ACCEPTED:Nuova partita iniziata!GAME_START:O");
        
        printf("Rematch accettato per partita %d - %s(X) vs %s(O)\n", 
               game->game_id, game->player1->name, game->player2->name);
        
        // Reset del richiedente per il prossimo possibile rematch
        game->rematch_requester = NULL;
    } else {
        // Solo uno ha richiesto, notifica l'altro
        char msg[128];
        snprintf(msg, sizeof(msg), "REMATCH_REQUEST:%s vuole giocare di nuovo", client->name);
        network_send_to_client(opponent, msg);
        network_send_to_client(client, "REMATCH_SENT:Richiesta inviata, in attesa dell'avversario");
        
        printf("Client %s ha richiesto rematch per partita %d\n", client->name, game->game_id);
    }
    
    mutex_unlock(&game->mutex);
    return 1;
}

/**
 * Annulla una richiesta di rematch in corso.
 * Resetta lo stato della partita e notifica l'avversario della cancellazione.
 * 
 * @param client Client che vuole annullare il rematch
 * @return 1 in caso di successo, 0 in caso di errore
 */
int game_cancel_rematch(Client *client) {
    if (!client || client->game_id <= 0) {
        network_send_to_client(client, "ERROR:Non sei in una partita");
        return 0;
    }
    
    Game *game = game_find_by_id(client->game_id);
    if (!game) {
        network_send_to_client(client, "ERROR:Partita non trovata");
        return 0;
    }
    
    mutex_lock(&game->mutex);
    
    // Ottiene l'avversario
    Client *opponent = NULL;
    if (game->player1 == client) {
        opponent = game->player2;
    } else if (game->player2 == client) {
        opponent = game->player1;
    } else {
        mutex_unlock(&game->mutex);
        network_send_to_client(client, "ERROR:Non fai parte di questa partita");
        return 0;
    }
    
    // Avvisa l'avversario che la rivincita è stata annullata
    if (opponent) {
        char msg[128];
        snprintf(msg, sizeof(msg), "REMATCH_CANCELLED:%s ha deciso di fare una nuova partita", client->name);
        network_send_to_client(opponent, msg);
    }
    
    // Reset della partita - pulizia completa
    game->rematch_requests = 0;
    game->state = GAME_STATE_OVER;  // Torna allo stato di terminata
    
    // Notifica che il rematch è stato cancellato
    printf("Client %s ha cancellato la rivincita per partita %d\n", client->name, game->game_id);
    
    mutex_unlock(&game->mutex);
    return 1;
}

/**
 * Rifiuta esplicitamente una richiesta di rematch.
 * Cancella tutte le richieste di rematch pendenti e impedisce future richieste.
 * 
 * @param client Client che rifiuta il rematch
 * @return 1 in caso di successo, 0 in caso di errore
 */
int game_decline_rematch(Client *client) {
    if (!client || client->game_id <= 0) {
        network_send_to_client(client, "ERROR:Non sei in una partita");
        return 0;
    }
    
    Game *game = game_find_by_id(client->game_id);
    if (!game) {
        network_send_to_client(client, "ERROR:Partita non trovata");
        return 0;
    }
    
    mutex_lock(&game->mutex);
    
    // Ottiene l'avversario
    Client *opponent = NULL;
    int player_bit = 0;
    if (game->player1 == client) {
        opponent = game->player2;
        player_bit = 1;
    } else if (game->player2 == client) {
        opponent = game->player1;
        player_bit = 2;
    } else {
        mutex_unlock(&game->mutex);
        network_send_to_client(client, "ERROR:Non fai parte di questa partita");
        return 0;
    }
    
    // Cancella TUTTE le richieste di rematch in corso
    game->rematch_requests = 0;
    // Segna che questo giocatore ha rifiutato il rematch
    game->rematch_declined |= player_bit;
    game->state = GAME_STATE_OVER;
    
    // Reset del richiedente dato che il rematch è stato rifiutato
    game->rematch_requester = NULL;
    
    // Avvisa l'avversario che il rematch è stato rifiutato
    if (opponent) {
        network_send_to_client(opponent, "REMATCH_DECLINED:L'avversario ha rifiutato l'altra partita");
    }
    
    // Conferma al giocatore che ha rifiutato che la sua azione è stata registrata
    network_send_to_client(client, "REMATCH_DECLINE_CONFIRMED:Hai rifiutato il rematch. La partita è terminata.");
    
    // Notifica che il rematch è stato rifiutato
    printf("Client %s ha rifiutato la rivincita per partita %d - cancellate tutte le richieste\n", client->name, game->game_id);
    
    mutex_unlock(&game->mutex);
    return 1;
}

/**
 * Invia un messaggio broadcast a tutti i client connessi tramite il sistema lobby.
 * Funzione di utilità per comunicazioni globali.
 * 
 * @param message Messaggio da inviare a tutti i client
 */
void game_broadcast_to_all_clients(const char *message) {
    lobby_broadcast_message(message, NULL);
}