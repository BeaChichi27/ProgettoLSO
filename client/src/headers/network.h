#ifndef NETWORK_H
#define NETWORK_H

#ifndef TIMEOUT_SEC
#define TIMEOUT_SEC 5
#endif
#include <pthread.h>
// Cross-platform socket includes
// Cross-platform socket includes
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

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define MAX_MSG_SIZE 1024
#define TIMEOUT_SEC 5

typedef struct {
    socket_t tcp_sock;
    socket_t udp_sock;
    int udp_port;
    char player_name[50];
#ifdef _WIN32
    HANDLE keep_alive_thread;
#else
    pthread_t keep_alive_thread;
#endif
    int keep_alive_active;
} NetworkConnection;

#define KEEP_ALIVE_INTERVAL 30 // secondi tra i ping

void network_start_keep_alive(NetworkConnection *conn);
void network_stop_keep_alive(NetworkConnection *conn);

int network_global_init();
void network_global_cleanup();
int network_init(NetworkConnection *conn);
int network_connect_to_server(NetworkConnection *conn);
void network_disconnect(NetworkConnection *conn);

int network_register_name(NetworkConnection *conn, const char *name);
int network_create_game(NetworkConnection *conn);
int network_join_game(NetworkConnection *conn, int game_id);
int network_send_move(NetworkConnection *conn, int move);
int network_request_rematch(NetworkConnection *conn);
int network_approve_join(NetworkConnection *conn, int approve);

int network_send(NetworkConnection *conn, const char *message, int use_udp);
int network_receive(NetworkConnection *conn, char *buffer, size_t buf_size, int use_udp);
int network_flush_receive_buffer(NetworkConnection *conn);

const char *network_get_error();

// Cross-platform utility functions
int network_set_timeout(socket_t sock, int timeout_sec);
int network_set_nonblocking(socket_t sock);

// --- PROTOTIPI AGGIUNTI ---
void network_cleanup(NetworkConnection *conn);
int network_start_receiver_thread(NetworkConnection* conn);
void network_cleanup_connection(NetworkConnection* conn);
void handle_server_message(const char* message);
void network_stop_receiver(void);

#endif