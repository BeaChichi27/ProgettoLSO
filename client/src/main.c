#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>       // Per signal() e SIGINT
#include <sys/time.h>     // Per struct timeval
#include <sys/types.h>    // Per fd_set e select

// CAMBIATO: Include cross-platform        
#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h>
    #include <conio.h>
#else
    #include <unistd.h>
    #include <termios.h>
    #include <sys/select.h>
#endif

#include "headers/network.h"
#include "headers/game_logic.h"
#include "headers/ui.h"
#include "headers/network.h"

static volatile int keep_running = 1;

// Aggiungi questo handler per SIGINT (Ctrl+C)
void handle_sigint(int sig) {
    (void)sig; // Ignora il segnale
    keep_running = 0;
}

void get_board_position(int move, int* row, int* col) {
    *row = (move - 1) / 3;
    *col = (move - 1) % 3;
}

void handle_create_game(NetworkConnection* conn) {
    if (!network_create_game(conn)) {
        ui_show_error(network_get_error());
        return;
    }

    ui_show_waiting_screen();
    time_t start_time = time(NULL);
    int received_pong = 1;
    
    while (keep_running) {
        // Controlla timeout
        if (difftime(time(NULL), start_time) > 300) { // 5 minuti
            ui_show_message("Timeout: nessun avversario trovato");
            network_send(conn, "CANCEL", 0);
            return;
        }
        
        // Ricevi messaggi dal server
        char message[MAX_MSG_SIZE];
        int bytes = network_receive(conn, message, sizeof(message), 0);
        
        if (bytes > 0) {
            if (strstr(message, "OPPONENT_JOINED")) {
                ui_show_message("Avversario trovato! La partita inizia...");
                return;
            }
            else if (strstr(message, "WAITING:")) {
                // Aggiorna schermata di attesa
                ui_show_waiting_screen_with_message(message + 8);
            }
            else if (strcmp(message, "PING") == 0) {
                network_send(conn, "PONG", 0);
            }
        }
        else if (bytes < 0) {
            ui_show_error("Connessione con il server interrotta");
            keep_running = 0;
            return;
        }
        
        // Controlla input utente
        if (kbhit()) {
            int key = getch();
            if (key == 27) { // ESC
                network_send(conn, "CANCEL", 0);
                return;
            }
        }
        
        sleep(1); // Riduci il carico sulla CPU
    }
}

void handle_join_game(NetworkConnection* conn) {
    ui_show_message("Richiesta lista partite...");
    network_send(conn, "LIST_GAMES", 0);
    
    char message[MAX_MSG_SIZE];
    if (network_receive(conn, message, sizeof(message), 0) <= 0) {
        ui_show_error(network_get_error());
        return;
    }
    
    ui_show_message(message);
    
    printf("Inserisci ID partita (0 per annullare): ");
    char input[10];
    if (fgets(input, sizeof(input), stdin) == NULL) {
        ui_show_error("Errore lettura input");
        return;
    }
    
    int game_id = atoi(input);
    if (game_id == 0) return;
    
    char join_msg[20];
    snprintf(join_msg, sizeof(join_msg), "JOIN:%d", game_id);
    network_send(conn, join_msg, 0);
    
    if (network_receive(conn, message, sizeof(message), 0) <= 0) {
        ui_show_error(network_get_error());
        return;
    }
    
    if (strstr(message, "JOIN_ACCEPTED")) {
        ui_show_message("Partita iniziata! Aspettando primo giocatore...");
    } else {
        ui_show_error("Impossibile unirsi alla partita");
    }
}

