#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>

// CAMBIATO: Include cross-platform
#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h>
#else
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/wait.h>
#endif

#include "headers/network.h"
#include "headers/lobby.h"
#include "headers/game_manager.h"

static ServerNetwork server;
static int server_running = 1;

// CAMBIATO: Gestione segnali cross-platform
#ifdef _WIN32
BOOL WINAPI ConsoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT) {
        printf("\nRicevuto segnale di chiusura, spegnimento server...\n");
        server_running = 0;
        network_shutdown(&server);
        return TRUE;
    }
    return FALSE;
}

void setup_signal_handlers() {
    if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE)) {
        printf("Errore impostazione gestore segnali: %lu\n", GetLastError());
    }
}
#else
void signal_handler(int sig) {
    static int shutdown_in_progress = 0;
    
    if (shutdown_in_progress) {
        printf("\nSecondo segnale ricevuto, uscita forzata\n");
        exit(1);
    }
    
    shutdown_in_progress = 1;
    printf("\nRicevuto segnale %d, spegnimento server...\n", sig);
    server_running = 0;
    
    // Non chiamare network_shutdown qui - lascia che il main loop lo faccia
}

void setup_signal_handlers() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN); // Ignora SIGPIPE per socket chiusi
}
#endif

int main() {
#ifdef _WIN32
    printf("=== TRIS SERVER (Windows) ===\n");
#else
    printf("=== TRIS SERVER (Linux) ===\n");
#endif
    
    setup_signal_handlers();
    
    if (!network_init(&server)) {
        fprintf(stderr, "Errore inizializzazione rete\n");
        return 1;
    }
    
    if (!lobby_init()) {
        fprintf(stderr, "Errore inizializzazione lobby\n");
        network_shutdown(&server);
        return 1;
    }
    
    if (!game_manager_init()) {
        fprintf(stderr, "Errore inizializzazione game manager\n");
        lobby_cleanup();
        network_shutdown(&server);
        return 1;
    }
    
    if (!network_start_listening(&server)) {
        fprintf(stderr, "Errore avvio server\n");
        game_manager_cleanup();
        lobby_cleanup();
        network_shutdown(&server);
        return 1;
    }
    
    printf("Server avviato con successo!\n");
    printf("Premere Ctrl+C per fermare il server\n\n");
    
    while (server_running) {
        // Usa select per rendere accept non-bloccante
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server.tcp_socket, &read_fds);
        
        struct timeval tv;
        tv.tv_sec = 1;  // Timeout di 1 secondo
        tv.tv_usec = 0;
        
        int select_result = select(server.tcp_socket + 1, &read_fds, NULL, NULL, &tv);
        
        if (!server_running) break;  // Check dopo ogni operazione bloccante
        
        if (select_result > 0 && FD_ISSET(server.tcp_socket, &read_fds)) {
            Client *new_client = network_accept_client(&server);
            if (!new_client) {
                if (server_running) {
                    printf("Errore accept client\n");
                }
                continue;
            }
            
            if (!server_running) {
                closesocket(new_client->client_fd);
                free(new_client);
                break;
            }
            
            // Creazione thread con gestione errori migliorata
#ifdef _WIN32
            new_client->thread = CreateThread(NULL, 0, network_handle_client_thread, 
                                             new_client, 0, NULL);
            if (new_client->thread == NULL) {
                printf("Errore creazione thread client: %lu\n", GetLastError());
                closesocket(new_client->client_fd);
                free(new_client);
                continue;
            }
            CloseHandle(new_client->thread);
#else
            if (pthread_create(&new_client->thread, NULL, network_handle_client_thread, new_client) != 0) {
                printf("Errore creazione thread client: %s\n", strerror(errno));
                closesocket(new_client->client_fd);
                free(new_client);
                continue;
            }
            pthread_detach(new_client->thread);
#endif
        }
        else if (select_result == -1) {
            if (!server_running) break;
            if (errno == EINTR) continue;  // Interrotto da signal, continua
            printf("Errore select: %s\n", strerror(errno));
            break;
        }
        // Se select_result == 0 (timeout), continua il loop per controllare server_running
    }
    
    printf("Iniziando spegnimento server...\n");
    network_shutdown(&server);
    printf("Pulizia in corso...\n");
    game_manager_cleanup();
    lobby_cleanup();
    printf("Server spento completamente\n");
    return 0;
}