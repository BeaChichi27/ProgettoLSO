#include "headers/lobby.h"
#include "headers/network.h"
#include "headers/game_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Client *clients[MAX_CLIENTS];
static mutex_t lobby_mutex;  // CAMBIATO: era CRITICAL_SECTION

int lobby_init() {
    if (mutex_init(&lobby_mutex) != 0) return 0;  // CAMBIATO: era InitializeCriticalSection
    
    if (mutex_lock(&lobby_mutex) != 0) return 0;  // CAMBIATO: era EnterCriticalSection
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i] = NULL;
    }
    mutex_unlock(&lobby_mutex);  // CAMBIATO: era LeaveCriticalSection
    
    printf("Lobby inizializzata\n");
    return 1;
}

void lobby_cleanup() {
    mutex_lock(&lobby_mutex);  // CAMBIATO: era EnterCriticalSection
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i]) {
            game_leave(clients[i]);
            clients[i] = NULL;
        }
    }
    mutex_unlock(&lobby_mutex);  // CAMBIATO: era LeaveCriticalSection
    mutex_destroy(&lobby_mutex);  // CAMBIATO: era DeleteCriticalSection
    
    printf("Lobby pulita\n");
}

static int lobby_find_free_slot() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == NULL) {
            return i;
        }
    }
    return -1;
}

Client* lobby_add_client(socket_t client_fd, const char *name) {  // CAMBIATO: era SOCKET
    mutex_lock(&lobby_mutex);  // CAMBIATO: era EnterCriticalSection
    int slot = lobby_find_free_slot();
    if (slot == -1) {
        mutex_unlock(&lobby_mutex);  // CAMBIATO: era LeaveCriticalSection
        return NULL;
    }
    
    Client *client = (Client*)malloc(sizeof(Client));
    if (!client) {
        mutex_unlock(&lobby_mutex);  // CAMBIATO: era LeaveCriticalSection
        return NULL;
    }
    
    memset(client, 0, sizeof(Client));
    client->client_fd = client_fd;
    strncpy(client->name, name, MAX_NAME_LEN - 1);
    client->name[MAX_NAME_LEN - 1] = '\0';
    client->is_active = 1;
    client->game_id = -1;
    
    clients[slot] = client;
    mutex_unlock(&lobby_mutex);  // CAMBIATO: era LeaveCriticalSection
    
    printf("Client %s aggiunto alla lobby\n", name);
    return client;
}

void lobby_remove_client(Client *client) {
    if (!client) return;
    
    mutex_lock(&lobby_mutex);  // CAMBIATO: era EnterCriticalSection
    game_leave(client);
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == client) {
            clients[i] = NULL;
            break;
        }
    }
    mutex_unlock(&lobby_mutex);  // CAMBIATO: era LeaveCriticalSection
    
    printf("Client %s rimosso dalla lobby\n", client->name);
}

Client* lobby_find_client_by_fd(socket_t fd) {  // CAMBIATO: era SOCKET
    mutex_lock(&lobby_mutex);  // CAMBIATO: era EnterCriticalSection
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i]->client_fd == fd) {
            Client *found = clients[i];
            mutex_unlock(&lobby_mutex);  // CAMBIATO: era LeaveCriticalSection
            return found;
        }
    }
    mutex_unlock(&lobby_mutex);  // CAMBIATO: era LeaveCriticalSection
    return NULL;
}

Client* lobby_find_client_by_name(const char *name) {
    if (!name) return NULL;
    
    mutex_lock(&lobby_mutex);  // CAMBIATO: era EnterCriticalSection
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && strcmp(clients[i]->name, name) == 0) {
            Client *found = clients[i];
            mutex_unlock(&lobby_mutex);  // CAMBIATO: era LeaveCriticalSection
            return found;
        }
    }
    mutex_unlock(&lobby_mutex);  // CAMBIATO: era LeaveCriticalSection
    return NULL;
}

void lobby_broadcast_message(const char *message, Client *exclude) {
    if (!message) return;
    
    mutex_lock(&lobby_mutex);  // CAMBIATO: era EnterCriticalSection
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i] != exclude && clients[i]->is_active) {
            network_send_to_client(clients[i], message);
        }
    }
    mutex_unlock(&lobby_mutex);  // CAMBIATO: era LeaveCriticalSection
}

void lobby_handle_client_message(Client *client, const char *message) {
    if (!client || !message) return;
    
    if (strncmp(message, "REGISTER:", 9) == 0) {
        const char *name = message + 9;
        
        if (lobby_find_client_by_name(name)) {
            network_send_to_client(client, "ERROR:Nome già in uso");
            return;
        }
        
        strncpy(client->name, name, MAX_NAME_LEN - 1);
        client->name[MAX_NAME_LEN - 1] = '\0';
        
        network_send_to_client(client, "OK:Registrazione completata");
        printf("Client registrato con nome: %s\n", name);
        return;
    }
    
    if (strncmp(message, "CREATE_GAME", 11) == 0) {
        int game_id = game_create_new(client);
        if (game_id > 0) {
            char response[64];
            sprintf(response, "GAME_CREATED:%d", game_id);
            network_send_to_client(client, response);
            network_send_to_client(client, "WAITING_OPPONENT");
        } else {
            network_send_to_client(client, "ERROR:Impossibile creare partita");
        }
    } else if (strncmp(message, "LIST_GAMES", 10) == 0) {
        char response[MAX_MSG_SIZE];
        game_list_available(response, sizeof(response));
        network_send_to_client(client, response);
    } else if (strncmp(message, "JOIN:", 5) == 0) {
        int game_id = atoi(message + 5);
        if (game_join(client, game_id)) {
            network_send_to_client(client, "JOIN_ACCEPTED");
        } else {
            network_send_to_client(client, "ERROR:Impossibile unirsi alla partita");
        }
    } else if (strncmp(message, "MOVE:", 5) == 0) {
        int row, col;
        if (sscanf(message + 5, "%d,%d", &row, &col) == 2) {
            if (!game_make_move(client->game_id, client, row, col)) {
                network_send_to_client(client, "ERROR:Mossa non valida");
            }
        } else {
            network_send_to_client(client, "ERROR:Formato mossa non valido");
        }
    } else if (strncmp(message, "LEAVE", 5) == 0) {
        game_leave(client);
        network_send_to_client(client, "LEFT_GAME");
    } else if (strncmp(message, "REMATCH", 7) == 0) {
        game_reset(client->game_id);
    } else if (strncmp(message, "CANCEL", 6) == 0) {
        if (client->game_id > 0) {
            game_leave(client);
            network_send_to_client(client, "GAME_CANCELED");
        }
    } else {
        network_send_to_client(client, "ERROR:Comando sconosciuto");
    }
}

mutex_t* lobby_get_mutex() {  // CAMBIATO: era CRITICAL_SECTION*
    return &lobby_mutex;
}

Client* lobby_get_client_by_index(int index) {
    if (index < 0 || index >= MAX_CLIENTS) return NULL;
    return clients[index]; 
}

int lobby_add_client_reference(Client *client) {
    if (!client) return 0;
    
    mutex_t* mutex = lobby_get_mutex();
    mutex_lock(mutex);  // CAMBIATO: era EnterCriticalSection
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == NULL) {
            clients[i] = client;
            mutex_unlock(mutex);  // CAMBIATO: era LeaveCriticalSection
            return 1;
        }
    }
    
    mutex_unlock(mutex);  // CAMBIATO: era LeaveCriticalSection
    return 0; 
}