void game_loop(NetworkConnection* conn, Game* game, int is_host) {
    while (keep_running && game->state != GAME_STATE_OVER) {
        ui_show_board(game->board);
        
        char message[MAX_MSG_SIZE];
        int bytes = network_receive(conn, message, sizeof(message), 1);
        
        if (bytes > 0 && strstr(message, "SERVER_DISCONNECT")) {
            ui_show_error("Il server ti ha disconnesso");
            keep_running = 0;
            break;
        }

        if ((is_host && game->current_player == PLAYER_X) || 
            (!is_host && game->current_player == PLAYER_O)) {
            int move = ui_get_player_move();
            if (move == 0) {
                keep_running = 0;
                break;
            }
            
            int row, col;
            get_board_position(move, &row, &col);
            
            if (game_make_move(game, row, col)) {
                char move_msg[20];
                snprintf(move_msg, sizeof(move_msg), "MOVE:%d,%d", row, col);
                if (!network_send(conn, move_msg, 1)) {
                    ui_show_error("Errore nell'invio della mossa");
                    keep_running = 0;
                    break;
                }
            } else {
                ui_show_error("Mossa non valida");
            }
        } else {
            ui_show_message("Aspettando mossa avversario... (ESC per uscire)");
            
            int bytes = network_receive(conn, message, sizeof(message), 1);
            
            if (bytes == 0) {
                // Timeout, controlla se l'utente vuole uscire
                if (kbhit()) {
                    char key = getch();
                    if (key == 27) { // ESC
                        keep_running = 0;
                        break;
                    }
                }
                continue;
            } else if (bytes < 0) {
                ui_show_error(network_get_error());
                keep_running = 0;
                break;
            }
            
            if (bytes > 0 && strstr(message, "MOVE:")) {
                int row, col;
                char symbol;
                if (sscanf(message, "MOVE:%d,%d:%c", &row, &col, &symbol) == 3) {
                    if (row >= 0 && row < 3 && col >= 0 && col < 3) {
                        game->board[row][col] = symbol;
                        game_check_winner(game);
                        if (game->state == GAME_STATE_PLAYING) {
                            game->current_player = (game->current_player == PLAYER_X) ? PLAYER_O : PLAYER_X;
                        }
                    }
                }
            } else if (bytes > 0 && strstr(message, "SERVER_DOWN")) {
                ui_show_error("Il server si è disconnesso");
                keep_running = 0;
                break;
            }
        }
    }
    
    if (keep_running) {
        ui_show_board(game->board);
        
        if (game->winner != PLAYER_NONE) {
            char msg[50];
            snprintf(msg, sizeof(msg), "%c ha vinto!", game->winner);
            ui_show_message(msg);
        } else if (game->is_draw) {
            ui_show_message("Pareggio!");
        }
    }
}

int main() {
    // Registra l'handler per SIGINT
    signal(SIGINT, handle_sigint);

    if (!network_global_init()) {
        ui_show_error("Errore inizializzazione rete");
        return 1;
    }

    char player_name[50];
    if (!ui_get_player_name(player_name, sizeof(player_name))) {
        ui_show_error("Nome non valido");
        network_global_cleanup();
        return 1;
    }

    NetworkConnection conn;
    network_init(&conn);
    
    if (!network_connect_to_server(&conn)) {
        ui_show_error(network_get_error());
        network_global_cleanup();
        return 1;
    }
    
    if (!network_register_name(&conn, player_name)) {
        ui_show_error(network_get_error());
        network_disconnect(&conn);
        network_global_cleanup();
        return 1;
    }

    // Aggiungi questo per assicurarti che la registrazione sia andata a buon fine
    ui_clear_screen();
    printf("\nRegistrazione completata come %s\n", player_name);
    sleep(1); // Breve pausa per leggere il messaggio

    while (keep_running) {  // Modificato da while(1)
        int choice = ui_show_main_menu();
        
        switch (choice) {
            case 1: {
                handle_create_game(&conn);
                Game game;
                game_init(&game);
                game.state = GAME_STATE_PLAYING;
                
                while (keep_running && game.state != GAME_STATE_OVER) {
                    game_loop(&conn, &game, 1);
                    
                    if (game.state == GAME_STATE_OVER && keep_running) {
                        if (ui_ask_rematch()) {
                            network_send(&conn, "REMATCH", 0);
                            game.state = GAME_STATE_REMATCH;
                            game_reset(&game);
                        }
                    }
                }
                break;
            }
                
            case 2: {
                handle_join_game(&conn);
                Game game;
                game_init(&game);
                game.state = GAME_STATE_PLAYING;
                
                while (keep_running && game.state != GAME_STATE_OVER) {
                    game_loop(&conn, &game, 0);
                    
                    if (game.state == GAME_STATE_OVER && keep_running) {
                        if (ui_ask_rematch()) {
                            network_send(&conn, "REMATCH", 0);
                            game.state = GAME_STATE_REMATCH;
                            game_reset(&game);
                        }
                    }
                }
                break;
            }
                
            case 3:
                keep_running = 0;
                break;
        }
    }

    network_disconnect(&conn);
    network_global_cleanup();
    return 0;
}