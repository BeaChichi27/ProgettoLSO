#include "tris_server.h"
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>


extern server_state_t server_state;





int setup_server_socket(int port) {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;

    
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        log_error("Socket creation failed");
        return -1;
    }

    
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        log_error("Setsockopt failed");
        close(server_fd);
        return -1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        log_error("Bind failed");
        close(server_fd);
        return -1;
    }

    
    if (listen(server_fd, MAX_PLAYERS) < 0) {
        log_error("Listen failed");
        close(server_fd);
        return -1;
    }

    
    if (fcntl(server_fd, F_SETFL, O_NONBLOCK) < 0) {
        log_error("Failed to set non-blocking");
        close(server_fd);
        return -1;
    }

    log_message("Server socket setup completed");
    return server_fd;
}





void *handle_client(void *arg) {
    int sock = *(int *)arg;
    free(arg); 

    player_t *player = NULL;
    char buffer[MAX_MSG_LEN] = {0};
    int bytes_read;

    
    bytes_read = read(sock, buffer, MAX_MSG_LEN - 1);
    if (bytes_read <= 0) {
        log_error("Initial client read failed");
        close(sock);
        return NULL;
    }

    buffer[bytes_read] = '\0';
    trim_whitespace(buffer);

    
    if (!is_valid_name(buffer)) {
        send_response(sock, "ERR Invalid name\n");
        close(sock);
        return NULL;
    }

    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    getpeername(sock, (struct sockaddr *)&addr, &addr_len);

    player = add_player(sock, addr);
    if (!player) {
        send_response(sock, "ERR Server full\n");
        close(sock);
        return NULL;
    }

    strncpy(player->name, buffer, MAX_NAME_LEN - 1);
    player->state = PLAYER_IN_LOBBY;
    player->last_activity = get_current_time();

    send_response(sock, "OK Welcome to Tris Server\n");

    log_message("New player connected");

    
    while (1) {
        bytes_read = read(sock, buffer, MAX_MSG_LEN - 1);
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                log_message("Client disconnected");
            } else {
                log_error("Error reading from socket");
            }
            break;
        }

        buffer[bytes_read] = '\0';
        player->last_activity = get_current_time();

        
        process_client_command(player, buffer);
    }

    
    if (player) {
        remove_player(player);
    }
    close(sock);
    return NULL;
}

void process_client_command(player_t *player, const char *command) {
    if (!player || !command) return;

    char cmd[MAX_MSG_LEN];
    char arg[MAX_MSG_LEN];
    int n = sscanf(command, "%s %[^\n]", cmd, arg);

    if (n <= 0) {
        send_response(player->socket, "ERR Invalid command\n");
        return;
    }

    
    if (strcmp(cmd, "QUIT") == 0) {
        send_response(player->socket, "OK Goodbye\n");
        remove_player(player);
        close(player->socket);
        return;
    }
    else if (strcmp(cmd, "PING") == 0) {
        send_response(player->socket, "OK PONG\n");
        return;
    }

    
    switch (player->state) {
        case PLAYER_IN_LOBBY:
            handle_lobby_commands(player, cmd, n > 1 ? arg : NULL);
            break;
        case PLAYER_IN_GAME:
            handle_game_commands(player, cmd, n > 1 ? arg : NULL);
            break;
        case PLAYER_WAITING:
            send_response(player->socket, "WAIT Please wait for game to start\n");
            break;
        default:
            send_response(player->socket, "ERR Invalid state\n");
            break;
    }
}





int broadcast_game_state(game_t *game) {
    if (!game || !game->players[0] || !game->players[1]) {
        return -1;
    }

    char *board_str = format_board(game->board);
    if (!board_str) {
        return -1;
    }

    char message[512];
    snprintf(message, sizeof(message), 
             "GAME %d %c\n%s\n", 
             game->current_turn + 1,
             game->current_turn == 0 ? 'X' : 'O',
             board_str);

    int ret1 = send_response(game->players[0]->socket, message);
    int ret2 = send_response(game->players[1]->socket, message);

    free(board_str);
    return (ret1 == 0 && ret2 == 0) ? 0 : -1;
}

void notify_game_start(game_t *game) {
    if (!game) return;

    char message[256];
    snprintf(message, sizeof(message), 
             "START %d %s %s\n", 
             game->game_id,
             game->players[0]->name,
             game->players[1]->name);

    send_response(game->players[0]->socket, message);
    send_response(game->players[1]->socket, message);
}

void notify_game_end(game_t *game, int winner) {
    if (!game) return;

    char message[256];
    if (winner >= 0 && winner < 2) {
        snprintf(message, sizeof(message), 
                 "END WIN %s\n", 
                 game->players[winner]->name);
    } else {
        snprintf(message, sizeof(message), "END DRAW\n");
    }

    send_response(game->players[0]->socket, message);
    send_response(game->players[1]->socket, message);
}





void set_socket_nonblocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) {
        log_error("fcntl F_GETFL failed");
        return;
    }
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
        log_error("fcntl F_SETFL failed");
    }
}

void close_socket(int *sock) {
    if (sock && *sock != -1) {
        shutdown(*sock, SHUT_RDWR);
        close(*sock);
        *sock = -1;
    }
}