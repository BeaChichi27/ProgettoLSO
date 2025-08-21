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

#define MAX_NAME_LEN 50

static volatile int keep_running = 1;

// Funzione per pulire il buffer stdin
void clear_stdin_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

// Funzione per gestire la fine della partita e il rematch
int handle_game_end(NetworkConnection *conn, Game *game, PlayerSymbol player_symbol) {
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
        
        // Domanda semplice come da traccia: vuoi iniziare un'altra partita?
        printf("\nVuoi iniziare un'altra partita? (s/n): ");
        
        char response;
        scanf(" %c", &response);
        
        // Pulisce il buffer stdin dopo l'input dell'utente
        clear_stdin_buffer();
        
        if (response == 's' || response == 'S') {
            // L'utente vuole giocare un'altra partita (rivincita con stesso ID)
            printf("Richiedendo un'altra partita...\n");
            if (network_request_rematch(conn)) {
                printf("Richiesta inviata. In attesa dell'avversario...\n");
                
                // Aspetta la risposta del rematch con timeout
                int rematch_handled = 0;
                time_t start_time = time(NULL);
                
                while (!rematch_handled && difftime(time(NULL), start_time) < 30) {
                    char message[MAX_MSG_SIZE];
                    int bytes = network_receive(conn, message, sizeof(message), 0);
                    
                    if (bytes > 0) {
                        if (strstr(message, "REMATCH_ACCEPTED:")) {
                            printf("Altra partita accettata! Nuova partita inizia...\n");
                            
                            // Controlla se GAME_START è incluso nello stesso messaggio
                            char *game_start_pos = strstr(message, "GAME_START:");
                            if (game_start_pos) {
                                printf("GAME_START trovato nel messaggio combinato: %s\n", message);
                                char new_symbol;
                                if (sscanf(game_start_pos, "GAME_START:%c", &new_symbol) == 1) {
                                    // Nota: player_symbol è passato per valore, non possiamo modificarlo
                                    // Questa modifica dovrebbe essere gestita dalla funzione chiamante
                                    printf("Nuovo simbolo da assegnare: %c\n", new_symbol);
                                }
                            } else {
                                // GAME_START arriva separatamente
                                char start_msg[MAX_MSG_SIZE];
                                int start_bytes = network_receive(conn, start_msg, sizeof(start_msg), 0);
                                if (start_bytes > 0 && strstr(start_msg, "GAME_START:")) {
                                    printf("Nuovo GAME_START ricevuto: %s\n", start_msg);
                                    char new_symbol;
                                    if (sscanf(start_msg, "GAME_START:%c", &new_symbol) == 1) {
                                        // Nota: player_symbol è passato per valore, non possiamo modificarlo
                                        // Questa modifica dovrebbe essere gestita dalla funzione chiamante
                                        printf("Nuovo simbolo da assegnare: %c\n", new_symbol);
                                    }
                                }
                            }
                            
                            // Reset del gioco per l'altra partita
                            game_init_board(game);
                            game->state = GAME_STATE_PLAYING;
                            game->current_player = PLAYER_X;
                            game->winner = PLAYER_NONE;
                            game->is_draw = 0;
                            
                             // Pulisce il buffer stdin per evitare input residui
                            clear_stdin_buffer();

                            // Mostra la board vuota per iniziare la nuova partita
                            printf("=== NUOVA PARTITA INIZIATA ===\n");
                            printf("\nPremere INVIO per proseguire...");

                            char c= getc(stdin); // Aspetta che l'utente prema INVIO
                            if(c=='\n'){
                                ui_show_board(game->board);
                            }
                            rematch_handled = 1;
                            return 1; // Continua il gioco - altra partita iniziata
                        } 
                        else if (strstr(message, "REMATCH_REQUEST:")) {
                            printf("L'avversario ha richiesto un'altra partita.\n");
                            printf("Accetti di giocare un'altra partita? (s/n): ");
                            char response_to_request;
                            scanf(" %c", &response_to_request);
                            
                            if (response_to_request == 's' || response_to_request == 'S') {
                                network_request_rematch(conn);
                                printf("Altra partita accettata! In attesa di iniziare...\n");
                                
                                // Pulisce il buffer stdin dopo l'input dell'utente
                                clear_stdin_buffer();
                                
                                // Continua nel loop per ricevere REMATCH_ACCEPTED
                            } else {
                                printf("Altra partita rifiutata.\n");
                                rematch_handled = 1;
                                return 0; // Esce dal gioco - altra partita rifiutata
                            }
                        }
                        else if (strstr(message, "REMATCH_SENT:")) {
                            printf("In attesa che l'avversario risponda alla richiesta...\n");
                        }
                        else if (strstr(message, "NEW_GAME_REQUEST")) {
                            // Questo non dovrebbe più essere usato, ma lo gestiamo per compatibilità
                            printf("L'avversario vuole iniziare un'altra partita.\n");
                            printf("Tornando al menu principale...\n");
                            rematch_handled = 1;
                            return 0; // Esce dal gioco
                        }
                        else if (strstr(message, "REMATCH_CANCELLED:")) {
                            printf("La richiesta è stata cancellata: %s\n", message + 17);
                            printf("Tornando al menu principale...\n");
                            rematch_handled = 1;
                            return 0; // Esce dal gioco - richiesta cancellata
                        }
                        else if (strstr(message, "REMATCH_DECLINED:")) {
                            printf("L'avversario ha rifiutato l'altra partita.\n");
                            printf("Tornando al menu principale...\n");
                            rematch_handled = 1;
                            return 0; // Esce dal gioco - rematch rifiutato dall'avversario
                        }
                        else if (strstr(message, "OPPONENT_LEFT:")) {
                            printf("L'avversario ha abbandonato la partita: %s\n", message + 14);
                            printf("Tornando al menu principale...\n");
                            rematch_handled = 1;
                            return 0; // Esce dal gioco - avversario disconnesso
                        }
                        else if (strstr(message, "ERROR:")) {
                            printf("Errore: %s\n", message);
                            rematch_handled = 1;
                            return 0; // Esce dal gioco - errore
                        }
                        else if (strstr(message, "SERVER_SHUTDOWN:")) {
                            printf("Server in spegnimento: %s\n", message + 16);
                            printf("Connessione interrotta.\n");
                            keep_running = 0; // Termina completamente il client
                            rematch_handled = 1;
                            return 0;
                        }
                    }
                    else {
                        sleep(1); // Aspetta 1 secondo prima di riprovare
                    }
                }
                
                if (!rematch_handled) {
                    printf("Timeout - l'avversario non ha risposto.\n");
                    return 0; // Esce dal gioco - timeout
                }
            } else {
                ui_show_error("Errore nell'invio richiesta");
                return 0; // Esce dal gioco - errore invio
            }
        } else if (response == 'n' || response == 'N') {
            // L'utente non vuole giocare un'altra partita - notifica il server
            printf("Rifiutando altra partita...\n");
            network_send(conn, "REMATCH_DECLINE", 0); // Invia il rifiuto al server
            printf("Partita terminata. Tornando al menu principale...\n");
            return 0; // Esce dal gioco
        } else {
            // Risposta non valida
            printf("Risposta non valida. Tornando al menu principale...\n");
            return 0; // Esce dal gioco per tornare al menu
        }
    }
    
    return 0; // Default - esce dal gioco
}

