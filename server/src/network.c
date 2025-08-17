#include "headers/network.h"
#include "headers/lobby.h"
#include "headers/game_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG_ERROR(msg) printf("ERROR: %s\n", msg)

const char* get_socket_error() {
#ifdef _WIN32
    static char buffer[256];
    int error = WSAGetLastError();
    snprintf(buffer, sizeof(buffer), "Socket error %d", error);
    return buffer;
#else
    return strerror(errno);
#endif
}

static ServerNetwork *global_server = NULL;

// Funzione per ottenere l'IP locale del server
void print_server_info() {
    printf("=== INFORMAZIONI SERVER ===\n");
    printf("Porta: %d\n", SERVER_PORT);
    
#ifdef _WIN32
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        printf("Hostname: %s\n", hostname);
        
        struct hostent *host_info = gethostbyname(hostname);
        if (host_info != NULL) {
            for (int i = 0; host_info->h_addr_list[i] != NULL; i++) {
                struct in_addr addr;
                memcpy(&addr, host_info->h_addr_list[i], sizeof(struct in_addr));
                printf("IP locale: %s\n", inet_ntoa(addr));
            }
        }
    }
#else
    // Per Linux, stampa le informazioni di rete
    system("hostname -I 2>/dev/null | head -n1 | tr ' ' '\\n' | grep -E '^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+$' | head -n1");
#endif
    printf("===========================\n\n");
}

static int network_register_udp_client(Client *client, struct sockaddr_in *udp_addr) {
    if (!client || !udp_addr) return 0;
    
    memcpy(&client->udp_addr, udp_addr, sizeof(struct sockaddr_in));
    
    printf("Client %s registrato per UDP da %s:%d\n", 
           client->name, 
           inet_ntoa(udp_addr->sin_addr), 
           ntohs(udp_addr->sin_port));
    return 1;
}

static Client* network_find_client_by_udp_addr(struct sockaddr_in *addr) {
    if (!addr) return NULL;
    
    mutex_t* mutex = lobby_get_mutex();
    mutex_lock(mutex);
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        Client *client = lobby_get_client_by_index(i); 
        if (client && 
            client->udp_addr.sin_addr.s_addr == addr->sin_addr.s_addr &&
            client->udp_addr.sin_port == addr->sin_port) {
            mutex_unlock(mutex);
            return client;
        }
    }
    
    mutex_unlock(mutex);
    return NULL;
}

static int network_send_udp_response(ServerNetwork *server, struct sockaddr_in *client_addr, const char *message) {
    if (!server || !client_addr || !message) return 0;
    
    int bytes_sent = sendto(server->udp_socket, message, (int)strlen(message), 0,
                           (struct sockaddr*)client_addr, sizeof(*client_addr));
    
    if (bytes_sent == SOCKET_ERROR_VALUE) {
#ifdef _WIN32
        printf("Errore invio UDP: %d\n", WSAGetLastError());
#else
        printf("Errore invio UDP: %s\n", strerror(errno));
#endif
        return 0;
    }
    
    printf("UDP inviato a %s:%d: %s\n", 
           inet_ntoa(client_addr->sin_addr), 
           ntohs(client_addr->sin_port), 
           message);
    return 1;
}

static void handle_udp_register(ServerNetwork *server, struct sockaddr_in *client_addr, const char *name) {
    Client *client = lobby_find_client_by_name(name);
    if (!client) {
        network_send_udp_response(server, client_addr, "ERROR:Client non trovato");
        return;
    }
    
    if (network_register_udp_client(client, client_addr)) {
        network_send_udp_response(server, client_addr, "UDP_REGISTERED:OK");
    } else {
        network_send_udp_response(server, client_addr, "ERROR:Registrazione fallita");
    }
}

static void handle_udp_move(ServerNetwork *server, struct sockaddr_in *client_addr, const char *move_data) {
    int row, col;
    if (sscanf(move_data, "%d,%d", &row, &col) != 2) {
        network_send_udp_response(server, client_addr, "ERROR:Formato mossa non valido");
        return;
    }
    
    Client *client = network_find_client_by_udp_addr(client_addr);
    if (!client) {
        network_send_udp_response(server, client_addr, "ERROR:Client non registrato per UDP");
        return;
    }
    
    if (client->game_id <= 0) {
        network_send_udp_response(server, client_addr, "ERROR:Non sei in una partita");
        return;
    }
    
    if (game_make_move(client->game_id, client, row, col)) {
        network_send_udp_response(server, client_addr, "MOVE_ACCEPTED");
    } else {
        network_send_udp_response(server, client_addr, "ERROR:Mossa non valida");
    }
}

