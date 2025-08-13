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

PlayerSymbol handle_create_game(NetworkConnection* conn) {
    PlayerSymbol assigned_symbol = PLAYER_NONE;
    char message[MAX_MSG_SIZE];
    int bytes;
    int game_started = 0;
    time_t start_time = time(NULL);

    if (!network_create_game(conn)) {
        ui_show_error(network_get_error());
        return PLAYER_NONE;
    }

    printf("Richiesta creazione partita inviata...\n");
    
    // Ricevi la conferma di creazione
    bytes = network_receive(conn, message, sizeof(message), 0);
    if (bytes <= 0) {
        ui_show_error("Nessuna risposta dal server");
        return PLAYER_NONE;
    }
    
    printf("Risposta server: %s\n", message);
    
    if (!strstr(message, "GAME_CREATED:")) {
        ui_show_error("Errore creazione partita");
        return PLAYER_NONE;
    }
    
    // Ricevi messaggio WAITING_OPPONENT
    bytes = network_receive(conn, message, sizeof(message), 0);
    if (bytes <= 0) {
        ui_show_error("Errore ricezione stato attesa");
        return PLAYER_NONE;
    }
    
    printf("Stato attesa: %s\n", message);
    ui_show_waiting_screen();
    
    while (keep_running && !game_started) {
        // Controlla timeout (5 minuti)
        if (difftime(time(NULL), start_time) > 300) {
            ui_show_message("Timeout: nessun avversario trovato");
            network_send(conn, "CANCEL", 0);
            return PLAYER_NONE;
        }
        
        // Configura select per controllare sia socket che input utente
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(conn->tcp_sock, &read_fds);
        
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        int select_result;
#ifdef _WIN32
        select_result = select(0, &read_fds, NULL, NULL, &tv);
#else
        select_result = select(conn->tcp_sock + 1, &read_fds, NULL, NULL, &tv);
#endif

        if (select_result > 0 && FD_ISSET(conn->tcp_sock, &read_fds)) {
            bytes = network_receive(conn, message, sizeof(message), 0);
            
            if (bytes > 0) {
                printf("Messaggio ricevuto durante attesa: %s\n", message);
                
                if (strstr(message, "OPPONENT_JOINED")) {
                    printf("Avversario si è unito!\n");
                    continue; // Aspetta GAME_START
                }
                else if (strstr(message, "GAME_START:")) {
                    char symbol;
                    if (sscanf(message, "GAME_START:%c", &symbol) == 1) {
                        assigned_symbol = (PlayerSymbol)symbol;
                        printf("Partita iniziata! Il tuo simbolo è: %c\n", assigned_symbol);
                        ui_clear_screen();
                        ui_show_message("Avversario trovato! La partita inizia...");
                        game_started = 1;
                        break;
                    }
                }
                else if (strcmp(message, "PING") == 0) {
                    network_send(conn, "PONG", 0);
                }
                else if (strstr(message, "ERROR:")) {
                    ui_show_error(message);
                    return PLAYER_NONE;
                }
            }
            else if (bytes < 0) {
                ui_show_error("Connessione con il server interrotta");
                return PLAYER_NONE;
            }
        }
        else if (select_result == -1) {
#ifdef _WIN32
            ui_show_error("Errore nella connessione");
#else
            ui_show_error(strerror(errno));
#endif
            return PLAYER_NONE;
        }
        
        // Controlla input utente per cancellazione
        if (kbhit()) {
            int key = getch();
            if (key == 27) { // ESC
                printf("Cancellazione richiesta dall'utente\n");
                network_send(conn, "CANCEL", 0);
                return PLAYER_NONE;
            }
        }
        
        // Aggiorna schermo di attesa periodicamente
        static time_t last_update = 0;
        time_t now = time(NULL);
        if (difftime(now, last_update) >= 5) {
            ui_show_waiting_screen();
            printf("In attesa di avversario... (ESC per annullare)\n");
            last_update = now;
        }
    }
    
    if (!game_started) {
        ui_show_error("Partita non avviata");
        return PLAYER_NONE;
    }

    return assigned_symbol;
}

