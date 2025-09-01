#define _GNU_SOURCE
#include "headers/network.h"
#include "headers/ui.h"
#include "headers/game_logic.h"
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <errno.h>
    #include <fcntl.h>
#endif

static char last_error[256] = {0};


/**
 * Imposta il messaggio di errore globale con informazioni di sistema specifiche per piattaforma.
 * Utilizza WSAGetLastError() su Windows ed errno su sistemi Unix.
 * 
 * @param msg Messaggio di errore base da impostare
 * 
 * @note Il messaggio viene memorizzato nel buffer statico last_error
 * @note Include automaticamente codici di errore di sistema quando disponibili
 */
static void set_error(const char *msg) {
#ifdef _WIN32
    int error = WSAGetLastError();
    if (error != 0) {
        snprintf(last_error, sizeof(last_error), "%s (WSAGetLastError: %d)", msg, error);
        return;
    }
#else
    if (errno != 0) {
        snprintf(last_error, sizeof(last_error), "%s (errno: %d - %s)", msg, errno, strerror(errno));
        return;
    }
#endif
    strncpy(last_error, msg, sizeof(last_error) - 1);
    last_error[sizeof(last_error) - 1] = '\0';
}

/**
 * Thread di keep-alive che invia periodicamente messaggi PING al server.
 * Mantiene attiva la connessione e rileva disconnessioni automaticamente.
 * 
 * @param lpParam Puntatore alla struttura NetworkConnection
 * @return 0 quando il thread termina
 * 
 * @note Cross-platform: utilizza thread Windows o pthread
 * @note Invia PING ogni KEEP_ALIVE_INTERVAL secondi
 * @note Si arresta quando keep_alive_active diventa false
 */
#ifdef _WIN32
static DWORD WINAPI keep_alive_thread(LPVOID lpParam) {
#else
static void* keep_alive_thread(void* lpParam) {
#endif
    NetworkConnection* conn = (NetworkConnection*)lpParam;
    
    while (conn->keep_alive_active) {
        if (!network_send(conn, "PING", 0)) {
            printf("Keep-alive fallito\n");
            break;
        }
        
        
#ifdef _WIN32
        Sleep(KEEP_ALIVE_INTERVAL * 1000);
#else
        sleep(KEEP_ALIVE_INTERVAL);
#endif
    }
    
    return 0;
}

/**
 * Avvia il thread di keep-alive per mantenere attiva la connessione al server.
 * Crea un thread separato che invierà periodicamente messaggi PING.
 * 
 * @param conn Puntatore alla connessione di rete da mantenere attiva
 * 
 * @note Non fa nulla se conn è NULL o se il keep-alive è già attivo
 * @note Il thread viene creato in modalità detached per cleanup automatico
 */
void network_start_keep_alive(NetworkConnection *conn) {
    if (!conn) return;
    
    
    return;
    
    conn->keep_alive_active = 1;
    
#ifdef _WIN32
    conn->keep_alive_thread = CreateThread(NULL, 0, keep_alive_thread, conn, 0, NULL);
    if (conn->keep_alive_thread == NULL) {
        printf("Errore creazione thread keep-alive\n");
    }
#else
    if (pthread_create(&conn->keep_alive_thread, NULL, keep_alive_thread, conn) != 0) {
        printf("Errore creazione thread keep-alive\n");
    }
#endif
}

/**
 * Arresta il thread di keep-alive e attende la sua terminazione.
 * Imposta il flag di terminazione e attende che il thread si chiuda pulitamente.
 * 
 * @param conn Puntatore alla connessione di rete
 * 
 * @note Attende al massimo 1 secondo su Windows per la terminazione del thread
 * @note Su sistemi Unix attende indefinitamente con pthread_join
 * @note Gestisce automaticamente la pulizia delle risorse del thread
 */
void network_stop_keep_alive(NetworkConnection *conn) {
    if (!conn) return;
    
    conn->keep_alive_active = 0;
    
#ifdef _WIN32
    if (conn->keep_alive_thread) {
        WaitForSingleObject(conn->keep_alive_thread, 1000);
        CloseHandle(conn->keep_alive_thread);
    }
#else
    if (conn->keep_alive_thread) {
        pthread_join(conn->keep_alive_thread, NULL);
    }
#endif
}

/**
 * Inizializza il sistema di rete globale per la piattaforma corrente.
 * Su Windows inizializza Winsock, su Unix non fa operazioni specifiche.
 * 
 * @return 1 se l'inizializzazione è riuscita, 0 in caso di errore
 * 
 * @note Deve essere chiamata prima di qualsiasi operazione di rete
 * @note Su Windows richiede la versione 2.2 di Winsock
 */
int network_global_init() {
#ifdef _WIN32
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        char error_msg[100];
        snprintf(error_msg, sizeof(error_msg), "WSAStartup fallito: %d", result);
        set_error(error_msg);
        return 0;
    }
#endif
    return 1;
}

