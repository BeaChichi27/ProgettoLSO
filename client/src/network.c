#include "headers/network.h"
#include "headers/ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

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
        
        // Attendi l'intervallo
#ifdef _WIN32
        Sleep(KEEP_ALIVE_INTERVAL * 1000);
#else
        sleep(KEEP_ALIVE_INTERVAL);
#endif
    }
    
    return 0;
}

void network_start_keep_alive(NetworkConnection *conn) {
    if (!conn) return;
    
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

// Cross-platform network initialization
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

void network_global_cleanup() {
#ifdef _WIN32
    WSACleanup();
#endif
}

int network_init(NetworkConnection *conn) {
    memset(conn, 0, sizeof(NetworkConnection));
    conn->tcp_sock = INVALID_SOCKET_VALUE;
    conn->udp_sock = INVALID_SOCKET_VALUE;
    return 1;
}

// Cross-platform timeout setting
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

    // Set timeout
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

    char response[MAX_MSG_SIZE];
    int bytes = recv(conn->tcp_sock, response, sizeof(response) - 1, 0);
    if (bytes <= 0) {
        set_error("Nessuna risposta dal server");
        return 0;
    }
    response[bytes] = '\0';

    // Modifica qui: gestisci meglio la risposta
    if (strstr(response, "OK") == NULL && strstr(response, "ERROR") != NULL) {
        set_error(response); // Mostra l'errore del server
        return 0;
    }

    if (network_set_timeout(conn->tcp_sock, 60) != 0) {
        set_error("Impossibile impostare timeout connessione");
        return 0;
    }
    
    strncpy(conn->player_name, name, sizeof(conn->player_name) - 1);
    conn->player_name[sizeof(conn->player_name) - 1] = '\0';
    return 1;
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

    if (sendto(conn->udp_sock, msg, strlen(msg), 0, 
              (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR_VALUE) {
        set_error("Invio mossa fallito");
        return 0;
    }
    return 1;
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

    socket_t sock = use_udp ? conn->udp_sock : conn->tcp_sock;
    int bytes;
    
    while (1) {  // Loop per gestire i PING
        if (use_udp) {
            struct sockaddr_in from_addr;
            socklen_t from_len = sizeof(from_addr);
            bytes = recvfrom(sock, buffer, buf_size - 1, 0, 
                          (struct sockaddr*)&from_addr, &from_len);
        } else {
            bytes = recv(sock, buffer, buf_size - 1, 0);
        }
        
        if (bytes > 0) {
            buffer[bytes] = '\0';  // Termina la stringa

            // Gestione automatica keep-alive per PING
            if (strcmp(buffer, "PING") == 0) {
                network_send_to_client(client, "PONG");
                continue; // Non passare alla lobby
            }
            // Delega altri messaggi alla lobby
            else {
                lobby_handle_client_message(client, buffer);
            }

            return bytes;  // Restituisce messaggi non-PING
        } 
        else if (bytes == 0) {
            // Connessione chiusa dal server (solo TCP)
            if (!use_udp) {
                set_error("Connessione chiusa dal server");
                strcpy(buffer, "SERVER_DOWN");
            }
            return 0;
        } 
        else {
            // Gestione errori
#ifdef _WIN32
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK || error == WSAETIMEDOUT) {
                return 0;  // Timeout (non un errore)
            }
            char err_msg[256];
            snprintf(err_msg, sizeof(err_msg), "Errore ricezione: %d", error);
            set_error(err_msg);
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return 0;  // Timeout (non un errore)
            }
            char err_msg[256];
            snprintf(err_msg, sizeof(err_msg), "Errore ricezione: %s", strerror(errno));
            set_error(err_msg);
#endif
            return -1;
        }
    }
}
}

int network_send(NetworkConnection *conn, const char *message, int use_udp) {
    if (!conn || !message) {
        return 0;
    }
    
    if (use_udp && conn->udp_sock != INVALID_SOCKET_VALUE) {
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(SERVER_PORT);
        inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);
        
        if (sendto(conn->udp_sock, message, strlen(message), 0,
                  (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR_VALUE) {
            return 0;
        }
    } else if (conn->tcp_sock != INVALID_SOCKET_VALUE) {
        if (send(conn->tcp_sock, message, strlen(message), 0) == SOCKET_ERROR_VALUE) {
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