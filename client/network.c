#include "tris_client.h"
#include "utils.h"

int initialize_client(client_context_t* ctx) {
    if (!ctx) {
        return -1;
    }
    
    
    memset(ctx, 0, sizeof(client_context_t));
    
    ctx->socket_fd = -1;
    ctx->state = STATE_DISCONNECTED;
    ctx->symbol = PLAYER_NONE;
    ctx->is_my_turn = 0;
    ctx->game_id = -1;
    
    
    initialize_board(ctx->board);
    
    return 0;
}

int connect_to_server(client_context_t* ctx) {
    if (!ctx) {
        return -1;
    }
    
    struct sockaddr_in server_addr;
    
    
    ctx->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ctx->socket_fd < 0) {
        perror("Errore creazione socket");
        return -1;
    }
    
    
    int opt = 1;
    
    
    if (setsockopt(ctx->socket_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) < 0) {
        perror("Errore setsockopt TCP_NODELAY");
        close(ctx->socket_fd);
        ctx->socket_fd = -1;
        return -1;
    }
    
    
    struct timeval timeout;
    timeout.tv_sec = TIMEOUT_SECONDS;
    timeout.tv_usec = 0;
    
    if (setsockopt(ctx->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("Errore setsockopt SO_RCVTIMEO");
        close(ctx->socket_fd);
        ctx->socket_fd = -1;
        return -1;
    }
    
    if (setsockopt(ctx->socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("Errore setsockopt SO_SNDTIMEO");
        close(ctx->socket_fd);
        ctx->socket_fd = -1;
        return -1;
    }
    
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        print_error("Indirizzo server non valido");
        close(ctx->socket_fd);
        ctx->socket_fd = -1;
        return -1;
    }
    
    
    if (connect(ctx->socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Errore connessione al server");
        close(ctx->socket_fd);
        ctx->socket_fd = -1;
        return -1;
    }
    
    ctx->state = STATE_MENU;
    return 0;
}

int send_message(client_context_t* ctx, const char* message) {
    if (!ctx || !message || ctx->socket_fd < 0) {
        return -1;
    }
    
    size_t len = strlen(message);
    if (len == 0 || len >= MAX_BUFFER) {
        return -1;
    }
    
    ssize_t sent = 0;
    ssize_t total_sent = 0;
    
    
    while (total_sent < (ssize_t)len) {
        sent = send(ctx->socket_fd, message + total_sent, len - total_sent, MSG_NOSIGNAL);
        
        if (sent < 0) {
            if (errno == EINTR) {
                continue; 
            }
            perror("Errore invio messaggio");
            return -1;
        }
        
        if (sent == 0) {
            print_error("Connessione chiusa dal server durante l'invio");
            return -1;
        }
        
        total_sent += sent;
    }
    
    return total_sent;
}

int receive_message(client_context_t* ctx, char* buffer, size_t size) {
    if (!ctx || !buffer || size == 0 || ctx->socket_fd < 0) {
        return -1;
    }
    
    memset(buffer, 0, size);
    
    ssize_t received = recv(ctx->socket_fd, buffer, size - 1, 0);
    
    if (received < 0) {
        if (errno == EINTR) {
            return 0; 
        }
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return 0; 
        }
        perror("Errore ricezione messaggio");
        return -1;
    }
    
    if (received == 0) {
        print_error("Server ha chiuso la connessione");
        return -1;
    }
    
    
    buffer[received] = '\0';
    
    
    trim_whitespace(buffer);
    
    return received;
}

int send_formatted_message(client_context_t* ctx, const char* format, ...) {
    if (!ctx || !format) {
        return -1;
    }
    
    char buffer[MAX_BUFFER];
    va_list args;
    
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    if (len < 0 || len >= (int)sizeof(buffer)) {
        print_error("Messaggio troppo lungo");
        return -1;
    }
    
    return send_message(ctx, buffer);
}

void cleanup_client(client_context_t* ctx) {
    if (!ctx) {
        return;
    }
    
    if (ctx->socket_fd >= 0) {
        
        if (ctx->state != STATE_DISCONNECTED) {
            send_message(ctx, MSG_CLIENT_QUIT);
        }
        
        
        if (close(ctx->socket_fd) < 0) {
            perror("Errore chiusura socket");
        }
        
        ctx->socket_fd = -1;
    }
    
    
    ctx->state = STATE_DISCONNECTED;
    ctx->symbol = PLAYER_NONE;
    ctx->is_my_turn = 0;
    ctx->game_id = -1;
    
    memset(ctx->player_name, 0, sizeof(ctx->player_name));
    memset(ctx->opponent_name, 0, sizeof(ctx->opponent_name));
    
    initialize_board(ctx->board);
}