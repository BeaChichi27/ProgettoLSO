#include "headers/lobby.h"
#include "headers/network.h"
#include "headers/game_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Client *clients[MAX_CLIENTS];
static mutex_t lobby_mutex;

/**
 * Inizializza la lobby del server e le strutture dati necessarie.
 * Prepara l'array dei client e inizializza il mutex per la sincronizzazione thread-safe.
 * 
 * @return 1 in caso di successo, 0 in caso di errore
 */
int lobby_init() {
    if (mutex_init(&lobby_mutex) != 0) return 0;
    
    if (mutex_lock(&lobby_mutex) != 0) return 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i] = NULL;
    }
    mutex_unlock(&lobby_mutex);
    
    printf("Lobby inizializzata\n");
    return 1;
}

/**
 * Pulisce e chiude tutte le risorse della lobby prima dello shutdown del server.
 * Disconnette tutti i client attivi, invia notifiche di spegnimento e libera le risorse.
 * Include una pausa per permettere ai thread di terminare correttamente.
 */
void lobby_cleanup() {
    mutex_lock(&lobby_mutex);
    printf("Disconnettendo tutti i client prima dello shutdown...\n");
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i]->is_active) {
            // Invia un messaggio di disconnessione al client
            network_send_to_client(clients[i], "SERVER_SHUTDOWN:Server in spegnimento");
            
            // Chiudi la connessione
            clients[i]->is_active = 0;
            closesocket(clients[i]->client_fd);
            
            printf("Client %s disconnesso per shutdown\n", clients[i]->name);
        }
        
        if (clients[i]) {
            game_leave(clients[i]);
            // Non fare free qui - verrà fatto dal thread del client
            clients[i] = NULL;
        }
    }
    mutex_unlock(&lobby_mutex);
    
    // Aspetta un po' per permettere ai thread di terminare
#ifdef _WIN32
    Sleep(1000);
#else
    sleep(1);
#endif
    
    mutex_destroy(&lobby_mutex);
    
    printf("Lobby pulita\n");
}

/**
 * Controlla se la lobby ha raggiunto il numero massimo di client connessi.
 * Operazione thread-safe che verifica la disponibilità di slot liberi.
 * 
 * @return 1 se la lobby è piena, 0 se ci sono ancora slot disponibili
 */
int lobby_is_full() {
    mutex_lock(&lobby_mutex);
    int count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] != NULL) {
            count++;
        }
    }
    mutex_unlock(&lobby_mutex);
    return count >= MAX_CLIENTS;
}

/**
 * Trova il primo slot libero nell'array dei client.
 * NOTA: Questa funzione non è thread-safe, deve essere chiamata con il mutex acquisito.
 * 
 * @return Indice del primo slot libero, -1 se non ci sono slot disponibili
 */
static int lobby_find_free_slot() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == NULL) {
            return i;
        }
    }
    return -1;
}

/**
 * Aggiunge un nuovo client alla lobby con le informazioni fornite.
 * Alloca memoria per il client, inizializza i suoi dati e lo inserisce nel primo slot libero.
 * 
 * @param client_fd Socket file descriptor del client
 * @param name Nome del giocatore (massimo MAX_NAME_LEN-1 caratteri)
 * @return Puntatore al client creato in caso di successo, NULL in caso di errore
 */
Client* lobby_add_client(socket_t client_fd, const char *name) {
    mutex_lock(&lobby_mutex);
    int slot = lobby_find_free_slot();
    if (slot == -1) {
        mutex_unlock(&lobby_mutex);
        return NULL;
    }
    
    Client *client = (Client*)malloc(sizeof(Client));
    if (!client) {
        mutex_unlock(&lobby_mutex);
        return NULL;
    }
    
    memset(client, 0, sizeof(Client));
    client->client_fd = client_fd;
    strncpy(client->name, name, MAX_NAME_LEN - 1);
    client->name[MAX_NAME_LEN - 1] = '\0';
    client->is_active = 1;
    client->game_id = -1;
    
    clients[slot] = client;
    mutex_unlock(&lobby_mutex);
    
    printf("Client %s aggiunto alla lobby\n", name);
    return client;
}

/**
 * Rimuove il riferimento al client dall'array della lobby senza fare cleanup della memoria.
 * Utilizzata quando il client viene gestito da un thread separato che si occuperà della deallocazione.
 * 
 * @param client Puntatore al client da rimuovere dalla lobby
 */
void lobby_remove_client_reference(Client *client) {
    if (!client) return;
    
    mutex_lock(&lobby_mutex);
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == client) {
            clients[i] = NULL;
            printf("Client %s rimosso dalla lobby slot %d\n", client->name, i);
            break;
        }
    }
    
    mutex_unlock(&lobby_mutex);
    
    // Aggiorna la lista giochi per tutti i client rimanenti
    lobby_broadcast_game_list();
}

