#ifndef SERVER_NETWORK_H
#define SERVER_NETWORK_H

// Cross-platform includes
#ifdef _WIN32
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
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
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

#define SERVER_PORT 8080
#define MAX_MSG_SIZE 1024
#define MAX_NAME_LEN 50

typedef struct {
    socket_t client_fd;
    char name[MAX_NAME_LEN];
    int game_id;
    char symbol;
    struct sockaddr_in udp_addr;
    int is_active;
    thread_t thread;
} Client;

typedef struct {
    socket_t tcp_socket;
    socket_t udp_socket;
    struct sockaddr_in server_addr;
    int is_running;
} ServerNetwork;

int network_init(ServerNetwork *server);
int network_start_listening(ServerNetwork *server);
void network_shutdown(ServerNetwork *server);

Client* network_accept_client(ServerNetwork *server);
int network_send_to_client(Client *client, const char *message);
int network_receive_from_client(Client *client, char *buffer, size_t buf_size);

thread_return_t THREAD_CALL network_handle_client_thread(thread_param_t arg);
thread_return_t THREAD_CALL network_handle_udp_thread(thread_param_t arg);

// Cross-platform utility functions
int network_platform_init(void);
void network_platform_cleanup(void);
int network_set_socket_nonblocking(socket_t sock);
int create_thread(thread_t *thread, thread_return_t (THREAD_CALL *start_routine)(thread_param_t), void *arg, const char *thread_name);

// Cross-platform mutex functions implementation
#ifdef _WIN32
    int mutex_init(mutex_t *mutex) { 
        InitializeCriticalSection(mutex); 
        return 0; 
    }
    int mutex_lock(mutex_t *mutex) { 
        EnterCriticalSection(mutex); 
        return 0; 
    }
    int mutex_unlock(mutex_t *mutex) { 
        LeaveCriticalSection(mutex); 
        return 0; 
    }
    int mutex_destroy(mutex_t *mutex) { 
        DeleteCriticalSection(mutex); 
        return 0; 
    }
#else
    int mutex_init(mutex_t *mutex) { 
        return pthread_mutex_init(mutex, NULL); 
    }
    int mutex_lock(mutex_t *mutex) { 
        return pthread_mutex_lock(mutex); 
    }
    int mutex_unlock(mutex_t *mutex) { 
        return pthread_mutex_unlock(mutex); 
    }
    int mutex_destroy(mutex_t *mutex) { 
        return pthread_mutex_destroy(mutex); 
    }
#endif

#endif