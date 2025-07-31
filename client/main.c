



#include "tris_client.h"
#include "protocol.h"
#include "utils.h"


client_context_t g_client;
volatile int g_running = 1;



void signal_handler(int sig) {
    (void)sig; 
    printf("\n\nRicevuto segnale di interruzione. Chiusura in corso...\n");
    g_running = 0;
    
    if (g_client.socket_fd >= 0) {
        send_message(&g_client, MSG_CLIENT_QUIT);
        cleanup_client(&g_client);
    }
    
    exit(EXIT_SUCCESS);
}

int main_loop(void) {
    char buffer[MAX_BUFFER];
    fd_set read_fds;
    struct timeval timeout;
    int max_fd;
    
    while (g_running) {
        
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(g_client.socket_fd, &read_fds);
        
        max_fd = (g_client.socket_fd > STDIN_FILENO) ? g_client.socket_fd : STDIN_FILENO;
        
        
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (activity < 0 && errno != EINTR) {
            print_error("Errore nella select");
            return -1;
        }
        
        if (activity == 0) {
            
            continue;
        }
        
        
        if (FD_ISSET(g_client.socket_fd, &read_fds)) {
            memset(buffer, 0, sizeof(buffer));
            int bytes_received = receive_message(&g_client, buffer, sizeof(buffer));
            
            if (bytes_received <= 0) {
                print_error("Connessione persa con il server");
                return -1;
            }
            
            handle_server_message(&g_client, buffer);
        }
        
        
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            switch (g_client.state) {
                case STATE_MENU:
                    process_menu_state(&g_client);
                    break;
                case STATE_LOBBY:
                    process_lobby_state(&g_client);
                    break;
                case STATE_PLAYING:
                    process_playing_state(&g_client);
                    break;
                case STATE_GAME_OVER:
                    
                    g_client.state = STATE_MENU;
                    break;
                default:
                    print_error("Stato client non riconosciuto");
                    break;
            }
        }
    }
    
    return 0;
}

int main(int argc, char* argv[]) {
    (void)argc; 
    (void)argv;
    
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN); 
    
    print_info("=== TRIS CLIENT INTEGRATO ===");
    print_info("Versione 1.0");
    
    
    if (initialize_client(&g_client) < 0) {
        print_error("Errore nell'inizializzazione del client");
        return EXIT_FAILURE;
    }
    
    
    print_info("Connessione al server in corso...");
    if (connect_to_server(&g_client) < 0) {
        print_error("Impossibile connettersi al server");
        cleanup_client(&g_client);
        return EXIT_FAILURE;
    }
    
    print_colored(COLOR_GREEN, "Connesso al server con successo!");
    
    
    printf("\n");
    get_player_name(g_client.player_name, MAX_NAME);
    
    if (send_formatted_message(&g_client, "%s %s", MSG_CLIENT_REGISTER, g_client.player_name) < 0) {
        print_error("Errore nella registrazione");
        cleanup_client(&g_client);
        return EXIT_FAILURE;
    }
    
    
    char buffer[MAX_BUFFER];
    if (receive_message(&g_client, buffer, sizeof(buffer)) <= 0) {
        print_error("Errore nella ricezione della conferma registrazione");
        cleanup_client(&g_client);
        return EXIT_FAILURE;
    }
    
    if (strstr(buffer, "ERROR") != NULL) {
        print_error("Registrazione fallita. Nome già in uso o non valido.");
        cleanup_client(&g_client);
        return EXIT_FAILURE;
    }
    
    print_colored(COLOR_GREEN, "Registrazione completata con successo!");
    
    
    g_client.state = STATE_MENU;
    
    
    printf("\n");
    print_info("Avvio del client...");
    
    int result = main_loop();
    
    
    cleanup_client(&g_client);
    
    if (result < 0) {
        print_error("Il client è terminato con errori");
        return EXIT_FAILURE;
    }
    
    print_info("Client terminato correttamente");
    return EXIT_SUCCESS;
}