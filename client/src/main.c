/**
 * ========================================================================
 * MAIN.C - CLIENT TRIS MULTIPLAYER
 * ========================================================================
 * 
 * Questo file contiene il punto di ingresso principale del client per il
 * gioco Tris multiplayer. Gestisce l'interfaccia utente, la comunicazione
 * con il server, la logica di gioco e la gestione delle partite.
 * 
 * FUNZIONALITÀ PRINCIPALI:
 * - Connessione e autenticazione al server
 * - Creazione e partecipazione a partite
 * - Gestione turni e mosse di gioco
 * - Sistema di rematch per partite multiple
 * - Gestione robusta degli errori e disconnessioni
 * 
 * COMPATIBILITÀ:
 * - Windows (Winsock2)
 * - Linux/Unix (BSD sockets)
 * 
 * AUTORE: [Nome Autore]
 * DATA: [Data]
 * VERSIONE: 1.0
 * ========================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>       
#include <errno.h>        
#include <sys/time.h>     
#include <sys/types.h>    


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

/**
 * Pulisce il buffer di input standard rimuovendo tutti i caratteri fino al newline.
 * Utilizzata per evitare che input residui interferiscano con successive letture.
 * 
 * @note Legge caratteri fino a trovare '\n' o EOF
 * @note Previene problemi di input multipli accidentali
 * @note Funzione di utilità per gestione robusta dell'input utente
 */
void clear_stdin_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

/**
 * Gestisce la fine della partita e le richieste di rematch.
 * Coordina il flusso di comunicazione tra client e server per gestire
 * richieste di nuove partite, accettazioni e rifiuti.
 * 
 * @param conn Connessione di rete attiva con il server
 * @param game Struttura dati del gioco corrente
 * @param player_symbol Simbolo del giocatore (X o O), può essere modificato per il rematch
 * @return 1 se si inizia una nuova partita, 0 se si esce dal gioco
 * 
 * @note Gestisce automaticamente la rotazione dei simboli nelle nuove partite
 * @note Include timeout e gestione dell'interruzione utente (Ctrl+C)
 */
