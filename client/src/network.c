#include "headers/network.h"
#include "headers/ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Cross-platform includes for implementation
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/time.h>
    #include <netdb.h>
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

    if (strstr(response, "OK") != NULL) {
        strncpy(conn->player_name, name, sizeof(conn->player_name) - 1);
        conn->player_name[sizeof(conn->player_name) - 1] = '\0';
        return 1;
    } else {
        set_error(response);
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

    if (sendto(conn->udp_sock, msg, strlen(msg), 0, 
              (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR_VALUE) {
        set_error("Invio mossa fallito");
        return 0;
    }
    return 1;
}

int network_receive(NetworkConnection *conn, char *buffer, size_t buf_size, int use_udp) {
    if (!conn || (use_udp==0 && conn->tcp_sock == INVALID_SOCKET_VALUE) || 
        (use_udp==1 && conn->udp_sock == INVALID_SOCKET_VALUE)) {
        set_error("Connessione non valida");
        return -1;
    }

    socket_t sock = use_udp ? conn->udp_sock : conn->tcp_sock;
    int bytes = recv(sock, buffer, buf_size - 1, 0);
    
    if (bytes > 0) {
        buffer[bytes] = '\0';
        if (strstr(buffer, "WAITING_OPPONENT")) {
            ui_show_waiting_screen();
        }
    } else if (bytes == 0) {
        set_error("Connessione chiusa dal server");
    } else {
#ifdef _WIN32
        int error = WSAGetLastError();
        if (error == WSAETIMEDOUT) {
            set_error("Timeout nella ricezione");
        } else {
            set_error("Errore nella ricezione");
        }
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            set_error("Timeout nella ricezione");
        } else {
            set_error("Errore nella ricezione");
        }
#endif
    }
    
    return bytes;
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
    u_long mode = 1;
    return ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    return fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
}