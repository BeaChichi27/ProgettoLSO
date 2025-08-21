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

int mutex_init(mutex_t *mutex) {
    #ifdef _WIN32
        return InitializeCriticalSection(mutex); // Corretto: Inizializza l'intera struttura
    #else
        return pthread_mutex_init(mutex, NULL); // Corretto: Inizializza l'intera struttura
    #endif
}

int mutex_lock(mutex_t *mutex) {
    #ifdef _WIN32
        EnterCriticalSection(mutex); // Corretto: Blocca l'intera struttura
        return 0;
    #else
        return pthread_mutex_lock(mutex); // Corretto: Blocca l'intera struttura
    #endif
}

int mutex_unlock(mutex_t *mutex) {
    #ifdef _WIN32
        LeaveCriticalSection(mutex); // Corretto: Sblocca l'intera struttura
        return 0;
    #else
        return pthread_mutex_unlock(mutex); // Corretto: Sblocca l'intera struttura
    #endif
}

int mutex_destroy(mutex_t *mutex) {
    #ifdef _WIN32
        DeleteCriticalSection(mutex); // Corretto: Distrugge l'intera struttura
        return 0;
    #else
        return pthread_mutex_destroy(mutex); // Corretto: Distrugge l'intera struttura
    #endif
}

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

void game_manager_cleanup() {
    if (mutex_lock(&games_mutex) != 0) return;
    
    for (int i = 0; i < MAX_GAMES; i++) {
        mutex_destroy(&games[i].mutex);
    }
    
    mutex_unlock(&games_mutex);
    mutex_destroy(&games_mutex);
    
    printf("Game Manager pulito\n");
}

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

static Game* game_find_free_slot() {
    for (int i = 0; i < MAX_GAMES; i++) {
        if (games[i].game_id == -1) {
            return &games[i];
        }
    }
    return NULL;
}

void game_init_board(Game *game) {
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            game->board[i][j] = PLAYER_NONE;
        }
    }
}

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
    game->creation_time = time(NULL);  // AGGIUNTO: era mancante
    game_init_board(game);
    
    creator->game_id = game->game_id;
    creator->symbol = 'X';
    
    mutex_unlock(&games_mutex);  // CAMBIATO: era LeaveCriticalSection
    
    printf("Partita %d creata da %s\n", game->game_id, creator->name);
    return game->game_id;
}

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
        snprintf(msg, sizeof(msg), "OPPONENT_LEFT:%s ha abbandonato", client->name);
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

int game_is_valid_move(Game *game, int row, int col) {
    if (row < 0 || row > 2 || col < 0 || col > 2) {
        return 0;
    }
    return game->board[row][col] == PLAYER_NONE;
}

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
    
    // Controlla se entrambi hanno richiesto rematch
    if (game->rematch_requests == 3) { // 1 | 2 = 3
        // Entrambi vogliono giocare di nuovo
        game->rematch_requests = 0;
        game_init_board(game);
        game->current_player = PLAYER_X;
        game->state = GAME_STATE_PLAYING;
        game->winner = PLAYER_NONE;
        game->is_draw = 0;
        
        network_send_to_client(game->player1, "REMATCH_ACCEPTED:Nuova partita iniziata!");
        network_send_to_client(game->player2, "REMATCH_ACCEPTED:Nuova partita iniziata!");
        network_send_to_client(game->player1, "GAME_START:X");
        network_send_to_client(game->player2, "GAME_START:O");
        
        printf("Rematch accettato per partita %d\n", game->game_id);
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
    if (game->player1 == client) {
        opponent = game->player2;
    } else if (game->player2 == client) {
        opponent = game->player1;
    } else {
        mutex_unlock(&game->mutex);
        network_send_to_client(client, "ERROR:Non fai parte di questa partita");
        return 0;
    }
    
    // Cancella TUTTE le richieste di rematch in corso
    game->rematch_requests = 0;
    game->state = GAME_STATE_OVER;
    
    // Avvisa l'avversario che il rematch è stato rifiutato
    if (opponent) {
        network_send_to_client(opponent, "REMATCH_DECLINED:L'avversario ha rifiutato l'altra partita");
    }
    
    // Notifica che il rematch è stato rifiutato
    printf("Client %s ha rifiutato la rivincita per partita %d - cancellate tutte le richieste\n", client->name, game->game_id);
    
    mutex_unlock(&game->mutex);
    return 1;
}

void game_broadcast_to_all_clients(const char *message) {
    // Questa funzione sarà implementata in lobby.c
    lobby_broadcast_message(message, NULL);
}