/**
 * Rimuove completamente un client dalla lobby e dalle partite.
 * Gestisce la disconnessione dalle partite in corso e rimuove il client dall'array.
 * 
 * @param client Puntatore al client da rimuovere
 */
void lobby_remove_client(Client *client) {
    if (!client) return;
    
    mutex_lock(&lobby_mutex);
    game_leave(client);
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == client) {
            clients[i] = NULL;
            break;
        }
    }
    mutex_unlock(&lobby_mutex);
    
    printf("Client %s rimosso dalla lobby\n", client->name);
}

/**
 * Cerca un client nella lobby utilizzando il file descriptor del socket.
 * Operazione thread-safe per trovare un client tramite la sua connessione.
 * 
 * @param fd File descriptor del socket del client da cercare
 * @return Puntatore al client se trovato, NULL altrimenti
 */
Client* lobby_find_client_by_fd(socket_t fd) {
    mutex_lock(&lobby_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i]->client_fd == fd) {
            Client *found = clients[i];
            mutex_unlock(&lobby_mutex);
            return found;
        }
    }
    mutex_unlock(&lobby_mutex);
    return NULL;
}

/**
 * Cerca un client nella lobby utilizzando il nome del giocatore.
 * Operazione thread-safe per trovare un client tramite il suo nome utente.
 * 
 * @param name Nome del giocatore da cercare
 * @return Puntatore al client se trovato, NULL altrimenti o se name è NULL
 */
Client* lobby_find_client_by_name(const char *name) {
    if (!name) return NULL;
    
    mutex_lock(&lobby_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && strcmp(clients[i]->name, name) == 0) {
            Client *found = clients[i];
            mutex_unlock(&lobby_mutex);
            return found;
        }
    }
    mutex_unlock(&lobby_mutex);
    return NULL;
}

/**
 * Invia un messaggio broadcast a tutti i client connessi alla lobby.
 * Permette di escludere un client specifico dall'invio (utile per non inviare messaggi al mittente).
 * 
 * @param message Messaggio da inviare a tutti i client
 * @param exclude Client da escludere dall'invio, NULL per inviare a tutti
 */
void lobby_broadcast_message(const char *message, Client *exclude) {
    if (!message) return;
    
    mutex_lock(&lobby_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i] != exclude && clients[i]->is_active) {
            network_send_to_client(clients[i], message);
        }
    }
    mutex_unlock(&lobby_mutex);
}

/**
 * Invia la lista aggiornata delle partite disponibili a tutti i client liberi.
 * Viene inviata solo ai client che non sono attualmente impegnati in una partita.
 * Operazione ottimizzata per evitare spam ai giocatori in partita.
 */
void lobby_broadcast_game_list() {
    char game_list[MAX_MSG_SIZE];
    game_list_available(game_list, sizeof(game_list));
    
    // Broadcast solo ai client che non sono in partita
    mutex_lock(&lobby_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i]->is_active && clients[i]->game_id <= 0) {
            network_send_to_client(clients[i], game_list);
        }
    }
    mutex_unlock(&lobby_mutex);
    
    printf("Lista giochi aggiornata e inviata a tutti i client liberi\n");
}

// Sostituisci la funzione lobby_handle_client_message in lobby.c

// Fix per lobby_handle_client_message - Sostituire completamente

/**
 * Gestisce ed elabora tutti i messaggi ricevuti dai client.
 * Router centrale per tutti i comandi del protocollo di comunicazione client-server.
 * Analizza il messaggio e chiama la funzione appropriata per gestire ogni comando.
 * 
 * @param client Client che ha inviato il messaggio
 * @param message Contenuto del messaggio ricevuto
 * 
 * @note Supporta i comandi: CREATE_GAME, LIST_GAMES, JOIN, MOVE, LEAVE, REMATCH, APPROVE, CANCEL, PING
 * @note Include logging dettagliato per debugging e tracciamento delle operazioni
 */