/**
 * Pulisce le risorse di rete globali della piattaforma.
 * Su Windows chiama WSACleanup, su Unix non fa operazioni specifiche.
 * 
 * @note Deve essere chiamata alla fine del programma per liberare le risorse
 * @note Corrisponde a network_global_init() per la pulizia simmetrica
 */
void network_global_cleanup() {
#ifdef _WIN32
    WSACleanup();
#endif
}

/**
 * Inizializza una struttura di connessione di rete.
 * Imposta tutti i campi a valori di default e inizializza i socket.
 * 
 * @param conn Puntatore alla struttura NetworkConnection da inizializzare
 * @return 1 se l'inizializzazione è riuscita, 0 in caso di errore
 * 
 * @note Crea socket TCP e UDP pronti per la connessione
 * @note I socket vengono configurati con timeout appropriati
 */
int network_init(NetworkConnection *conn) {
    memset(conn, 0, sizeof(NetworkConnection));
    conn->tcp_sock = INVALID_SOCKET_VALUE;
    conn->udp_sock = INVALID_SOCKET_VALUE;
    return 1;
}


#ifndef _WIN32
    #include <pthread.h>
#endif

pthread_t receiver_thread;
int game_running = 1;
int game_started = 0;
char player_symbol = ' ';



void network_cleanup(NetworkConnection *conn) {
    network_stop_keep_alive(conn);
    network_disconnect(conn);
}

void* message_receiver_thread(void* arg) {
    NetworkConnection* conn = (NetworkConnection*)arg;
    char buffer[1024];
    
    printf("[DEBUG] Thread receiver avviato\n");
    
    while (game_running) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = network_receive(conn, buffer, sizeof(buffer), 0);
        
        if (bytes_received > 0) {
            
            buffer[strcspn(buffer, "\n")] = 0;
            
            printf("\n[DEBUG] Ricevuto: %s\n", buffer);
            handle_server_message(buffer);
        }
        else if (bytes_received == 0) {
            
            usleep(100000); 
            continue;
        }
        else {
            
            const char* error = network_get_error();
            
            
            if (strstr(error, "Interrupted system call") != NULL) {
                printf("[DEBUG] Segnale ricevuto, continuo...\n");
                continue;
            }
            
            
            if (strstr(error, "Timeout") != NULL || 
                strstr(error, "EAGAIN") != NULL || 
                strstr(error, "EWOULDBLOCK") != NULL || 
                strstr(error, "WSAETIMEDOUT") != NULL) {
                usleep(100000); 
                continue;
            }
            
            
            printf("\n[ERRORE] Connessione persa: %s\n", error);
            game_running = 0;
            break;
        }
    }
    
    printf("[DEBUG] Thread receiver terminato\n");
    return NULL;
}

void handle_server_message(const char* message) {
    
    
    
    if (strncmp(message, "GAMES:", 6) == 0) {
        printf("\n=== PARTITE DISPONIBILI ===\n");
        printf("%s\n", message + 6);
        printf("===========================\n");
    }
    else if (strncmp(message, "OK:", 3) == 0) {
        printf("✓ %s\n", message + 3);
    }
    else if (strncmp(message, "ERROR:", 6) == 0) {
        printf("❌ Errore: %s\n", message + 6);
    }
    else if (strcmp(message, "PONG") != 0) {
        
        if (strncmp(message, "GAME_START:", 11) != 0 &&
            strncmp(message, "JOIN_ACCEPTED:", 14) != 0 &&
            strncmp(message, "MOVE:", 5) != 0 &&
            strncmp(message, "GAME_OVER:", 10) != 0 &&
            strncmp(message, "OPPONENT_", 9) != 0) {
            printf("[DEBUG] Messaggio ricevuto dal thread: %s\n", message);
        }
    }
}