// Aggiungi questo handler per SIGINT (Ctrl+C)
void handle_sigint(int sig) {
    (void)sig; // Ignora il segnale
    printf("\nRicevuto segnale di interruzione...\n");
    // network_stop_receiver();  // Disabilitato temporaneamente
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

    printf("[DEBUG] Richiesta creazione partita inviata...\n");
    fflush(stdout);
    
    // Ricevi la conferma di creazione
    printf("[DEBUG] Attendo risposta GAME_CREATED...\n");
    fflush(stdout);
    bytes = network_receive(conn, message, sizeof(message), 0);
    if (bytes <= 0) {
        printf("[DEBUG] ERRORE: bytes ricevuti=%d\n", bytes);
        ui_show_error("Nessuna risposta dal server");
        return PLAYER_NONE;
    }
    
    printf("[DEBUG] Risposta server: %s\n", message);
    fflush(stdout);
    
    if (!strstr(message, "GAME_CREATED:")) {
        printf("[DEBUG] ERRORE: Messaggio non contiene GAME_CREATED: %s\n", message);
        ui_show_error("Errore creazione partita");
        return PLAYER_NONE;
    }
    
    // Controlla se il messaggio contiene anche WAITING_OPPONENT
    if (strstr(message, "WAITING_OPPONENT")) {
        printf("[DEBUG] GAME_CREATED e WAITING_OPPONENT ricevuti insieme\n");
        // Già abbiamo tutto quello che ci serve
    } else {
        // Ricevi messaggio WAITING_OPPONENT separato
        printf("[DEBUG] Attendo messaggio WAITING_OPPONENT separato...\n");
        fflush(stdout);
        bytes = network_receive(conn, message, sizeof(message), 0);
        if (bytes <= 0) {
            printf("[DEBUG] ERRORE: bytes ricevuti per WAITING_OPPONENT=%d\n", bytes);
            ui_show_error("Errore ricezione stato attesa");
            return PLAYER_NONE;
        }
        
        printf("[DEBUG] Stato attesa: %s\n", message);
        fflush(stdout);
    }
    ui_show_waiting_screen();
    
retry_waiting:
    while (keep_running && !game_started) {
        // Controlla timeout (5 minuti)
        if (difftime(time(NULL), start_time) > 300) {
            ui_show_message("Timeout: nessun avversario trovato");
            network_send(conn, "CANCEL", 0);
            return PLAYER_NONE;
        }
        
        // Controlla sempre se ci sono messaggi in attesa prima di fare select
        fd_set pre_check;
        FD_ZERO(&pre_check);
        FD_SET(conn->tcp_sock, &pre_check);
        struct timeval instant = {0, 0}; // Check immediato
        
#ifdef _WIN32
        int pre_result = select(0, &pre_check, NULL, NULL, &instant);
#else
        int pre_result = select(conn->tcp_sock + 1, &pre_check, NULL, NULL, &instant);
#endif
        
        if (pre_result > 0) {
            printf(">>> MESSAGGIO IMMEDIATAMENTE DISPONIBILE!\n");
            bytes = network_receive(conn, message, sizeof(message), 0);
            if (bytes > 0) {
                printf(">>> Messaggio ricevuto (pre-check): %s\n", message);
                goto process_message;
            }
        }
        
        // Configura select per controllare sia socket che input utente
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(conn->tcp_sock, &read_fds);
        
        struct timeval tv;
        tv.tv_sec = 0;  // Timeout ancora più breve per massima reattività
        tv.tv_usec = 100000; // 100ms
        
        int select_result;
#ifdef _WIN32
        select_result = select(0, &read_fds, NULL, NULL, &tv);
#else
        select_result = select(conn->tcp_sock + 1, &read_fds, NULL, NULL, &tv);
#endif

        if (select_result > 0 && FD_ISSET(conn->tcp_sock, &read_fds)) {
            bytes = network_receive(conn, message, sizeof(message), 0);
            
            if (bytes > 0) {
                printf(">>> Messaggio ricevuto: %s\n", message);
                goto process_message; // Vai alla sezione di processing
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
        
        // Aggiorna schermo di attesa periodicamente e controlla connessione
        static time_t last_update = 0;
        time_t now = time(NULL);
        if (difftime(now, last_update) >= 3) {
            ui_show_waiting_screen();
            printf("In attesa di avversario... (ESC per annullare)\n");
            printf(">>> Controllo attivo messaggi dal server...\n");
            last_update = now;
            
            // Controllo aggiuntivo per messaggi in arrivo senza select
            fd_set quick_check;
            FD_ZERO(&quick_check);
            FD_SET(conn->tcp_sock, &quick_check);
            struct timeval quick_tv = {0, 0}; // Non-blocking
            
#ifdef _WIN32
            int quick_result = select(0, &quick_check, NULL, NULL, &quick_tv);
#else
            int quick_result = select(conn->tcp_sock + 1, &quick_check, NULL, NULL, &quick_tv);
#endif
            if (quick_result > 0) {
                printf(">>> MESSAGGIO DISPONIBILE! Leggendo...\n");
                bytes = network_receive(conn, message, sizeof(message), 0);
                if (bytes > 0) {
                    printf(">>> Messaggio trovato nel controllo aggiuntivo: %s\n", message);
                    goto process_message; // Vai alla sezione di processing
                }
            }
        }
    }
    
    if (!game_started) {
        ui_show_error("Partita non avviata");
        return PLAYER_NONE;
    }

    return assigned_symbol;

continue_waiting:
    // Ritorna al loop di attesa
    goto retry_waiting;

process_message:
    // Sezione per processare i messaggi ricevuti
    if (strstr(message, "JOIN_REQUEST:")) {
        // Richiesta di join da un altro giocatore
        char player_name[MAX_NAME_LEN];
        int game_id;
        if (sscanf(message, "JOIN_REQUEST:%49[^:]:%d", player_name, &game_id) == 2) {
            printf("\n=== RICHIESTA DI JOIN ===\n");
            printf("Il giocatore '%s' vuole unirsi alla tua partita.\n", player_name);
            printf("Accetti? (s/n): ");
            fflush(stdout);
            
            char response;
            scanf(" %c", &response);
            
            if (response == 's' || response == 'S') {
                network_send(conn, "APPROVE:1", 0);
                printf("Richiesta approvata. In attesa che il gioco inizi...\n");
            } else {
                network_send(conn, "APPROVE:0", 0);
                printf("Richiesta rifiutata. In attesa di altri giocatori...\n");
            }
        }
        // Torna al loop di attesa
        goto continue_waiting;
    }
    else if (strstr(message, "JOIN_APPROVED_BY_YOU")) {
        printf("Hai approvato il join. La partita sta iniziando...\n");
        goto continue_waiting;
    }
    else if (strstr(message, "JOIN_REJECTED_BY_YOU")) {
        printf("Hai rifiutato il join. In attesa di altri giocatori...\n");
        goto continue_waiting;
    }
    else if (strstr(message, "JOIN_CANCELLED:")) {
        printf("Il giocatore ha annullato la richiesta di join.\n");
        goto continue_waiting;
    }
    else if (strstr(message, "OPPONENT_JOINED")) {
        printf("Avversario si è unito!\n");
        goto continue_waiting; // Aspetta GAME_START
    }
    else if (strstr(message, "GAME_START:")) {
        char symbol;
        if (sscanf(message, "GAME_START:%c", &symbol) == 1) {
            assigned_symbol = (PlayerSymbol)symbol;
            printf("Partita iniziata! Il tuo simbolo è: %c\n", assigned_symbol);
            ui_clear_screen();
            ui_show_message("Avversario trovato! La partita inizia...");
            game_started = 1;
            return assigned_symbol;
        }
    }
    else if (strstr(message, "ERROR:")) {
        ui_show_error(message);
        return PLAYER_NONE;
    }
    else if (strstr(message, "SERVER_SHUTDOWN:")) {
        printf("Server in spegnimento: %s\n", message + 16);
        keep_running = 0;
        return PLAYER_NONE;
    }

    return assigned_symbol;
}

PlayerSymbol handle_join_game(NetworkConnection* conn) {
    PlayerSymbol assigned_symbol = PLAYER_NONE;
    char message[MAX_MSG_SIZE];
    int bytes;
    
    // Prima svuota il buffer di ricezione per eliminare messaggi pendenti
    int flushed = network_flush_receive_buffer(conn);
    if (flushed > 0) {
        printf("[DEBUG] Buffer svuotato: %d bytes di vecchi messaggi rimossi\n", flushed);
    }
    
    // Richiedi la lista delle partite disponibili
    printf("Richiesta lista partite...\n");
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

    // Drena eventuali ulteriori risposte GAMES rimaste nel buffer prima del JOIN
    {
        fd_set read_fds;
        struct timeval tv;
        while (1) {
            FD_ZERO(&read_fds);
            FD_SET(conn->tcp_sock, &read_fds);
            tv.tv_sec = 0;
            tv.tv_usec = 100000; // Wait 100ms for any pending messages
#ifdef _WIN32
            int sel = select(0, &read_fds, NULL, NULL, &tv);
#else
            int sel = select(conn->tcp_sock + 1, &read_fds, NULL, NULL, &tv);
#endif
            if (sel > 0 && FD_ISSET(conn->tcp_sock, &read_fds)) {
                char tmp[MAX_MSG_SIZE];
                int r = network_receive(conn, tmp, sizeof(tmp), 0);
                if (r > 0) {
                    if (strncmp(tmp, "GAMES:", 6) == 0) {
                        printf("[DEBUG] Drained extra GAMES: %s\n", tmp);
                        continue; // continua a drenare
                    }
                    // Ignore other unexpected messages (PONG already handled by network_receive)
                    printf("[DEBUG] Ignoro messaggio inatteso prima del JOIN: %s\n", tmp);
                    continue;
                }
            }
            break; // nulla da drenare
        }
    }
    
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
    printf("Connessione alla partita in corso...\n");
    
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
        else if (strstr(message, "SERVER_SHUTDOWN:")) {
            printf("Server in spegnimento: %s\n", message + 16);
            keep_running = 0;
            return PLAYER_NONE;
        }
        else if (strstr(message, "JOIN_PENDING:")) {
            printf("Richiesta inviata al creatore. In attesa di approvazione...\n");
            continue; // Aspetta la risposta del creatore
        }
        else if (strstr(message, "JOIN_APPROVED:")) {
            char symbol;
            if (sscanf(message, "JOIN_APPROVED:%c", &symbol) == 1) {
                assigned_symbol = (PlayerSymbol)symbol;
                printf("Join approvato! Il tuo simbolo è: %c\n", assigned_symbol);
                printf("In attesa che la partita inizi...\n");
                continue; // Aspetta GAME_START
            }
        }
        else if (strstr(message, "JOIN_REJECTED:")) {
            ui_show_error("La tua richiesta è stata rifiutata dal creatore");
            return PLAYER_NONE;
        }
        else if (strncmp(message, "GAMES:", 6) == 0) {
            // Potrebbe essere una lista rimasta nel buffer: ignorala
            printf("[DEBUG] Ignoro GAMES arrivato dopo JOIN\n");
            continue;
        }
        else if (strstr(message, "JOIN_ACCEPTED")) {
            printf("Join accettato! In attesa del messaggio GAME_START...\n");
            printf("Unito alla partita! In attesa di inizio...\n");
            continue; // Aspetta il GAME_START
        }
        else if (strstr(message, "GAME_START:")) {
            char symbol;
            if (sscanf(message, "GAME_START:%c", &symbol) == 1) {
                assigned_symbol = (PlayerSymbol)symbol;
                printf("Partita iniziata! Il tuo simbolo è: %c\n", assigned_symbol);
                ui_clear_screen();
                printf("Partita iniziata!\n");
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
            
            // Attendi conferma della mossa
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
                // Non fare continue qui, perché potrebbe non essere più il nostro turno
                // Torna al loop principale per verificare di chi è il turno
                break; // Esce dal loop della mossa e torna al game loop principale
            }
            else if (strstr(message, "MOVE:")) {
                // Aggiorna il board e controlla lo stato del gioco
                if (game_process_network_message(game, message)) {
                    ui_show_board(game->board);
                    printf("Board aggiornato dopo la mossa\n");
                } else {
                    printf("Errore nell'aggiornamento del board\n");
                }
                
                // Dopo aver processato la nostra mossa, controlla immediatamente
                // se ci sono messaggi dell'avversario in arrivo
                fd_set read_fds;
                struct timeval tv;
                FD_ZERO(&read_fds);
                FD_SET(conn->tcp_sock, &read_fds);
                tv.tv_sec = 0;
                tv.tv_usec = 200000; // 200ms per messaggi immediati
                
#ifdef _WIN32
                int sel = select(0, &read_fds, NULL, NULL, &tv);
#else
                int sel = select(conn->tcp_sock + 1, &read_fds, NULL, NULL, &tv);
#endif
                
                if (sel > 0 && FD_ISSET(conn->tcp_sock, &read_fds)) {
                    char extra_msg[MAX_MSG_SIZE];
                    int extra_bytes = network_receive(conn, extra_msg, sizeof(extra_msg), 0);
                    if (extra_bytes > 0) {
                        printf("Messaggio immediato dopo la mossa: %s\n", extra_msg);
                        if (strstr(extra_msg, "MOVE:")) {
                            if (game_process_network_message(game, extra_msg)) {
                                ui_show_board(game->board);
                                printf("Mossa avversario processata immediatamente\n");
                            }
                        }
                        else if (strstr(extra_msg, "GAME_OVER:")) {
                            if (game_process_network_message(game, extra_msg)) {
                                printf("Partita terminata!\n");
                                if (!handle_game_end(conn, game, player_symbol)) {
                                    game->state = GAME_STATE_OVER; // Segnala che il gioco è finito
                                }
                                return;
                            }
                        }
                    }
                }
                // La mossa è stata confermata, continua il game loop
            }
            else if (strstr(message, "GAME_OVER:")) {
                if (game_process_network_message(game, message)) {
                    printf("Partita terminata!\n");
                    if (!handle_game_end(conn, game, player_symbol)) {
                        game->state = GAME_STATE_OVER; // Segnala che il gioco è finito
                    }
                    return; // Esce dalla funzione dopo aver gestito il rematch
                }
            }
            else if (strstr(message, "ERROR:")) {
                printf("Errore dal server: %s\n", message);
                ui_show_error(message);
                // Se c'è un errore, torna al game loop per permettere all'avversario di giocare
                continue;
            }
            else if (strcmp(message, "PING") == 0) {
                network_send(conn, "PONG", 0);
                continue; // Riprova a ricevere la risposta
            }
            else {
                printf("Messaggio inaspettato durante mossa: %s\n", message);
                continue; // Riprova la mossa
            }
        } 
        else {
            printf("\n=== TURNO AVVERSARIO ===\n");
            printf("Simbolo corrente: %c (il tuo: %c)\n", game->current_player, player_symbol);
            printf("Aspettando mossa avversario... (ESC per uscire)\n");
            
            // Loop per ricevere messaggi dell'avversario
            while (keep_running && game->current_player != player_symbol) {
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
                            ui_show_board(game->board);
                            printf("Mossa avversario processata\n");
                            break;  // Esce dal loop per aggiornare la visualizzazione
                        }
                    }
                    else if (strstr(message, "GAME_OVER:")) {
                        if (game_process_network_message(game, message)) {
                            printf("Partita terminata!\n");
                            if (!handle_game_end(conn, game, player_symbol)) {
                                game->state = GAME_STATE_OVER; // Segnala che il gioco è finito
                            }
                            return; // Esce dalla funzione
                        }
                    }
                    else if (strstr(message, "OPPONENT_LEFT")) {
                        ui_show_message("L'avversario ha abbandonato la partita");
                        keep_running = 0;
                        break;
                    }
                    else if (strcmp(message, "PING") == 0) {
                        network_send(conn, "PONG", 0);
                        continue; // Continua ad aspettare
                    }
                    else if (strstr(message, "REMATCH_ACCEPTED:")) {
                        printf("Rematch accettato da entrambi! Nuova partita inizia...\n");
                        // Reset del gioco locale
                        game_init_board(game);
                        game->state = GAME_STATE_PLAYING;
                        game->current_player = PLAYER_X;
                        
                        // Attendi il messaggio GAME_START per confermare il nuovo simbolo
                        char start_msg[MAX_MSG_SIZE];
                        int start_bytes = network_receive(conn, start_msg, sizeof(start_msg), 0);
                        if (start_bytes > 0 && strstr(start_msg, "GAME_START:")) {
                            printf("Nuovo GAME_START ricevuto: %s\n", start_msg);
                            char new_symbol;
                            if (sscanf(start_msg, "GAME_START:%c", &new_symbol) == 1) {
                                player_symbol = (PlayerSymbol)new_symbol;
                                printf("Nuovo simbolo assegnato: %c\n", player_symbol);
                            }
                        }
                        
                        printf("Nuova partita iniziata! Il tuo simbolo: %c\n", player_symbol);
                        break; // Esce per ricominciare il game loop
                    }
                    else {
                        printf("Messaggio non gestito durante attesa: %s\n", message);
                        continue;
                    }
                }
                else if (select_result == 0) {
                    // Timeout - continua ad aspettare
                    continue;
                }
                else if (select_result == -1) {
                    // Errore select
                    printf("Errore in select durante attesa avversario\n");
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
            
            // Reset delle variabili per la prossima iterazione
            game->winner = PLAYER_NONE;
            game->is_draw = 0;
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
        
        // Gestione fine partita - offri SOLO rematch (stessa partita)
        printf("\n=== PARTITA TERMINATA ===\n");
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
        
        // UNA SOLA domanda per il rematch
        printf("\nVuoi giocare un'altra partita? (s/n): ");
        fflush(stdout);
        
        char response;
        scanf(" %c", &response);
        
        if (response == 's' || response == 'S') {
            printf("Richiedendo rematch...\n");
            if (network_request_rematch(conn)) {
                printf("Richiesta rematch inviata. In attesa dell'avversario...\n");
                
                // Aspetta la risposta del rematch con timeout
                int rematch_handled = 0;
                int rematch_requested_by_me = 1; // Flag per indicare che ho richiesto io il rematch
                time_t start_time = time(NULL);
                
                while (!rematch_handled && difftime(time(NULL), start_time) < 30) {
                    char message[MAX_MSG_SIZE];
                    int bytes = network_receive(conn, message, sizeof(message), 0);
                    
                    if (bytes > 0) {
                        if (strstr(message, "REMATCH_ACCEPTED:")) {
                            printf("Rivincita accettata! Nuova partita inizia...\n");
                            
                            // Controlla se GAME_START è incluso nello stesso messaggio
                            char *game_start_pos = strstr(message, "GAME_START:");
                            if (game_start_pos) {
                                // GAME_START è nel messaggio combinato
                                printf("GAME_START trovato nel messaggio combinato: %s\n", message);
                                char new_symbol;
                                if (sscanf(game_start_pos, "GAME_START:%c", &new_symbol) == 1) {
                                    player_symbol = (PlayerSymbol)new_symbol;
                                    printf("Nuovo simbolo assegnato: %c\n", player_symbol);
                                }
                            } else {
                                // GAME_START arriva separatamente
                                char start_msg[MAX_MSG_SIZE];
                                int start_bytes = network_receive(conn, start_msg, sizeof(start_msg), 0);
                                if (start_bytes > 0 && strstr(start_msg, "GAME_START:")) {
                                    printf("Nuovo GAME_START ricevuto: %s\n", start_msg);
                                    char new_symbol;
                                    if (sscanf(start_msg, "GAME_START:%c", &new_symbol) == 1) {
                                        player_symbol = (PlayerSymbol)new_symbol;
                                        printf("Nuovo simbolo assegnato: %c\n", player_symbol);
                                    }
                                }
                            }
                            
                            // Reset del gioco per la rivincita
                            game_init_board(game);
                            game->state = GAME_STATE_PLAYING;
                            game->current_player = PLAYER_X;
                            game->winner = PLAYER_NONE;
                            game->is_draw = 0;
                            
                            printf("Rivincita iniziata! Il tuo simbolo: %c\n", player_symbol);
                            rematch_handled = 1;
                            return; // Torna al game loop per la nuova partita
                        } 
                        else if (strstr(message, "REMATCH_REQUEST:")) {
                            if (!rematch_requested_by_me) {
                                // Solo se NON ho richiesto io il rematch, mostro la domanda
                                printf("L'avversario ha richiesto una rivincita.\n");
                                printf("Accetti la rivincita? (s/n): ");
                                scanf(" %c", &response);
                                
                                if (response == 's' || response == 'S') {
                                    network_request_rematch(conn);
                                    printf("Rivincita accettata! In attesa di iniziare...\n");
                                    // Continua nel loop per ricevere REMATCH_ACCEPTED
                                } else {
                                    printf("Rivincita rifiutata.\n");
                                    rematch_handled = 1;
                                }
                            } else {
                                // Ignoro completamente il messaggio se ho già richiesto io il rematch
                                printf("[DEBUG] Ignorando REMATCH_REQUEST perché ho già richiesto io\n");
                                // Non faccio nulla, continuo ad aspettare REMATCH_ACCEPTED
                            }
                        }
                        else if (strstr(message, "REMATCH_SENT:")) {
                            printf("In attesa che l'avversario risponda alla rivincita...\n");
                        }
                        else if (strstr(message, "ERROR:")) {
                            printf("Errore rivincita: %s\n", message);
                            rematch_handled = 1;
                        }
                    }
                    else {
                        // Timeout o errore
                        sleep(1); // Aspetta 1 secondo prima di riprovare
                    }
                }
                
                if (!rematch_handled) {
                    printf("Timeout - l'avversario non ha risposto. Tornando al menu principale...\n");
                }
            } else {
                ui_show_error("Errore nell'invio richiesta rivincita");
            }
        } else {
            printf("Partita terminata. Tornando al menu principale...\n");
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

    // TEMPORANEAMENTE DISABILITIAMO IL THREAD RECEIVER
    // if (!network_start_receiver_thread(&conn)) {
    //     ui_show_error("Errore avvio thread receiver");
    //     network_disconnect(&conn);
    //     network_global_cleanup();
    //     return 1;
    // }

    // DISABILITIAMO IL KEEP-ALIVE AUTOMATICO
    // network_start_keep_alive(&conn);

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
                
                // Loop per gestire partita e rivincite
                while (keep_running) {
                    game_loop(&conn, &game, my_symbol);
                    
                    // Se il gioco è finito, handle_game_end gestisce tutto
                    if (game.state == GAME_STATE_OVER) {
                        break; // Esce dal loop se la partita è finita e non c'è rematch
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
                
                // Loop per gestire partita e rivincite
                while (keep_running) {
                    game_loop(&conn, &game, my_symbol);
                    
                    // Se il gioco è finito, handle_game_end gestisce tutto
                    if (game.state == GAME_STATE_OVER) {
                        break; // Esce dal loop se la partita è finita e non c'è rematch
                    }
                }
                break;
            }
                
            case 3:  // Esci
                printf("Uscita in corso...\n");
                // network_stop_receiver();  // Disabilitato temporaneamente
                keep_running = 0;
                break;
                
            default:
                ui_show_error("Scelta non valida");
                break;
        }
    }

    // Cleanup semplificato senza thread receiver
    network_stop_keep_alive(&conn);
    network_disconnect(&conn);
    network_global_cleanup();
    return 0;
}