void lobby_handle_client_message(Client *client, const char *message) {
    if (!client || !message) return;
    
    printf("Elaborando messaggio: %s da %s\n", message, client->name);
    
    if (strncmp(message, "CREATE_GAME", 11) == 0) {
        // Controlla se già in partita ATTIVA prima di creare
        if (client->game_id > 0) {
            Game *current_game = game_find_by_id(client->game_id);
            if (current_game && current_game->state != GAME_STATE_OVER) {
                network_send_to_client(client, "ERROR:Sei già in una partita");
                return;
            }
            // Se la partita è terminata, resetta il game_id del client
            if (current_game && current_game->state == GAME_STATE_OVER) {
                client->game_id = -1;
            }
        }
        
        int game_id = game_create_new(client);
        if (game_id > 0) {
            char response[64];
            sprintf(response, "GAME_CREATED:%d", game_id);
            network_send_to_client(client, response);
            network_send_to_client(client, "WAITING_OPPONENT");
            printf("Client %s ha creato partita %d\n", client->name, game_id);
            
            // Broadcast della nuova partita disponibile
            lobby_broadcast_game_list();
        } else {
            network_send_to_client(client, "ERROR:Impossibile creare partita");
        }
    } 
    else if (strncmp(message, "LIST_GAMES", 10) == 0) {
        char response[MAX_MSG_SIZE];
        game_list_available(response, sizeof(response));
        network_send_to_client(client, response);
    } 
    else if (strncmp(message, "JOIN:", 5) == 0) {
        int game_id = atoi(message + 5);
        printf("Client %s tenta di unirsi alla partita %d\n", client->name, game_id);
        
        // FIX: La logica è già gestita in game_join
        if (!game_join(client, game_id)) {
            // game_join già invia il messaggio di errore appropriato
            printf("Join fallito per %s -> partita %d\n", client->name, game_id);
        }
    } 
    else if (strncmp(message, "MOVE:", 5) == 0) {
        int row, col;
        if (sscanf(message + 5, "%d,%d", &row, &col) == 2) {
            if (!game_make_move(client->game_id, client, row, col)) {
                network_send_to_client(client, "ERROR:Mossa non valida");
            }
        } else {
            network_send_to_client(client, "ERROR:Formato mossa non valido");
        }
    } 
    else if (strncmp(message, "LEAVE", 5) == 0) {
        game_leave(client);
        network_send_to_client(client, "LEFT_GAME");
    } 
    else if (strncmp(message, "REMATCH_DECLINE", 15) == 0) {
        if (!game_decline_rematch(client)) {
            printf("Nessun rematch per %s\n", client->name);
        }
    }
    else if (strncmp(message, "REMATCH", 7) == 0) {
        if (!game_request_rematch(client)) {
            // game_request_rematch già invia il messaggio di errore appropriato
            printf("Rematch fallito per %s\n", client->name);
        }
    }
    else if (strncmp(message, "APPROVE:", 8) == 0) {
        int approve = atoi(message + 8); // 1 per approvare, 0 per rifiutare
        if (!game_approve_join(client, approve)) {
            printf("Approvazione fallita per %s\n", client->name);
        }
    } 
    else if (strncmp(message, "CANCEL", 6) == 0) {
        if (client->game_id > 0) {
            game_leave(client);
            network_send_to_client(client, "GAME_CANCELED");
        } else {
            network_send_to_client(client, "ERROR:Non sei in una partita");
        }
    } 
    else if (strncmp(message, "PING", 4) == 0) {
        // Handle PING with PONG response (usando strncmp per gestire eventuali caratteri extra)
        network_send_to_client(client, "PONG");
    }
    else {
        printf("Comando sconosciuto da %s: %s\n", client->name, message);
        network_send_to_client(client, "ERROR:Comando sconosciuto");
    }
}

/**
 * Restituisce un puntatore al mutex globale della lobby.
 * Utilizzata da altre funzioni che necessitano di sincronizzazione con la lobby.
 * 
 * @return Puntatore al mutex della lobby
 */
mutex_t* lobby_get_mutex() {
    return &lobby_mutex;
}

/**
 * Accede a un client nell'array tramite indice.
 * Funzione di utilità per accesso diretto agli slot della lobby.
 * 
 * @param index Indice dello slot da verificare (0 a MAX_CLIENTS-1)
 * @return Puntatore al client nello slot specificato, NULL se indice non valido o slot vuoto
 */
Client* lobby_get_client_by_index(int index) {
    if (index < 0 || index >= MAX_CLIENTS) return NULL;
    return clients[index]; 
}

/**
 * Aggiunge un riferimento a un client esistente nell'array della lobby.
 * Utilizzata quando il client è già stato allocato altrove e si vuole solo aggiungere
 * il riferimento alla lobby senza duplicare la creazione.
 * 
 * @param client Puntatore al client da aggiungere alla lobby
 * @return 1 in caso di successo, 0 se la lobby è piena o client è NULL
 */
int lobby_add_client_reference(Client *client) {
    if (!client) return 0;
    
    mutex_t* mutex = lobby_get_mutex();
    mutex_lock(mutex);
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == NULL) {
            clients[i] = client;
            mutex_unlock(mutex);
            printf("Client FD:%d aggiunto alla lobby nello slot %d\n", (int)client->client_fd, i);
            return 1;
        }
    }
    
    mutex_unlock(mutex);
    printf("ERRORE: Lobby piena, impossibile aggiungere client FD:%d\n", (int)client->client_fd);
    return 0; 
}

/**
 * Gestisce specificamente i messaggi di approvazione join.
 * Wrapper per la funzione game_approve_join con gestione degli errori.
 * 
 * @param client Client creatore che deve approvare il join
 * @param message Messaggio contenente la decisione (1 per approvare, 0 per rifiutare)
 */
void lobby_handle_approve_join(Client *client, const char *message) {
    if (!client || !message) return;
    
    int approve = atoi(message);
    if (!game_approve_join(client, approve)) {
        printf("Approvazione fallita per %s\n", client->name);
    }
}