PlayerSymbol handle_join_game(NetworkConnection* conn) {
    PlayerSymbol assigned_symbol = PLAYER_NONE;
    char message[MAX_MSG_SIZE];
    int bytes;
    
    // Richiedi la lista delle partite disponibili
    ui_show_message("Richiesta lista partite...");
    if (!network_send(conn, "LIST_GAMES", 0)) {
        ui_show_error("Errore nell'invio richiesta lista partite");
        return PLAYER_NONE;
    }

    // Ricevi la lista delle partite
    bytes = network_receive(conn, message, sizeof(message), 0);
    if (bytes <= 0) {
        ui_show_error(network_get_error());
        return PLAYER_NONE;
    }
    
    printf("Lista partite ricevuta: %s\n", message);
    
    // Verifica che sia davvero una lista di partite
    if (!strstr(message, "GAMES:")) {
        ui_show_error("Risposta server non valida per lista partite");
        return PLAYER_NONE;
    }
    
    ui_clear_screen();
    ui_show_message(message);
    
    // Controlla se ci sono partite disponibili
    if (strstr(message, "Nessuna partita disponibile")) {
        ui_show_message("Nessuna partita disponibile al momento");
        return PLAYER_NONE;
    }
    
    // Ottieni l'ID della partita dall'utente
    printf("\nInserisci ID partita (0 per annullare): ");
    fflush(stdout);
    
    char input[10];
    if (fgets(input, sizeof(input), stdin) == NULL) {
        ui_show_error("Errore lettura input");
        return PLAYER_NONE;
    }
    
    int game_id = atoi(input);
    if (game_id == 0) {
        printf("Operazione annullata\n");
        return PLAYER_NONE;
    }
    
    // Invia la richiesta di join
    char join_msg[32];
    snprintf(join_msg, sizeof(join_msg), "JOIN:%d", game_id);
    printf("Invio richiesta: %s\n", join_msg);
    if (!network_send(conn, join_msg, 0)) {
        ui_show_error("Errore nell'invio richiesta join");
        return PLAYER_NONE;
    }

    printf("Richiesta inviata, in attesa di risposta...\n");
    ui_show_message("Connessione alla partita in corso...");
    
    // Loop per gestire tutte le possibili risposte
    time_t start_time = time(NULL);
    while (keep_running) {
        // Timeout dopo 30 secondi
        if (difftime(time(NULL), start_time) > 30) {
            ui_show_error("Timeout: impossibile unirsi alla partita");
            return PLAYER_NONE;
        }
        
        bytes = network_receive(conn, message, sizeof(message), 0);
        if (bytes <= 0) {
            ui_show_error("Errore ricezione risposta");
            return PLAYER_NONE;
        }
        
        printf("Risposta ricevuta: %s\n", message);
        
        // Gestisci tutti i tipi di risposta
        if (strstr(message, "ERROR:")) {
            ui_show_error(message);
            return PLAYER_NONE;
        }
        else if (strstr(message, "JOIN_ACCEPTED")) {
            printf("Join accettato! In attesa del messaggio GAME_START...\n");
            ui_show_message("Unito alla partita! In attesa di inizio...");
            continue; // Aspetta il GAME_START
        }
        else if (strstr(message, "GAME_START:")) {
            char symbol;
            if (sscanf(message, "GAME_START:%c", &symbol) == 1) {
                assigned_symbol = (PlayerSymbol)symbol;
                printf("Partita iniziata! Il tuo simbolo è: %c\n", assigned_symbol);
                ui_clear_screen();
                ui_show_message("Partita iniziata!");
                return assigned_symbol;
            }
        }
        else if (strcmp(message, "PING") == 0) {
            network_send(conn, "PONG", 0);
            continue;
        }
        else if (strstr(message, "OPPONENT_LEFT") || strstr(message, "GAME_CANCELLED")) {
            ui_show_error(message);
            return PLAYER_NONE;
        }
        else {
            printf("Messaggio non riconosciuto durante JOIN: %s\n", message);
            continue; // Continua ad aspettare
        }
    }
    
    return PLAYER_NONE;
}