int handle_game_end(NetworkConnection *conn, Game *game, PlayerSymbol *player_symbol) {
    
    if (keep_running && game->state == GAME_STATE_OVER) {
        ui_show_board(game->board);
        
        if (game->winner != PLAYER_NONE) {
            if (game->winner == *player_symbol) {
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
        
        
        printf("\nVuoi iniziare un'altra partita? (s/n): ");
        
        char response;
        scanf(" %c", &response);
        
        
        clear_stdin_buffer();
        
        if (response == 's' || response == 'S') {
            
            printf("Richiedendo un'altra partita...\n");
            if (network_request_rematch(conn)) {
                printf("Richiesta inviata. In attesa dell'avversario...\n");
                
                
                int rematch_handled = 0;
                int rematch_requested_by_me = 1; 
                time_t start_time = time(NULL);
                
                characters, indicating that it is a multi-line comment.
                The actual code logic is not present in this snippet. */
                while (!rematch_handled && difftime(time(NULL), start_time) < 30) {
                    char message[MAX_MSG_SIZE];
                    int bytes = network_receive(conn, message, sizeof(message), 0);
                    
                    if (bytes > 0) {
                        if (strstr(message, "REMATCH_ACCEPTED:")) {
                            printf("Altra partita accettata! Nuova partita inizia...\n");
                            
                            
                            char *game_start_pos = strstr(message, "GAME_START:");
                            if (game_start_pos) {
                                printf("GAME_START trovato nel messaggio combinato: %s\n", message);
                                char new_symbol;
                                if (sscanf(game_start_pos, "GAME_START:%c", &new_symbol) == 1) {
                                    
                                    *player_symbol = (PlayerSymbol)new_symbol;
                                    printf("Nuovo simbolo assegnato: %c\n", *player_symbol);
                                    printf("Premi INVIO 2 volte per continuare...\n");
                                    
                                    getchar();

                                }
                            } else {
                                
                                char start_msg[MAX_MSG_SIZE];
                                int start_bytes = network_receive(conn, start_msg, sizeof(start_msg), 0);
                                if (start_bytes > 0 && strstr(start_msg, "GAME_START:")) {
                                    printf("Nuovo GAME_START ricevuto: %s\n", start_msg);
                                    char new_symbol;
                                    if (sscanf(start_msg, "GAME_START:%c", &new_symbol) == 1) {
                                        
                                        *player_symbol = (PlayerSymbol)new_symbol;
                                        printf("Nuovo simbolo assegnato: %c\n", *player_symbol);
                                    }
                                }
                            }
                            
                            
                            game_init_board(game);
                            game->state = GAME_STATE_PLAYING;
                            game->current_player = PLAYER_X;
                            game->winner = PLAYER_NONE;
                            game->is_draw = 0;
                            
                            
                            clear_stdin_buffer();

                            
                            printf("=== NUOVA PARTITA INIZIATA ===\n");
                            printf("\nPremere INVIO per proseguire...");

                            
                            getchar();
                            
                            
                            if (!keep_running) {
                                printf("Uscita durante rematch...\n");
                                return 0; 
                            }
                            
                            ui_show_board(game->board);
                            rematch_handled = 1;
                            return 1; 
                        } 
                        else if (strstr(message, "REMATCH_REQUEST:")) {
                            if (!rematch_requested_by_me) {
                                
                                printf("L'avversario ha richiesto un'altra partita.\n");
                                printf("Accetti di giocare un'altra partita? (s/n): ");
                                char response_to_request;
                                scanf(" %c", &response_to_request);
                                
                                if (response_to_request == 's' || response_to_request == 'S') {
                                    network_request_rematch(conn);
                                    printf("Altra partita accettata! In attesa di iniziare...\n");
                                    
                                    
                                    clear_stdin_buffer();
                                    
                                    
                                } else {
                                    printf("Altra partita rifiutata.\n");
                                    rematch_handled = 1;
                                    return 0; 
                                }
                            } else {
                                
                                printf("In attesa che l'avversario risponda...\n");
                            }
                        }
                        else if (strstr(message, "REMATCH_SENT:")) {
                            printf("In attesa che l'avversario risponda alla richiesta...\n");
                        }
                        else if (strstr(message, "NEW_GAME_REQUEST")) {
                            
                            printf("L'avversario vuole iniziare un'altra partita.\n");
                            printf("Tornando al menu principale...\n");
                            rematch_handled = 1;
                            return 0; 
                        }
                        else if (strstr(message, "REMATCH_CANCELLED:")) {
                            printf("La richiesta è stata cancellata: %s\n", message + 17);
                            printf("Tornando al menu principale...\n");
                            rematch_handled = 1;
                            return 0; 
                        }
                        else if (strstr(message, "REMATCH_DECLINED:")) {
                            printf("L'avversario ha rifiutato l'altra partita.\n");
                            printf("Premi INVIO per tornare al menu principale...\n");
                            
                            getchar();
                            rematch_handled = 1;
                            return 0; 
                        }
                        else if (strstr(message, "REMATCH_DECLINE_CONFIRMED:")) {
                            printf("%s\n", message + 26); 
                            printf("Premi INVIO per tornare al menu principale...\n");
                            
                            getchar();
                            rematch_handled = 1;
                            return 0; 
                        }
                        else if (strstr(message, "OPPONENT_LEFT:")) {
                            printf("L'avversario ha abbandonato la partita: %s\n", message + 14);
                            printf("Tornando al menu principale...\n");
                            rematch_handled = 1;
                            return 0; 
                        }
                        else if (strstr(message, "ERROR:")) {
                            printf("Errore: %s\n", message);
                            rematch_handled = 1;
                            return 0; 
                        }
                        else if (strstr(message, "SERVER_SHUTDOWN:")) {
                            printf("Server in spegnimento: %s\n", message + 16);
                            printf("Connessione interrotta.\n");
                            keep_running = 0; 
                            rematch_handled = 1;
                            return 0;
                        }
                    }
                    else {
                        sleep(1); 
                    }
                }
                
                if (!rematch_handled) {
                    printf("Timeout - l'avversario non ha risposto.\n");
                    return 0; 
                }
            } else {
                ui_show_error("Errore nell'invio richiesta");
                return 0; 
            }
        } else if (response == 'n' || response == 'N') {
            
            printf("Rifiutando altra partita...\n");
            network_send(conn, "REMATCH_DECLINE", 0); 
            
            
            char confirm_msg[MAX_MSG_SIZE];
            int confirm_bytes = network_receive(conn, confirm_msg, sizeof(confirm_msg), 0);
            if (confirm_bytes > 0) {
                if (strstr(confirm_msg, "REMATCH_DECLINE_CONFIRMED:")) {
                    printf("%s\n", confirm_msg + 26); 
                } else {
                    printf("Conferma server: %s\n", confirm_msg);
                }
            }
            
            printf("Premi INVIO per tornare al menu principale...\n");
            
            
            getchar();
            return 0; 
        } else {
            
            printf("Risposta non valida. Tornando al menu principale...\n");
            return 0; 
        }
    }
    
    return 0; 
}

/**
 * Gestore del segnale SIGINT (Ctrl+C) per interruzione controllata del client.
 * Imposta la flag globale per terminare il programma in modo pulito.
 * 
 * @param sig Numero del segnale ricevuto (ignorato)
 * 
 * @note Non chiama funzioni non async-signal-safe per evitare comportamenti indefiniti
 */
void handle_sigint(int sig) {
    (void)sig; 
    printf("\nRicevuto segnale di interruzione...\n");
    
    keep_running = 0;
}

/**
 * Converte una posizione da numero della cella (1-9) a coordinate di riga e colonna.
 * Utilizzata per mappare l'input utente sulla griglia 3x3 del gioco.
 * 
 * @param move Numero della cella (1-9) dove 1=alto-sinistra, 9=basso-destra
 * @param row Puntatore dove scrivere la riga risultante (0-2)
 * @param col Puntatore dove scrivere la colonna risultante (0-2)
 * 
 * @note La numerazione segue il layout del tastierino numerico
 */
void get_board_position(int move, int* row, int* col) {
    *row = (move - 1) / 3;
    *col = (move - 1) % 3;
}

/**
 * Gestisce la creazione di una nuova partita e l'attesa di un avversario.
 * Invia la richiesta al server, attende la conferma e poi si mette in attesa
 * che un altro giocatore si unisca alla partita.
 * 
 * @param conn Connessione di rete attiva con il server
 * @return Simbolo assegnato al giocatore (X o O), PLAYER_NONE in caso di errore
 * 
 * @note Include timeout di 5 minuti per l'attesa dell'avversario
 * @note Gestisce la disconnessione durante l'attesa
 * @note Il creatore della partita ottiene sempre il simbolo X inizialmente
 */
PlayerSymbol handle_create_game(NetworkConnection* conn) {
    PlayerSymbol assigned_symbol = PLAYER_NONE;
    char message[MAX_MSG_SIZE];
    int bytes;
    int game_started = 0;
    time_t start_time = time(NULL);

    
    int flushed = network_flush_receive_buffer(conn);
    if (flushed > 0) {
        printf("[DEBUG] Buffer pulito: %d bytes di messaggi precedenti rimossi\n", flushed);
    }

    if (!network_create_game(conn)) {
        ui_show_error(network_get_error());
        return PLAYER_NONE;
    }

    printf("[DEBUG] Richiesta creazione partita inviata...\n");
    fflush(stdout);
    
    
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
    
    
    if (strstr(message, "WAITING_OPPONENT")) {
        printf("[DEBUG] GAME_CREATED e WAITING_OPPONENT ricevuti insieme\n");
        
    } else {
        
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
        
        if (difftime(time(NULL), start_time) > 300) {
            ui_show_message("Timeout: nessun avversario trovato");
            network_send(conn, "CANCEL", 0);
            return PLAYER_NONE;
        }
        
        
        fd_set pre_check;
        FD_ZERO(&pre_check);
        FD_SET(conn->tcp_sock, &pre_check);
        struct timeval instant = {0, 0}; 
        
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
        
        
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(conn->tcp_sock, &read_fds);
        
        struct timeval tv;
        tv.tv_sec = 0;  
        tv.tv_usec = 100000; 
        
        int select_result;
        do {
#ifdef _WIN32
            select_result = select(0, &read_fds, NULL, NULL, &tv);
#else
            select_result = select(conn->tcp_sock + 1, &read_fds, NULL, NULL, &tv);
#endif
        } while (select_result == -1 && errno == EINTR);  

        if (select_result > 0 && FD_ISSET(conn->tcp_sock, &read_fds)) {
            bytes = network_receive(conn, message, sizeof(message), 0);
            
            if (bytes > 0) {
                printf(">>> Messaggio ricevuto: %s\n", message);
                goto process_message; 
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
        
        
        if (kbhit()) {
            int key = getch();
            if (key == 27) { 
                printf("Cancellazione richiesta dall'utente\n");
                network_send(conn, "CANCEL", 0);
                return PLAYER_NONE;
            }
        }
        
        
        static time_t last_update = 0;
        time_t now = time(NULL);
        if (difftime(now, last_update) >= 3) {
            ui_show_waiting_screen();
            printf("In attesa di avversario... (ESC per annullare)\n");
            printf(">>> Controllo attivo messaggi dal server...\n");
            last_update = now;
            
            
            fd_set quick_check;
            FD_ZERO(&quick_check);
            FD_SET(conn->tcp_sock, &quick_check);
            struct timeval quick_tv = {0, 0}; 
            
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
                    goto process_message; 
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
    
    goto retry_waiting;

process_message:
    
    if (strstr(message, "JOIN_REQUEST:")) {
        
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
        goto continue_waiting; 
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

/**
 * Gestisce l'unione a una partita esistente.
 * Mostra la lista delle partite disponibili, permette la selezione e
 * gestisce il processo di approvazione da parte del creatore della partita.
 * 
 * @param conn Connessione di rete attiva con il server
 * @return Simbolo assegnato al giocatore (X o O), PLAYER_NONE in caso di errore
 * 
 * @note Richiede l'approvazione del creatore della partita
 * @note Include timeout per l'attesa dell'approvazione
 * @note Il giocatore che si unisce ottiene generalmente il simbolo O
 */
PlayerSymbol handle_join_game(NetworkConnection* conn) {
    PlayerSymbol assigned_symbol = PLAYER_NONE;
    char message[MAX_MSG_SIZE];
    int bytes;
    
    
    int flushed = network_flush_receive_buffer(conn);
    if (flushed > 0) {
        printf("[DEBUG] Buffer svuotato: %d bytes di vecchi messaggi rimossi\n", flushed);
    }
    
    
    printf("Richiesta lista partite...\n");
    if (!network_send(conn, "LIST_GAMES", 0)) {
        ui_show_error("Errore nell'invio richiesta lista partite");
        return PLAYER_NONE;
    }

    
    bytes = network_receive(conn, message, sizeof(message), 0);
    if (bytes <= 0) {
        ui_show_error(network_get_error());
        return PLAYER_NONE;
    }
    
    printf("Lista partite ricevuta: %s\n", message);
    
    
    if (!strstr(message, "GAMES:")) {
        ui_show_error("Risposta server non valida per lista partite");
        return PLAYER_NONE;
    }
    
    ui_clear_screen();
    ui_show_message(message);

    
    {
        fd_set read_fds;
        struct timeval tv;
        while (1) {
            FD_ZERO(&read_fds);
            FD_SET(conn->tcp_sock, &read_fds);
            tv.tv_sec = 0;
            tv.tv_usec = 100000; 
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
                        continue; 
                    }
                    
                    printf("[DEBUG] Ignoro messaggio inatteso prima del JOIN: %s\n", tmp);
                    continue;
                }
            }
            break; 
        }
    }
    
    
    if (strstr(message, "Nessuna partita disponibile")) {
        ui_show_message("Nessuna partita disponibile al momento");
        return PLAYER_NONE;
    }
    
    
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
    
    
    char join_msg[32];
    snprintf(join_msg, sizeof(join_msg), "JOIN:%d", game_id);
    printf("Invio richiesta: %s\n", join_msg);
    if (!network_send(conn, join_msg, 0)) {
        ui_show_error("Errore nell'invio richiesta join");
        return PLAYER_NONE;
    }

    printf("Richiesta inviata, in attesa di risposta...\n");
    printf("Connessione alla partita in corso...\n");
    
    
    time_t start_time = time(NULL);
    while (keep_running) {
        
        if (difftime(time(NULL), start_time) > 30) {
            ui_show_error("Timeout: il creatore non ha risposto entro 30 secondi");
            return PLAYER_NONE;
        }
        
        
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(conn->tcp_sock, &read_fds);
        
        struct timeval tv;
        tv.tv_sec = 2;  
        tv.tv_usec = 0;
        
        int select_result;
        do {
#ifdef _WIN32
            select_result = select(0, &read_fds, NULL, NULL, &tv);
#else
            select_result = select(conn->tcp_sock + 1, &read_fds, NULL, NULL, &tv);
#endif
        } while (select_result == -1 && errno == EINTR);  
        
        if (select_result == 0) {
            
            printf("Aspettando risposta del creatore...\n");
            continue;
        }
        
        if (select_result < 0) {
            ui_show_error("Errore di connessione durante l'attesa");
            return PLAYER_NONE;
        }
        
        bytes = network_receive(conn, message, sizeof(message), 0);
        if (bytes <= 0) {
            ui_show_error("Errore ricezione risposta");
            return PLAYER_NONE;
        }
        
        printf("Risposta ricevuta: %s\n", message);
        
        
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
            continue; 
        }
        else if (strstr(message, "JOIN_APPROVED:")) {
            char symbol;
            if (sscanf(message, "JOIN_APPROVED:%c", &symbol) == 1) {
                assigned_symbol = (PlayerSymbol)symbol;
                printf("Join approvato! Il tuo simbolo è: %c\n", assigned_symbol);
                printf("In attesa che la partita inizi...\n");
                continue; 
            }
        }
        else if (strstr(message, "JOIN_REJECTED:")) {
            ui_show_error("La tua richiesta è stata rifiutata dal creatore");
            return PLAYER_NONE;
        }
        else if (strncmp(message, "GAMES:", 6) == 0) {
            
            printf("[DEBUG] Ignoro GAMES arrivato dopo JOIN\n");
            continue;
        }
        else if (strstr(message, "JOIN_ACCEPTED")) {
            printf("Join accettato! In attesa del messaggio GAME_START...\n");
            printf("Unito alla partita! In attesa di inizio...\n");
            continue; 
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
            continue; 
        }
    }
    
    return PLAYER_NONE;
}


/**
 * Loop principale del gioco che gestisce l'interazione durante una partita.
 * Coordina i turni, l'input del giocatore, la comunicazione con il server
 * e la visualizzazione dello stato di gioco.
 * 
 * @param conn Connessione di rete attiva con il server
 * @param game Struttura dati del gioco corrente
 * @param player_symbol Simbolo del giocatore (X o O)
 * 
 * @note Gestisce timeout per le mosse e disconnessioni
 * @note Include validazione delle mosse e sincronizzazione con il server
 * @note Gestisce automaticamente la fine partita e le richieste di rematch
 */
void game_loop(NetworkConnection* conn, Game* game, PlayerSymbol player_symbol) {
    printf("Iniziando game loop. Il tuo simbolo è: %c\n", player_symbol);
    
    // ========== INIZIALIZZAZIONE PARTITA ==========
    game->state = GAME_STATE_PLAYING;
    game->current_player = PLAYER_X; // X inizia sempre
    
    while (keep_running && game->state != GAME_STATE_OVER) {
        ui_show_board(game->board);
        
        // ========== DETERMINAZIONE TURNO ==========
        int is_my_turn = (game->current_player == player_symbol);
        
        if (is_my_turn) {
            // ========== GESTIONE TURNO GIOCATORE ==========
            printf("\n=== IL TUO TURNO ===\n");
            printf("Simbolo corrente: %c (il tuo: %c)\n", game->current_player, player_symbol);
            
            // Input e validazione mossa
            int move = ui_get_player_move();
            if (move == 0) {
                printf("Uscita richiesta dall'utente\n");
                keep_running = 0;
                break;
            }
            
            int row, col;
            get_board_position(move, &row, &col);
            printf("Mossa selezionata: cella %d -> riga %d, colonna %d\n", move, row, col);
            
            
            if (!game_is_valid_move(game, row, col)) {
                ui_show_error("Mossa non valida! Cella già occupata.");
                continue;
            }
            
            // ========== INVIO MOSSA AL SERVER ==========
            char move_msg[32];
            snprintf(move_msg, sizeof(move_msg), "MOVE:%d,%d", row, col);
            printf("Invio mossa: %s\n", move_msg);
            
            if (!network_send(conn, move_msg, 0)) {
                ui_show_error("Errore nell'invio della mossa");
                keep_running = 0;
                break;
            }
            
            printf("Mossa inviata, in attesa di risposta...\n");
            
            // ========== RICEZIONE RISPOSTA SERVER ==========
            char message[MAX_MSG_SIZE];
            int bytes = network_receive(conn, message, sizeof(message), 0);
            
            if (bytes <= 0) {
                ui_show_error("Errore ricezione risposta mossa");
                keep_running = 0;
                break;
            }
            
            printf("Risposta server: %s\n", message);
            
            // ========== ELABORAZIONE RISPOSTA ==========
            if (strstr(message, "ERROR:")) {
                ui_show_error(message);
                
                
                break; 
            }
            else if (strstr(message, "MOVE:")) {
                
                if (game_process_network_message(game, message)) {
                    ui_show_board(game->board);
                    printf("Board aggiornato dopo la mossa\n");
                } else {
                    printf("Errore nell'aggiornamento del board\n");
                }
                
                
                
                fd_set read_fds;
                struct timeval tv;
                FD_ZERO(&read_fds);
                FD_SET(conn->tcp_sock, &read_fds);
                tv.tv_sec = 0;
                tv.tv_usec = 200000; 
                
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
                                if (!handle_game_end(conn, game, &player_symbol)) {
                                    game->state = GAME_STATE_OVER; 
                                }
                                return;
                            }
                        }
                    }
                }
                
            }
            else if (strstr(message, "GAME_OVER:")) {
                if (game_process_network_message(game, message)) {
                    printf("Partita terminata!\n");
                    if (!handle_game_end(conn, game, &player_symbol)) {
                        game->state = GAME_STATE_OVER; 
                    }
                    return; 
                }
            }
            else if (strstr(message, "ERROR:")) {
                printf("Errore dal server: %s\n", message);
                ui_show_error(message);
                
                continue;
            }
            else if (strcmp(message, "PING") == 0) {
                network_send(conn, "PONG", 0);
                continue; 
            }
            else {
                printf("Messaggio inaspettato durante mossa: %s\n", message);
                continue; 
            }
        } 
        else {
            // ========== GESTIONE TURNO AVVERSARIO ==========
            printf("\n=== TURNO AVVERSARIO ===\n");
            printf("Simbolo corrente: %c (il tuo: %c)\n", game->current_player, player_symbol);
            printf("Aspettando mossa avversario... (ESC per uscire)\n");
            
            // Loop di attesa mossa avversario
            while (keep_running && game->current_player != player_symbol) {
                
                fd_set read_fds;
                FD_ZERO(&read_fds);
                FD_SET(conn->tcp_sock, &read_fds);
                
                struct timeval tv;
                tv.tv_sec = 1;  
                tv.tv_usec = 0;
                
                int select_result;
                do {
#ifdef _WIN32
                    select_result = select(0, &read_fds, NULL, NULL, &tv);
#else
                    select_result = select(conn->tcp_sock + 1, &read_fds, NULL, NULL, &tv);
#endif
                } while (select_result == -1 && errno == EINTR);  

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
                        
                        if (game_process_network_message(game, message)) {
                            ui_show_board(game->board);
                            printf("Mossa avversario processata\n");
                            break;  
                        }
                    }
                    else if (strstr(message, "GAME_OVER:")) {
                        if (game_process_network_message(game, message)) {
                            printf("Partita terminata!\n");
                            if (!handle_game_end(conn, game, &player_symbol)) {
                                game->state = GAME_STATE_OVER; 
                            }
                            return; 
                        }
                    }
                    else if (strstr(message, "ERROR:")) {
                        printf("Errore dal server: %s\n", message);
                        
                        
                        if (strstr(message, "rematch") || strstr(message, "REMATCH")) {
                            printf("Errore rematch ignorato durante partita in corso\n");
                            continue;
                        }
                        
                        
                        ui_show_error(message + 6); 
                        continue;
                    }
                    else if (strstr(message, "OPPONENT_LEFT")) {
                        ui_show_message("L'avversario ha abbandonato la partita");
                        keep_running = 0;
                        break;
                    }
                    else if (strcmp(message, "PING") == 0) {
                        network_send(conn, "PONG", 0);
                        continue; 
                    }
                    else if (strstr(message, "REMATCH_ACCEPTED:")) {
                        printf("Rematch accettato da entrambi! Nuova partita inizia...\n");
                        
                        game_init_board(game);
                        game->state = GAME_STATE_PLAYING;
                        game->current_player = PLAYER_X;
                        
                        
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
                        break; 
                    }
                    else {
                        printf("Messaggio non gestito durante attesa: %s\n", message);
                        continue;
                    }
                }
                else if (select_result == 0) {
                    
                    continue;
                }
                else if (select_result == -1) {
                    
                    printf("Errore in select durante attesa avversario\n");
                    break;
                }
                
                
                if (kbhit()) {
                    int key = getch();
                    if (key == 27) { 
                        printf("Uscita richiesta dall'utente\n");
                        network_send(conn, "LEAVE", 0);
                        keep_running = 0;
                        break;
                    }
                }
            }
            
            
            game->winner = PLAYER_NONE;
            game->is_draw = 0;
        }
    }
    
    
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
        
        
        printf("\nVuoi giocare un'altra partita? (s/n): ");
        fflush(stdout);
        
        char response;
        scanf(" %c", &response);
        
        if (response == 's' || response == 'S') {
            printf("Richiedendo rematch...\n");
            if (network_request_rematch(conn)) {
                printf("Richiesta rematch inviata. In attesa dell'avversario...\n");
                
                
                int rematch_handled = 0;
                int rematch_requested_by_me = 1; 
                time_t start_time = time(NULL);
                
                while (!rematch_handled && difftime(time(NULL), start_time) < 30) {
                    char message[MAX_MSG_SIZE];
                    int bytes = network_receive(conn, message, sizeof(message), 0);
                    
                    if (bytes > 0) {
                        if (strstr(message, "REMATCH_ACCEPTED:")) {
                            printf("Rivincita accettata! Nuova partita inizia...\n");
                            
                            
                            char *game_start_pos = strstr(message, "GAME_START:");
                            if (game_start_pos) {
                                
                                printf("GAME_START trovato nel messaggio combinato: %s\n", message);
                                char new_symbol;
                                if (sscanf(game_start_pos, "GAME_START:%c", &new_symbol) == 1) {
                                    player_symbol = (PlayerSymbol)new_symbol;
                                    printf("Nuovo simbolo assegnato: %c\n", player_symbol);
                                }
                            } else {
                                
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
                            
                            
                            game_init_board(game);
                            game->state = GAME_STATE_PLAYING;
                            game->current_player = PLAYER_X;
                            game->winner = PLAYER_NONE;
                            game->is_draw = 0;
                            
                            printf("Rivincita iniziata! Il tuo simbolo: %c\n", player_symbol);
                            rematch_handled = 1;
                            return; 
                        } 
                        else if (strstr(message, "REMATCH_REQUEST:")) {
                            if (!rematch_requested_by_me) {
                                
                                printf("L'avversario ha richiesto una rivincita.\n");
                                printf("Accetti la rivincita? (s/n): ");
                                scanf(" %c", &response);
                                
                                if (response == 's' || response == 'S') {
                                    network_request_rematch(conn);
                                    printf("Rivincita accettata! In attesa di iniziare...\n");
                                    
                                } else {
                                    printf("Rivincita rifiutata.\n");
                                    rematch_handled = 1;
                                }
                            } else {
                                
                                printf("[DEBUG] Ignorando REMATCH_REQUEST perché ho già richiesto io\n");
                                
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
                        
                        sleep(1); 
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

/**
 * Funzione principale del client Tris.
 * Inizializza la rete, gestisce la registrazione del giocatore,
 * presenta il menu principale e coordina le partite.
 * 
 * @return 0 se l'esecuzione è completata con successo, 1 in caso di errore
 * 
 * @note Gestisce automaticamente la pulizia delle risorse alla chiusura
 * @note Include gestione dei segnali per interruzione pulita (Ctrl+C)
 * @note Supporta creazione e partecipazione a partite multiple
 */
int main() {
    // ========== INIZIALIZZAZIONE SISTEMA ==========
    signal(SIGINT, handle_sigint);

    if (!network_global_init()) {
        ui_show_error("Errore inizializzazione rete");
        return 1;
    }

    // ========== REGISTRAZIONE GIOCATORE ==========
    char player_name[50];
    if (!ui_get_player_name(player_name, sizeof(player_name))) {
        ui_show_error("Nome non valido");
        network_global_cleanup();
        return 1;
    }

    // ========== CONNESSIONE AL SERVER ==========
    NetworkConnection conn;
    network_init(&conn);
    
    if (!network_connect_to_server(&conn)) {
        ui_show_error(network_get_error());
        network_global_cleanup();
        return 1;
    }
    
    // ========== AUTENTICAZIONE ==========
    if (!network_register_name(&conn, player_name)) {
        ui_show_error(network_get_error());
        network_disconnect(&conn);
        network_global_cleanup();
        return 1;
    }

    ui_clear_screen();
    printf("\nRegistrazione completata come %s\n", player_name);
    sleep(1);

    // ========== LOOP PRINCIPALE DEL MENU ==========
    while (keep_running) {
        // Pulizia buffer per evitare messaggi residui
        int flushed = network_flush_receive_buffer(&conn);
        if (flushed > 0) {
            printf("[DEBUG] Buffer menu pulito: %d bytes rimossi\n", flushed);
        }
        
        int choice = ui_show_main_menu();
        
        switch (choice) {
            case 1: {  // ========== CREAZIONE PARTITA ==========
                PlayerSymbol my_symbol = handle_create_game(&conn);
                if (my_symbol == PLAYER_NONE) break;

                Game game;
                game_init(&game);
                game.state = GAME_STATE_PLAYING;
                game.current_player = PLAYER_X;  // Il primo giocatore inizia sempre
                
                // Loop di gioco per partite multiple (rematch)
                while (keep_running) {
                    game_loop(&conn, &game, my_symbol);
                    
                    // Se la partita è terminata definitivamente, esci dal loop
                    if (game.state == GAME_STATE_OVER) {
                        break; 
                    }
                }
                break;
            }
                
            case 2: {  // ========== UNIONE A PARTITA ==========
                PlayerSymbol my_symbol = handle_join_game(&conn);
                if (my_symbol == PLAYER_NONE) break;

                Game game;
                game_init(&game);
                game.state = GAME_STATE_PLAYING;
                game.current_player = PLAYER_X;  // Il primo giocatore inizia sempre
                
                // Loop di gioco per partite multiple (rematch)
                while (keep_running) {
                    game_loop(&conn, &game, my_symbol);
                    
                    // Se la partita è terminata definitivamente, esci dal loop
                    if (game.state == GAME_STATE_OVER) {
                        break; 
                    }
                }
                break;
            }
                
            case 3:  // ========== USCITA ==========
                printf("Uscita in corso...\n");
                
                keep_running = 0;
                break;
                
            default:
                ui_show_error("Scelta non valida");
                break;
        }
    }

    // ========== PULIZIA E DISCONNESSIONE ==========
    network_stop_keep_alive(&conn);
    network_disconnect(&conn);
    network_global_cleanup();
    return 0;
}