int network_start_receiver_thread(NetworkConnection* conn) {
    if (!conn) return 0;
    
    game_running = 1;
    printf("Avvio thread receiver...\n");
    
#ifdef _WIN32
    conn->receiver_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)message_receiver_thread, conn, 0, NULL);
    if (conn->receiver_thread == NULL) {
        printf("Errore nella creazione del thread receiver\n");
        return 0;
    }
#else
    if (pthread_create(&receiver_thread, NULL, message_receiver_thread, conn) != 0) {
        printf("Errore nella creazione del thread receiver\n");
        return 0;
    }
#endif
    
    return 1;
}

void network_cleanup_connection(NetworkConnection* conn) {
    printf("[DEBUG] Inizio cleanup connessione\n");
    game_running = 0;  
    
#ifdef _WIN32
    if (conn->receiver_thread) {
        WaitForSingleObject(conn->receiver_thread, 1000);
        CloseHandle(conn->receiver_thread);
    }
#else
    if (receiver_thread) {
        pthread_join(receiver_thread, NULL);
    }
#endif
    
    network_cleanup(conn);
    printf("[DEBUG] Cleanup connessione completato\n");
}


void network_stop_receiver() {
    game_running = 0;
}

int network_set_timeout(socket_t sock, int timeout_sec) {
#ifdef _WIN32
    DWORD timeout = timeout_sec * 1000;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) != 0) {
        return -1;
    }
    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout)) != 0) {
        return -1;
    }
#else
    struct timeval tv;
    tv.tv_sec = timeout_sec;
    tv.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0) {
        return -1;
    }
    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != 0) {
        return -1;
    }
#endif
    return 0;
}

int network_connect_to_server(NetworkConnection *conn) {
    conn->tcp_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (conn->tcp_sock == INVALID_SOCKET_VALUE) {
        set_error("Errore creazione socket TCP");
        return 0;
    }

    
    network_set_timeout(conn->tcp_sock, TIMEOUT_SEC);

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        set_error("Indirizzo server non valido");
        closesocket(conn->tcp_sock);
        conn->tcp_sock = INVALID_SOCKET_VALUE;
        return 0;
    }

    if (connect(conn->tcp_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR_VALUE) {
        set_error("Connessione al server fallita");
        closesocket(conn->tcp_sock);
        conn->tcp_sock = INVALID_SOCKET_VALUE;
        return 0;
    }

    conn->udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (conn->udp_sock == INVALID_SOCKET_VALUE) {
        set_error("Errore creazione socket UDP");
        closesocket(conn->tcp_sock);
        conn->tcp_sock = INVALID_SOCKET_VALUE;
        return 0;
    }

    return 1;
}

int network_register_name(NetworkConnection *conn, const char *name) {
    if (!conn || conn->tcp_sock == INVALID_SOCKET_VALUE || !name) {
        set_error("Connessione non valida");
        return 0;
    }

    char msg[MAX_MSG_SIZE];
    snprintf(msg, sizeof(msg), "REGISTER:%s", name);

    if (send(conn->tcp_sock, msg, strlen(msg), 0) == SOCKET_ERROR_VALUE) {
        set_error("Invio nome fallito");
        return 0;
    }

    
    network_set_timeout(conn->tcp_sock, 10); 

    char response[MAX_MSG_SIZE];
    int bytes = recv(conn->tcp_sock, response, sizeof(response) - 1, 0);
    if (bytes <= 0) {
        char err_msg[256];
#ifdef _WIN32
        int error = WSAGetLastError();
        snprintf(err_msg, sizeof(err_msg), "Nessuna risposta dal server (errore: %d)", error);
#else
        snprintf(err_msg, sizeof(err_msg), "Nessuna risposta dal server (errno: %d - %s)", errno, strerror(errno));
#endif
        set_error(err_msg);
        return 0;
    }
    response[bytes] = '\0';

    
    if (strstr(response, "OK:") != NULL || strstr(response, "OK") != NULL) {
        
        if (network_set_timeout(conn->tcp_sock, 60) != 0) {
            set_error("Impossibile impostare timeout connessione");
            return 0;
        }
        
        strncpy(conn->player_name, name, sizeof(conn->player_name) - 1);
        conn->player_name[sizeof(conn->player_name) - 1] = '\0';
        return 1;
    } 
    else if (strstr(response, "ERROR") != NULL) {
        
        set_error(response);
        return 0;
    }
    else {
        
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "Risposta server non valida: %s", response);
        set_error(err_msg);
        return 0;
    }
}