static void handle_udp_ping(ServerNetwork *server, struct sockaddr_in *client_addr) {
    network_send_udp_response(server, client_addr, "PONG");
}

static void handle_udp_game_state(ServerNetwork *server, struct sockaddr_in *client_addr) {
    Client *client = network_find_client_by_udp_addr(client_addr);
    if (!client) {
        network_send_udp_response(server, client_addr, "ERROR:Client non registrato");
        return;
    }
    
    if (client->game_id <= 0) {
        network_send_udp_response(server, client_addr, "GAME_STATE:NO_GAME");
        return;
    }
    
    Game *game = game_find_by_id(client->game_id);
    if (!game) {
        network_send_udp_response(server, client_addr, "ERROR:Partita non trovata");
        return;
    }
    
    char state_msg[256];
    sprintf(state_msg, "GAME_STATE:%d:%c:%d", 
            game->game_id, 
            (char)game->current_player, 
            (int)game->state);
    
    network_send_udp_response(server, client_addr, state_msg);
}

#ifdef _WIN32
thread_return_t THREAD_CALL network_handle_udp_thread(thread_param_t arg) {
#else
thread_return_t network_handle_udp_thread(thread_param_t arg) {
#endif
    ServerNetwork *server = (ServerNetwork*)arg;
    char buffer[MAX_MSG_SIZE];
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    printf("Thread UDP avviato\n");
    
    while (server->is_running) {
        int bytes = recvfrom(server->udp_socket, buffer, sizeof(buffer) - 1, 0,
                           (struct sockaddr*)&client_addr, &addr_len);
        
        if (bytes <= 0) {
#ifdef _WIN32
            int error = WSAGetLastError();
            if (error == WSAETIMEDOUT) {
                continue;
            }
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
#endif
            if (server->is_running) {
                printf("Errore ricezione UDP: %s\n", get_socket_error());
            }
            continue;
        }

        buffer[bytes] = '\0';
        
        printf("UDP ricevuto: %s da %s:%d\n", 
               buffer, 
               inet_ntoa(client_addr.sin_addr), 
               ntohs(client_addr.sin_port));
        
        if (strncmp(buffer, "UDP_REGISTER:", 13) == 0) {
            const char *name = buffer + 13;
            handle_udp_register(server, &client_addr, name);
        }
        else if (strncmp(buffer, "MOVE:", 5) == 0) {
            const char *move_data = buffer + 5;
            handle_udp_move(server, &client_addr, move_data);
        }
        else if (strncmp(buffer, "PING", 4) == 0) {
            handle_udp_ping(server, &client_addr);
        }
        else if (strncmp(buffer, "GET_GAME_STATE", 14) == 0) {
            handle_udp_game_state(server, &client_addr);
        }
        else if (strncmp(buffer, "UDP_DISCONNECT", 14) == 0) {
            Client *client = network_find_client_by_udp_addr(&client_addr);
            if (client) {
                memset(&client->udp_addr, 0, sizeof(client->udp_addr));
                network_send_udp_response(server, &client_addr, "UDP_DISCONNECTED");
                printf("Client %s disconnesso da UDP\n", client->name);
            }
        }
        else {
            network_send_udp_response(server, &client_addr, "ERROR:Comando UDP sconosciuto");
        }
    }
    
    printf("Thread UDP terminato\n");
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

int network_init(ServerNetwork *server) {
#ifdef _WIN32
    WSADATA wsaData;
    long result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        printf("WSAStartup fallito: %lu\n", result);
        return 0;
    }
#endif

    memset(server, 0, sizeof(ServerNetwork));
    
    // Crea socket TCP
    server->tcp_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server->tcp_socket == INVALID_SOCKET_VALUE) {
        printf("Errore creazione socket TCP: %s\n", get_socket_error());
#ifdef _WIN32
        WSACleanup();
#endif
        return 0;
    }
    
    // Crea socket UDP
    server->udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (server->udp_socket == INVALID_SOCKET_VALUE) {
        printf("Errore creazione socket UDP: %s\n", get_socket_error());
        closesocket(server->tcp_socket);
#ifdef _WIN32
        WSACleanup();
#endif
        return 0;
    }
    
    // Configurazione indirizzo server
    memset(&server->server_addr, 0, sizeof(server->server_addr));
    server->server_addr.sin_family = AF_INET;
    server->server_addr.sin_addr.s_addr = INADDR_ANY; // Ascolta su tutte le interfacce di rete
    server->server_addr.sin_port = htons(SERVER_PORT);
    
    // Configura opzioni socket
#ifdef _WIN32
    BOOL opt = TRUE;
    if (setsockopt(server->tcp_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) == SOCKET_ERROR_VALUE) {
        printf("Errore setsockopt TCP SO_REUSEADDR: %s\n", get_socket_error());
    }
    if (setsockopt(server->udp_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) == SOCKET_ERROR_VALUE) {
        printf("Errore setsockopt UDP SO_REUSEADDR: %s\n", get_socket_error());
    }
    
    // Timeout per UDP
    DWORD timeout = 1000; 
    setsockopt(server->udp_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    
    // Disabilita algoritmo di Nagle per TCP (migliori prestazioni per piccoli pacchetti)
    BOOL no_delay = TRUE;
    setsockopt(server->tcp_socket, IPPROTO_TCP, TCP_NODELAY, (char*)&no_delay, sizeof(no_delay));
#else
    int opt = 1;
    if (setsockopt(server->tcp_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        printf("Errore setsockopt TCP SO_REUSEADDR: %s\n", strerror(errno));
    }
    if (setsockopt(server->udp_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        printf("Errore setsockopt UDP SO_REUSEADDR: %s\n", strerror(errno));
    }
    
    // Timeout per UDP
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    setsockopt(server->udp_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    // Disabilita algoritmo di Nagle per TCP
    int no_delay = 1;
    setsockopt(server->tcp_socket, IPPROTO_TCP, TCP_NODELAY, &no_delay, sizeof(no_delay));
#endif
    
    global_server = server;
    printf("Network inizializzato correttamente\n");
    return 1;
}

int network_start_listening(ServerNetwork *server) {
    // Bind TCP
    if (bind(server->tcp_socket, (struct sockaddr*)&server->server_addr, 
             sizeof(server->server_addr)) == SOCKET_ERROR_VALUE) {
        printf("Errore bind TCP sulla porta %d: %s\n", SERVER_PORT, get_socket_error());
        printf("Possibili cause:\n");
        printf("- Porta già in uso da un altro processo\n");
        printf("- Permessi insufficienti (provare con sudo su Linux)\n");
        printf("- Firewall che blocca la porta\n");
        return 0;
    }
    printf("TCP bind riuscito sulla porta %d\n", SERVER_PORT);
    
    // Bind UDP
    if (bind(server->udp_socket, (struct sockaddr*)&server->server_addr, 
             sizeof(server->server_addr)) == SOCKET_ERROR_VALUE) {
        printf("Errore bind UDP sulla porta %d: %s\n", SERVER_PORT, get_socket_error());
        return 0;
    }
    printf("UDP bind riuscito sulla porta %d\n", SERVER_PORT);
    
    // Avvia listening TCP
    if (listen(server->tcp_socket, SOMAXCONN) == SOCKET_ERROR_VALUE) {
        printf("Errore listen TCP: %s\n", get_socket_error());
        return 0;
    }
    
    server->is_running = 1;
    
    // Stampa informazioni del server
    print_server_info();
    printf("Server pronto per connessioni TCP e UDP\n");
    printf("IMPORTANTE: Assicurati che il firewall permetta connessioni sulla porta %d\n\n", SERVER_PORT);
    
    // Crea thread UDP
#ifdef _WIN32
    HANDLE udp_thread = CreateThread(NULL, 0, network_handle_udp_thread, server, 0, NULL);
    if (udp_thread == NULL) {
        printf("Errore creazione thread UDP: %lu\n", GetLastError());
        return 0;
    }
    CloseHandle(udp_thread);
#else
    pthread_t udp_thread;
    if (pthread_create(&udp_thread, NULL, network_handle_udp_thread, server) != 0) {
        printf("Errore creazione thread UDP: %s\n", strerror(errno));
        return 0;
    }
    pthread_detach(udp_thread);
#endif
    
    return 1;
}

void network_shutdown(ServerNetwork *server) {
    printf("Iniziando spegnimento server...\n");
    server->is_running = 0;

    if (server->tcp_socket != INVALID_SOCKET_VALUE) {
#ifdef _WIN32
        shutdown(server->tcp_socket, SD_BOTH);
#else
        shutdown(server->tcp_socket, SHUT_RDWR);
#endif
        closesocket(server->tcp_socket);
        server->tcp_socket = INVALID_SOCKET_VALUE;
        printf("Socket TCP chiuso\n");
    }

    if (server->udp_socket != INVALID_SOCKET_VALUE) {
#ifdef _WIN32
        shutdown(server->udp_socket, SD_BOTH);
#else
        shutdown(server->udp_socket, SHUT_RDWR);
#endif
        closesocket(server->udp_socket);
        server->udp_socket = INVALID_SOCKET_VALUE;
        printf("Socket UDP chiuso\n");
    }

#ifdef _WIN32
    WSACleanup();
#endif

    printf("Server spento completamente\n");
}

Client* network_accept_client(ServerNetwork *server) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    socket_t client_fd = accept(server->tcp_socket, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd == INVALID_SOCKET_VALUE) {
#ifdef _WIN32
        int error = WSAGetLastError();
        if (error != WSAEINTR && error != WSAECONNABORTED && server->is_running) {
            printf("Errore accept: %d\n", error);
        }
#else
        if (errno != EINTR && errno != ECONNABORTED && server->is_running) {
            printf("Errore accept: %s\n", strerror(errno));
        }
#endif
        return NULL;
    }
    
    // Informazioni dettagliate sulla connessione
    printf("\n=== NUOVA CONNESSIONE ===\n");
    printf("IP remoto: %s\n", inet_ntoa(client_addr.sin_addr));
    printf("Porta remota: %d\n", ntohs(client_addr.sin_port));
    printf("Socket FD: %d\n", (int)client_fd);
    printf("========================\n");
    
    // Crea struttura client
    Client *client = (Client*)malloc(sizeof(Client));
    if (!client) {
        printf("Errore allocazione memoria per client\n");
        closesocket(client_fd);
        return NULL;
    }
    
    memset(client, 0, sizeof(Client));
    client->client_fd = client_fd;
    client->is_active = 1;
    client->game_id = -1;
    strcpy(client->name, "Unknown");
    
    // Configura socket client per migliori prestazioni
#ifdef _WIN32
    BOOL no_delay = TRUE;
    setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, (char*)&no_delay, sizeof(no_delay));
#else
    int no_delay = 1;
    setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &no_delay, sizeof(no_delay));
#endif
    
    return client;
}

int network_send_to_client(Client *client, const char *message) {
    if (!client || !message || strlen(message) == 0 || strlen(message) >= MAX_MSG_SIZE) {
        LOG_ERROR("network_send_to_client: Parametri invalidi");
        return 0;
    }
    
    if (!client->is_active) {
        printf("Tentativo di invio a client inattivo: %s\n", client->name);
        return 0;
    }
    
    int bytes_sent = send(client->client_fd, message, (int)strlen(message), 0);
    if (bytes_sent == SOCKET_ERROR_VALUE) {
#ifdef _WIN32
        int error = WSAGetLastError();
        printf("Errore invio messaggio TCP a %s (FD:%d): %d\n", client->name, (int)client->client_fd, error);
#else
        printf("Errore invio messaggio TCP a %s (FD:%d): %s\n", client->name, (int)client->client_fd, strerror(errno));
#endif
        client->is_active = 0;
        return 0;
    }
    
    printf("TCP -> %s: %s\n", client->name, message);
    return 1;
}

int network_receive_from_client(Client *client, char *buffer, size_t buf_size) {
    if (!client || !client->is_active || !buffer) {
        return -1;
    }
    
    // Timeout più lungo per evitare disconnessioni premature
#ifdef _WIN32
    DWORD timeout_ms = 30000; // 30 secondi
    setsockopt(client->client_fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout_ms, sizeof(timeout_ms));
#else
    struct timeval timeout = { .tv_sec = 30, .tv_usec = 0 }; // 30 secondi
    setsockopt(client->client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif
    
    int bytes = recv(client->client_fd, buffer, (int)buf_size - 1, 0);
    if (bytes <= 0) {
        if (bytes == 0) {
            printf("Client %s (FD:%d) si è disconnesso correttamente\n", client->name, (int)client->client_fd);
        } else {
#ifdef _WIN32
            int error = WSAGetLastError();
            if (error == WSAETIMEDOUT) {
                printf("Timeout ricezione da %s (FD:%d) - client inattivo\n", client->name, (int)client->client_fd);
            } else {
                printf("Errore ricezione da %s (FD:%d): %d\n", client->name, (int)client->client_fd, error);
            }
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("Timeout ricezione da %s (FD:%d) - client inattivo\n", client->name, (int)client->client_fd);
            } else {
                printf("Errore ricezione da %s (FD:%d): %s\n", client->name, (int)client->client_fd, strerror(errno));
            }
#endif
        }
        client->is_active = 0;
        return -1;
    }
    
    buffer[bytes] = '\0';
    printf("TCP <- %s: %s\n", client->name, buffer);
    return bytes;
}

// Aggiungi questa funzione nel file network.c del server

thread_return_t THREAD_CALL network_handle_client_thread(thread_param_t arg) {
    Client *client = (Client*)arg;
    char buffer[MAX_MSG_SIZE];
    int bytes;
    
    printf("Thread client avviato per %s (FD: %d)\n", client->name, (int)client->client_fd);
    
    // Aggiungi il client alla lobby
    if (!lobby_add_client_reference(client)) {
        printf("Errore aggiunta client %s alla lobby\n", client->name);
        closesocket(client->client_fd);
        free(client);
#ifdef _WIN32
        return 0;
#else
        return NULL;
#endif
    }
    
    printf("Client FD:%d aggiunto alla lobby nello slot\n", (int)client->client_fd);
    
    while (client->is_active) {
        bytes = network_receive_from_client(client, buffer, sizeof(buffer));
        
        if (bytes <= 0) {
            printf("Client %s disconnesso (bytes: %d)\n", client->name, bytes);
            break;
        }
        
        printf("Gestendo messaggio da %s: %s\n", client->name, buffer);
        
        // FIX PRINCIPALE: Gestisci i messaggi di registrazione qui
        if (strncmp(buffer, "REGISTER:", 9) == 0) {
            const char *name = buffer + 9;
            
            // Controlla se il nome è già in uso
            if (lobby_find_client_by_name(name) && strcmp(client->name, name) != 0) {
                network_send_to_client(client, "ERROR:Nome già in uso");
                continue;
            }
            
            // Se è lo stesso nome, conferma la registrazione
            if (strcmp(client->name, name) == 0) {
                network_send_to_client(client, "OK:Già registrato");
                continue;
            }
            
            // Aggiorna il nome del client
            strncpy(client->name, name, MAX_NAME_LEN - 1);
            client->name[MAX_NAME_LEN - 1] = '\0';
            
            network_send_to_client(client, "OK:Registrazione completata");
            printf("Client registrato con nome: %s\n", name);
        }
        // Gestisci i ping/keep-alive
        else if (strcmp(buffer, "PING") == 0) {
            network_send_to_client(client, "PONG");
        }
        // Delega altri messaggi alla lobby
        else {
            lobby_handle_client_message(client, buffer);
        }
    }
    
    printf("Client %s sta uscendo dal thread\n", client->name);
    
    // Pulizia
    lobby_remove_client(client);
    closesocket(client->client_fd);
    free(client);
    
    printf("Thread client terminato\n");
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

int create_thread(thread_t *thread, thread_return_t (THREAD_CALL *start_routine)(thread_param_t), void *arg, const char *thread_name) {
#ifdef _WIN32
    *thread = CreateThread(NULL, 0, start_routine, arg, 0, NULL);
    if (*thread == NULL) {
        printf("Errore creazione thread %s: %lu\n", thread_name, GetLastError());
        return -1;
    }
#else
    if (pthread_create(thread, NULL, start_routine, arg) != 0) {
        printf("Errore creazione thread %s: %s\n", thread_name, strerror(errno));
        return -1;
    }
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
#endif
    printf("Thread %s creato con successo\n", thread_name);
    return 0;
}