void game_loop(NetworkConnection* conn, Game* game, PlayerSymbol player_symbol) {
    printf("Iniziando game loop. Il tuo simbolo è: %c\n", player_symbol);
    
    // Inizializza il gioco
    game->state = GAME_STATE_PLAYING;
    game->current_player = PLAYER_X; // Il primo giocatore è sempre X
    
    while (keep_running && game->state != GAME_STATE_OVER) {
        ui_show_board(game->board);
        
        // Determina di chi è il turno
        int is_my_turn = (game->current_player == player_symbol);
        
        if (is_my_turn) {
            printf("\n=== IL TUO TURNO ===\n");
            printf("Simbolo corrente: %c (il tuo: %c)\n", game->current_player, player_symbol);
            
            int move = ui_get_player_move();
            if (move == 0) {
                printf("Uscita richiesta dall'utente\n");
                keep_running = 0;
                break;
            }
            
            int row, col;
            get_board_position(move, &row, &col);
            printf("Mossa selezionata: cella %d -> riga %d, colonna %d\n", move, row, col);
            
            // Verifica se la mossa è valida localmente
            if (!game_is_valid_move(game, row, col)) {
                ui_show_error("Mossa non valida! Cella già occupata.");
                continue;
            }
            
            // Invia la mossa via TCP
            char move_msg[32];
            snprintf(move_msg, sizeof(move_msg), "MOVE:%d,%d", row, col);
            printf("Invio mossa: %s\n", move_msg);
            
            if (!network_send(conn, move_msg, 0)) {
                ui_show_error("Errore nell'invio della mossa");
                keep_running = 0;
                break;
            }
            
            printf("Mossa inviata, in attesa di risposta...\n");
            
            // Attendi conferma o aggiornamento del gioco
            char message[MAX_MSG_SIZE];
            int bytes = network_receive(conn, message, sizeof(message), 0);
            
            if (bytes <= 0) {
                ui_show_error("Errore ricezione risposta mossa");
                keep_running = 0;
                break;
            }
            
            printf("Risposta server: %s\n", message);
            
            // Processa la risposta
            if (strstr(message, "ERROR:")) {
                ui_show_error(message);
                continue;  // Riprova
            }
            else if (strstr(message, "MOVE:")) {
                // Aggiorna il board e controlla lo stato del gioco
                if (game_process_network_message(game, message)) {
                    printf("Board aggiornato dopo la mossa\n");
                } else {
                    printf("Errore nell'aggiornamento del board\n");
                }
            }
            else if (strstr(message, "GAME_OVER:")) {
                if (game_process_network_message(game, message)) {
                    printf("Partita terminata!\n");
                    break;
                }
            }
        } 
        else {
            printf("\n=== TURNO AVVERSARIO ===\n");
            printf("Simbolo corrente: %c (il tuo: %c)\n", game->current_player, player_symbol);
            printf("Aspettando mossa avversario... (ESC per uscire)\n");
            
            // Configura select per controllare sia socket che input utente
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(conn->tcp_sock, &read_fds);
            
            struct timeval tv;
            tv.tv_sec = 1;  // Timeout di 1 secondo
            tv.tv_usec = 0;
            
            int select_result;
#ifdef _WIN32
            select_result = select(0, &read_fds, NULL, NULL, &tv);
#else
            select_result = select(conn->tcp_sock + 1, &read_fds, NULL, NULL, &tv);
#endif

            if (select_result > 0 && FD_ISSET(conn->tcp_sock, &read_fds)) {
                char message[MAX_MSG_SIZE];
                int bytes = network_receive(conn, message, sizeof(message), 0);
                
                if (bytes <= 0) {
                    ui_show_error("Connessione persa");
                    keep_running = 0;
                    break;
                }
                
                printf("Messaggio ricevuto durante attesa: %s\n", message);
                
                if (strstr(message, "MOVE:")) {
                    // L'avversario ha fatto una mossa
                    if (game_process_network_message(game, message)) {
                        printf("Mossa avversario processata\n");
                        continue;  // Torna al loop per mostrare il board aggiornato
                    }
                }
                else if (strstr(message, "GAME_OVER:")) {
                    if (game_process_network_message(game, message)) {
                        printf("Partita terminata!\n");
                        break;
                    }
                }
                else if (strstr(message, "OPPONENT_LEFT")) {
                    ui_show_message("L'avversario ha abbandonato la partita");
                    keep_running = 0;
                    break;
                }
                else if (strcmp(message, "PING") == 0) {
                    network_send(conn, "PONG", 0);
                }
            }
            else if (select_result == -1) {
                ui_show_error("Errore di connessione");
                keep_running = 0;
                break;
            }
            
            // Controlla input utente
            if (kbhit()) {
                int key = getch();
                if (key == 27) { // ESC
                    printf("Uscita richiesta dall'utente\n");
                    network_send(conn, "LEAVE", 0);
                    keep_running = 0;
                    break;
                }
            }
        }
    }
    
    // Mostra il risultato finale
    if (keep_running && game->state == GAME_STATE_OVER) {
        ui_show_board(game->board);
        
        if (game->winner != PLAYER_NONE) {
            if (game->winner == player_symbol) {
                ui_show_message("HAI VINTO!");
            } else {
                ui_show_message("HAI PERSO!");
            }
            char msg[50];
            snprintf(msg, sizeof(msg), "Vincitore: %c", game->winner);
            printf("%s\n", msg);
        } else if (game->is_draw) {
            ui_show_message("PAREGGIO!");
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
    
    // Registra il nome (rimosso il doppio registro)
    if (!network_register_name(&conn, player_name)) {
        ui_show_error(network_get_error());
        network_disconnect(&conn);
        network_global_cleanup();
        return 1;
    }

    ui_clear_screen();
    printf("\nRegistrazione completata come %s\n", player_name);
    sleep(1);

    // Avvia keep-alive dopo la registrazione
    network_start_keep_alive(&conn);

    while (keep_running) {
        int choice = ui_show_main_menu();
        
        switch (choice) {
            case 1: {  // Crea partita
                PlayerSymbol my_symbol = handle_create_game(&conn);
                if (my_symbol == PLAYER_NONE) break;

                Game game;
                game_init(&game);
                game.state = GAME_STATE_PLAYING;
                game.current_player = PLAYER_X;  // Il server dovrebbe gestire chi inizia
                
                while (keep_running && game.state != GAME_STATE_OVER) {
                    game_loop(&conn, &game, my_symbol);
                    
                    if (game.state == GAME_STATE_OVER && keep_running) {
                        if (ui_ask_rematch()) {
                            network_send(&conn, "REMATCH", 0);
                            game.state = GAME_STATE_REMATCH;
                            game_reset(&game);
                            // Aspetta conferma rematch dal server
                            char response[MAX_MSG_SIZE];
                            if (network_receive(&conn, response, sizeof(response), 0) > 0) {
                                if (strstr(response, "REMATCH_ACCEPTED")) {
                                    my_symbol = (strstr(response, "X") ? PLAYER_X : PLAYER_O);
                                }
                            }
                        }
                    }
                }
                break;
            }
                
            case 2: {  // Unisciti a partita
                PlayerSymbol my_symbol = handle_join_game(&conn);
                if (my_symbol == PLAYER_NONE) break;

                Game game;
                game_init(&game);
                game.state = GAME_STATE_PLAYING;
                game.current_player = PLAYER_X;  // Il server dovrebbe gestire chi inizia
                
                while (keep_running && game.state != GAME_STATE_OVER) {
                    game_loop(&conn, &game, my_symbol);
                    
                    if (game.state == GAME_STATE_OVER && keep_running) {
                        if (ui_ask_rematch()) {
                            network_send(&conn, "REMATCH", 0);
                            game.state = GAME_STATE_REMATCH;
                            game_reset(&game);
                            // Aspetta conferma rematch dal server
                            char response[MAX_MSG_SIZE];
                            if (network_receive(&conn, response, sizeof(response), 0) > 0) {
                                if (strstr(response, "REMATCH_ACCEPTED")) {
                                    my_symbol = (strstr(response, "X") ? PLAYER_X : PLAYER_O);
                                }
                            }
                        }
                    }
                }
                break;
            }
                
            case 3:  // Esci
                keep_running = 0;
                break;
                
            default:
                ui_show_error("Scelta non valida");
                break;
        }
    }

    network_stop_keep_alive(&conn);
    network_disconnect(&conn);
    network_global_cleanup();
    return 0;
}