int network_create_game(NetworkConnection *conn) {
    const char *msg = "CREATE_GAME";
    if (send(conn->tcp_sock, msg, strlen(msg), 0) == SOCKET_ERROR_VALUE) {
        set_error("Invio richiesta creazione partita fallito");
        return 0;
    }
    return 1;
}

int network_join_game(NetworkConnection *conn, int game_id) {
    char msg[MAX_MSG_SIZE];
    snprintf(msg, sizeof(msg), "JOIN_GAME:%d", game_id);
    
    if (send(conn->tcp_sock, msg, strlen(msg), 0) == SOCKET_ERROR_VALUE) {
        set_error("Invio richiesta join partita fallito");
        return 0;
    }
    return 1;
}

int network_send_move(NetworkConnection *conn, int move) {
    if (conn->udp_sock == INVALID_SOCKET_VALUE) {
        set_error("Connessione UDP non inizializzata");
        return 0;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    char msg[16];
    snprintf(msg, sizeof(msg), "MOVE:%d", move);

    ssize_t result;
    do {
        result = sendto(conn->udp_sock, msg, strlen(msg), 0, 
                       (struct sockaddr *)&server_addr, sizeof(server_addr));
    } while (result == -1 && errno == EINTR);  
    
    if (result == SOCKET_ERROR_VALUE) {
        set_error("Invio mossa fallito");
        return 0;
    }
    return 1;
}


int network_flush_receive_buffer(NetworkConnection *conn) {
    if (conn->tcp_sock == INVALID_SOCKET_VALUE) {
        set_error("Connessione non inizializzata");
        return -1;
    }

    
    int flags = fcntl(conn->tcp_sock, F_GETFL, 0);
    fcntl(conn->tcp_sock, F_SETFL, flags | O_NONBLOCK);
    
    char temp_buffer[1024];
    int total_flushed = 0;
    int bytes_read;
    
    
    do {
        bytes_read = recv(conn->tcp_sock, temp_buffer, sizeof(temp_buffer), 0);
        if (bytes_read > 0) {
            total_flushed += bytes_read;
            temp_buffer[bytes_read] = '\0';
            printf("[DEBUG] Drained extra data: %s\n", temp_buffer);
        }
    } while (bytes_read > 0);
    
    
    fcntl(conn->tcp_sock, F_SETFL, flags);
    
    return total_flushed;
}

int network_receive(NetworkConnection *conn, char *buffer, size_t buf_size, int use_udp) {
    if (!conn || !buffer || buf_size <= 0) {
        set_error("Parametri non validi");
        return -1;
    }

    if ((use_udp && conn->udp_sock == INVALID_SOCKET_VALUE) || 
        (!use_udp && conn->tcp_sock == INVALID_SOCKET_VALUE)) {
        set_error("Connessione non valida");
        return -1;
    }

retry_receive:
    socket_t sock = use_udp ? conn->udp_sock : conn->tcp_sock;
    int bytes;
    
    
    if (use_udp) {
        struct sockaddr_in from_addr;
        socklen_t from_len = sizeof(from_addr);
        do {
            bytes = recvfrom(sock, buffer, buf_size - 1, 0, 
                           (struct sockaddr*)&from_addr, &from_len);
        } while (bytes == -1 && errno == EINTR);  
    } else {
        do {
            bytes = recv(sock, buffer, buf_size - 1, 0);
            if (bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                
                usleep(10000); 
            }
        } while (bytes == -1 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK));
    }
    
    if (bytes > 0) {
        buffer[bytes] = '\0';  
        
        
        char *newline = strchr(buffer, '\n');
        if (newline) *newline = '\0';
        
        
        printf("[DEBUG_RECV] Ricevuto: '%s'\n", buffer);
        
        
        if (strcmp(buffer, "PING") == 0) {
            printf("[DEBUG_RECV] Auto-risposta a PING\n");
            network_send(conn, "PONG", use_udp);
            goto retry_receive; 
        }
        if (strcmp(buffer, "PONG") == 0) {
            printf("[DEBUG_RECV] Ignoro PONG\n");
            goto retry_receive; 
        }

        return strlen(buffer);  
    } 
    else if (bytes == 0) {
        
        if (!use_udp) {
            set_error("Connessione chiusa dal server");
            strcpy(buffer, "SERVER_DOWN");
        }
        return 0;
    } 
    else {
        
#ifdef _WIN32
        int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK || error == WSAETIMEDOUT) {
            return 0;  
        }
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "Errore ricezione: %d", error);
        set_error(err_msg);
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;  
        }
        if (errno == EINTR) {
            
            printf("[DEBUG] System call interrotta da segnale, riprovo...\n");
            goto retry_receive;
        }
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "Errore ricezione: %s", strerror(errno));
        set_error(err_msg);
