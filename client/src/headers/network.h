#ifndef NETWORK_H
#define NETWORK_H

/*
 * HEADER NETWORK - GESTIONE COMUNICAZIONE CLIENT ↔ SERVER
 *
 * Questo header definisce tutte le funzioni e strutture necessarie per
 * la comunicazione di rete del client del gioco di Tris.
 * 
 * Supporta sia connessioni TCP che UDP, con gestione keep-alive
 * e threading per ricezione asincrona dei messaggi.
 *
 * Compatibile con Windows e Linux.
 */

#ifndef TIMEOUT_SEC
#define TIMEOUT_SEC 5
#endif
#include <pthread.h>

// ========== CONFIGURAZIONE CROSS-PLATFORM ==========

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    
    typedef SOCKET socket_t;
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
    #define SOCKET_ERROR_VALUE SOCKET_ERROR
    #define SHUT_RDWR SD_BOTH
#else
    // Configurazione per sistemi Unix/Linux
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <errno.h>
    #include <fcntl.h>
    
    typedef int socket_t;
    #define INVALID_SOCKET_VALUE -1
    #define SOCKET_ERROR_VALUE -1
    #define closesocket close
    #define SHUT_RDWR SHUT_RDWR
#endif

// ========== CONFIGURAZIONI DEFAULT ==========

#define SERVER_IP "127.0.0.1"         // Indirizzo IP del server
#define SERVER_PORT 8080               // Porta del server
#define MAX_MSG_SIZE 1024              // Dimensione massima messaggio
#define TIMEOUT_SEC 5                  // Timeout per operazioni di rete
#define KEEP_ALIVE_INTERVAL 30         // Intervallo keep-alive in secondi

// ========== STRUTTURE DATI ==========

/**
 * Struttura che rappresenta una connessione di rete del client.
 * Contiene tutti i dati necessari per comunicare con il server.
 */
typedef struct {
    socket_t tcp_sock;              // Socket TCP per comunicazione affidabile
    socket_t udp_sock;              // Socket UDP per comunicazione veloce
    int udp_port;                   // Porta UDP assegnata
    char player_name[50];           // Nome del giocatore
#ifdef _WIN32
    HANDLE keep_alive_thread;       // Thread keep-alive (Windows)
#else
    pthread_t keep_alive_thread;    // Thread keep-alive (Linux)
#endif
    int keep_alive_active;          // Flag per controllo keep-alive
} NetworkConnection;

// ========== FUNZIONI DI GESTIONE KEEP-ALIVE ==========

void network_start_keep_alive(NetworkConnection *conn);
void network_stop_keep_alive(NetworkConnection *conn);

// ========== FUNZIONI DI INIZIALIZZAZIONE ==========

int network_global_init();
void network_global_cleanup();
int network_init(NetworkConnection *conn);
int network_connect_to_server(NetworkConnection *conn);
void network_disconnect(NetworkConnection *conn);

// ========== FUNZIONI DI COMUNICAZIONE GIOCO ==========

int network_register_name(NetworkConnection *conn, const char *name);
int network_create_game(NetworkConnection *conn);
int network_join_game(NetworkConnection *conn, int game_id);
int network_send_move(NetworkConnection *conn, int move);
int network_request_rematch(NetworkConnection *conn);
int network_approve_join(NetworkConnection *conn, int approve);

// ========== FUNZIONI DI COMUNICAZIONE LOW-LEVEL ==========

int network_send(NetworkConnection *conn, const char *message, int use_udp);
int network_receive(NetworkConnection *conn, char *buffer, size_t buf_size, int use_udp);
int network_flush_receive_buffer(NetworkConnection *conn);

const char *network_get_error();

// ========== FUNZIONI DI UTILITÀ ==========

int network_set_timeout(socket_t sock, int timeout_sec);
int network_set_nonblocking(socket_t sock);

// ========== FUNZIONI DI CLEANUP E THREADING ==========

void network_cleanup(NetworkConnection *conn);
int network_start_receiver_thread(NetworkConnection* conn);
void network_cleanup_connection(NetworkConnection* conn);
void handle_server_message(const char* message);
void network_stop_receiver(void);

#endif