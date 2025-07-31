#include "tris_server.h"
#include <signal.h>
#include <sys/select.h>


server_state_t server_state;

void init_server(void) {
    
    memset(&server_state, 0, sizeof(server_state_t));
    pthread_mutex_init(&server_state.lock, NULL);
    server_state.next_game_id = 1;

    log_message("Server initialized");
}

void cleanup_server(void) {
    
    pthread_mutex_lock(&server_state.lock);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (server_state.players[i]) {
            close(server_state.players[i]->socket);
            free(server_state.players[i]);
            server_state.players[i] = NULL;
        }
    }

    
    for (int i = 0; i < MAX_GAMES; i++) {
        if (server_state.games[i]) {
            free(server_state.games[i]);
            server_state.games[i] = NULL;
        }
    }
    pthread_mutex_unlock(&server_state.lock);

    pthread_mutex_destroy(&server_state.lock);
    log_message("Server cleanup completed");
}





void run_server(void) {
    int server_fd = setup_server_socket(SERVER_PORT);
    if (server_fd < 0) {
        log_error("Failed to setup server socket");
        exit(EXIT_FAILURE);
    }

    fd_set read_fds;
    int max_fd = server_fd;
    struct timeval timeout;

    log_message("Server started and listening for connections");

    while (1) {
        
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);

        
        pthread_mutex_lock(&server_state.lock);
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (server_state.players[i]) {
                FD_SET(server_state.players[i]->socket, &read_fds);
                if (server_state.players[i]->socket > max_fd) {
                    max_fd = server_state.players[i]->socket;
                }
            }
        }
        pthread_mutex_unlock(&server_state.lock);

        
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
        if (activity < 0 && errno != EINTR) {
            log_error("Select error");
            continue;
        }

        
        if (FD_ISSET(server_fd, &read_fds)) {
            handle_new_connection(server_fd);
        }

        
        check_inactive_players();

        
        cleanup_finished_games();
    }

    close(server_fd);
}

void handle_new_connection(int server_fd) {
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    int new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);

    if (new_socket < 0) {
        log_error("Accept failed");
        return;
    }

    
    pthread_t thread_id;
    int *sock_ptr = malloc(sizeof(int));
    *sock_ptr = new_socket;

    if (pthread_create(&thread_id, NULL, handle_client, (void *)sock_ptr) != 0) {
        log_error("Failed to create client thread");
        close(new_socket);
        free(sock_ptr);
        return;
    }

    pthread_detach(thread_id);
    log_message("New connection accepted");
}





player_t *add_player(int sock, struct sockaddr_in addr) {
    pthread_mutex_lock(&server_state.lock);

    
    int free_slot = -1;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!server_state.players[i]) {
            free_slot = i;
            break;
        }
    }

    if (free_slot == -1) {
        pthread_mutex_unlock(&server_state.lock);
        return NULL;
    }

    
    player_t *new_player = malloc(sizeof(player_t));
    if (!new_player) {
        pthread_mutex_unlock(&server_state.lock);
        return NULL;
    }

    memset(new_player, 0, sizeof(player_t));
    new_player->socket = sock;
    new_player->addr = addr;
    new_player->state = PLAYER_IN_LOBBY;
    new_player->game_id = -1;
    new_player->last_activity = get_current_time();

    server_state.players[free_slot] = new_player;
    pthread_mutex_unlock(&server_state.lock);

    return new_player;
}

void remove_player(player_t *player) {
    if (!player) return;

    pthread_mutex_lock(&server_state.lock);

    
    if (player->game_id != -1) {
        for (int i = 0; i < MAX_GAMES; i++) {
            if (server_state.games[i] && server_state.games[i]->game_id == player->game_id) {
                if (server_state.games[i]->players[0] == player) {
                    server_state.games[i]->players[0] = NULL;
                } else if (server_state.games[i]->players[1] == player) {
                    server_state.games[i]->players[1] = NULL;
                }

                
                if (server_state.games[i]->state == GAME_ACTIVE) {
                    server_state.games[i]->state = GAME_OVER;
                    notify_game_end(server_state.games[i], -1); 
                }
                break;
            }
        }
    }

    
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (server_state.players[i] == player) {
            free(server_state.players[i]);
            server_state.players[i] = NULL;
            break;
        }
    }

    pthread_mutex_unlock(&server_state.lock);
    log_message("Player removed");
}

void check_inactive_players(void) {
    time_t now = get_current_time();
    pthread_mutex_lock(&server_state.lock);

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (server_state.players[i] && 
            (now - server_state.players[i]->last_activity) > INACTIVITY_TIMEOUT) {
            
            log_message("Disconnecting inactive player");
            close(server_state.players[i]->socket);
            remove_player(server_state.players[i]);
        }
    }

    pthread_mutex_unlock(&server_state.lock);
}

void cleanup_finished_games(void) {
    pthread_mutex_lock(&server_state.lock);
    time_t now = get_current_time();

    for (int i = 0; i < MAX_GAMES; i++) {
        if (server_state.games[i] && server_state.games[i]->state == GAME_OVER) {
            
            if ((now - server_state.games[i]->start_time) > 30) {
                free(server_state.games[i]);
                server_state.games[i] = NULL;
            }
        }
    }

    pthread_mutex_unlock(&server_state.lock);
}





void handle_signal(int sig) {
    switch (sig) {
        case SIGINT:
        case SIGTERM:
            log_message("Received shutdown signal");
            cleanup_server();
            exit(EXIT_SUCCESS);
            break;
        default:
            break;
    }
}

int main(int argc, char *argv[]) {
    
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    init_server();
    run_server(); 

    return 0;
}