#endif
        return -1;
    }
}

int network_send(NetworkConnection *conn, const char *message, int use_udp) {
    if (!conn || !message) {
        return 0;
    }
    
    
    printf("[DEBUG_SEND] Invio: '%s'\n", message);
    
    if (use_udp && conn->udp_sock != INVALID_SOCKET_VALUE) {
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(SERVER_PORT);
        inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);
        
        ssize_t result;
        do {
            result = sendto(conn->udp_sock, message, strlen(message), 0,
                           (struct sockaddr*)&server_addr, sizeof(server_addr));
        } while (result == -1 && errno == EINTR);  
        
        if (result == SOCKET_ERROR_VALUE) {
            return 0;
        }
    } else if (conn->tcp_sock != INVALID_SOCKET_VALUE) {
        
        char tcp_message[1024];
        snprintf(tcp_message, sizeof(tcp_message), "%s\n", message);
        
        ssize_t result;
        do {
            result = send(conn->tcp_sock, tcp_message, strlen(tcp_message), 0);
        } while (result == -1 && errno == EINTR);  
        
        if (result == SOCKET_ERROR_VALUE) {
            return 0;
        }
    } else {
        return 0;
    }
    
    return 1;
}

void network_disconnect(NetworkConnection *conn) {
    if (conn->tcp_sock != INVALID_SOCKET_VALUE) {
        closesocket(conn->tcp_sock);
        conn->tcp_sock = INVALID_SOCKET_VALUE;
    }
    if (conn->udp_sock != INVALID_SOCKET_VALUE) {
        closesocket(conn->udp_sock);
        conn->udp_sock = INVALID_SOCKET_VALUE;
    }
}

const char *network_get_error() {
    return last_error;
}

int network_request_rematch(NetworkConnection *conn) {
    const char *msg = "REMATCH";
    
    if (send(conn->tcp_sock, msg, strlen(msg), 0) == SOCKET_ERROR_VALUE) {
        set_error("Invio richiesta rematch fallito");
        return 0;
    }
    return 1;
}

int network_approve_join(NetworkConnection *conn, int approve) {
    char msg[32];
    snprintf(msg, sizeof(msg), "APPROVE:%d", approve);
    
    if (send(conn->tcp_sock, msg, strlen(msg), 0) == SOCKET_ERROR_VALUE) {
        set_error("Invio approvazione join fallito");
        return 0;
    }
    return 1;
}

int network_set_nonblocking(socket_t sock) {
#ifdef _WIN32
    unsigned long mode = 1;
    return ioctlsocket(sock, FIONBIO, &mode) == 0 ? 0 : -1;
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
}