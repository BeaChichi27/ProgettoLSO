#ifndef SERVER_NETWORK_H
#define SERVER_NETWORK_H

/*
 * HEADER NETWORK - GESTIONE RETE SERVER TRIS
 *
 * Questo header definisce tutte le strutture dati e funzioni necessarie
 * per la gestione della comunicazione di rete del server Tris multiplayer.
 * 
 * Gestisce connessioni TCP e UDP, threading per client multipli,
 * e comunicazione bidirezionale con i client connessi.
 *
 * Supporta fino a MAX_CLIENTS connessioni simultanee con gestione
 * thread-safe e cross-platform (Windows/Linux).
 *
 * Include astrazioni per socket, thread e mutex per portabilità.
 */

#include <unistd.h>
#include <sys/time.h>

// ========== CONFIGURAZIONE CROSS-PLATFORM ==========

#ifdef _WIN32
    // Configurazione per Windows
    #include <winsock2.h>
    #include <windows.h>
    #include <ws2tcpip.h>
    
    typedef SOCKET socket_t;
    typedef HANDLE thread_t;
    typedef DWORD thread_return_t;
    typedef LPVOID thread_param_t;
    typedef CRITICAL_SECTION mutex_t;
    
    #define THREAD_CALL WINAPI
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
    #define SOCKET_ERROR_VALUE SOCKET_ERROR
#else
    // Configurazione per sistemi Unix/Linux
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>  
    #include <arpa/inet.h>
    #include <netdb.h>        
    #include <pthread.h>
    #include <unistd.h>
    #include <errno.h>
    
    typedef int socket_t;
    typedef pthread_t thread_t;
    typedef void* thread_return_t;
    typedef void* thread_param_t;
    typedef pthread_mutex_t mutex_t;
    
    #define THREAD_CALL
    #define INVALID_SOCKET_VALUE -1
    #define SOCKET_ERROR_VALUE -1
    #define closesocket close
#endif

// ========== CONFIGURAZIONI SERVER ==========

#define SERVER_PORT 8080            // Porta di ascolto del server
#define MAX_MSG_SIZE 1024           // Dimensione massima messaggi
#define MAX_NAME_LEN 50             // Lunghezza massima nome giocatore
#define MAX_CLIENTS 100             // Numero massimo client simultanei

// ========== STRUTTURE DATI ==========

/**
 * Struttura che rappresenta un client connesso al server.
 * Contiene tutte le informazioni necessarie per la comunicazione e lo stato.
 */
typedef struct {
    socket_t client_fd;             // File descriptor della connessione TCP
    char name[MAX_NAME_LEN];        // Nome del giocatore
    int game_id;                    // ID della partita corrente (-1 se non in gioco)
    char symbol;                    // Simbolo del giocatore (X o O)
    struct sockaddr_in udp_addr;    // Indirizzo UDP per comunicazione veloce
    int is_active;                  // Flag per stato attivazione client
    thread_t thread;                // Thread dedicato per gestione client
} Client;

/**
 * Struttura principale del server di rete.
 * Contiene le socket di ascolto e lo stato del server.
 */
typedef struct {
    socket_t tcp_socket;            // Socket TCP per connessioni affidabili
    socket_t udp_socket;            // Socket UDP per comunicazioni veloci
    struct sockaddr_in server_addr; // Indirizzo del server
    int is_running;                 // Flag per stato di esecuzione server
} ServerNetwork;

// ========== FUNZIONI DI GESTIONE SERVER ==========

int network_init(ServerNetwork *server);
int network_start_listening(ServerNetwork *server);
void network_shutdown(ServerNetwork *server);

// ========== FUNZIONI DI GESTIONE CLIENT ==========

Client* network_accept_client(ServerNetwork *server);
int network_send_to_client(Client *client, const char *message);
int network_receive_from_client(Client *client, char *buffer, size_t buf_size);

// ========== FUNZIONI DI THREADING ==========

thread_return_t THREAD_CALL network_handle_client_thread(thread_param_t arg);
thread_return_t THREAD_CALL network_handle_udp_thread(thread_param_t arg);

// ========== FUNZIONI DI UTILITÀ CROSS-PLATFORM ==========

int network_platform_init(void);
void network_platform_cleanup(void);
int network_set_socket_nonblocking(socket_t sock);
int create_thread(thread_t *thread, thread_return_t (THREAD_CALL *start_routine)(thread_param_t), void *arg, const char *thread_name);

// ========== FUNZIONI DI DEBUG E INFORMAZIONI ==========

const char* get_socket_error(void);
void print_server_info(void);

#endif