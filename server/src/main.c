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
/**
 * Gestore dei segnali di console per Windows.
 * Gestisce eventi di chiusura console (Ctrl+C, chiusura finestra) per permettere
 * shutdown controllato del server.
 * 
 * @param signal Tipo di segnale ricevuto (CTRL_C_EVENT, CTRL_CLOSE_EVENT, etc.)
 * @return TRUE se il segnale è stato gestito, FALSE altrimenti
 * 
 * @note Imposta server_running=0 per avviare la procedura di shutdown
 * @note Chiama network_shutdown() per interrompere immediatamente l'accettazione di nuove connessioni
 */
BOOL WINAPI ConsoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT) {
        printf("\nRicevuto segnale di chiusura, spegnimento server...\n");
        server_running = 0;
        network_shutdown(&server);
        return TRUE;
    }
    return FALSE;
}

/**
 * Configura i gestori di segnale per Windows.
 * Installa il gestore per eventi di console utilizzando SetConsoleCtrlHandler.
 * 
 * @note Stampa un errore se l'installazione del gestore fallisce
 * @note Il gestore viene impostato per gestire tutti gli eventi di chiusura console
 */
void setup_signal_handlers() {
    if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE)) {
        printf("Errore impostazione gestore segnali: %lu\n", GetLastError());
    }
}
#else
/**
 * Gestore dei segnali Unix per shutdown controllato del server.
 * Gestisce SIGINT e SIGTERM per permettere chiusura pulita delle risorse.
 * 
 * @param sig Numero del segnale ricevuto
 * 
 * @note Previene shutdown multipli con flag statico
 * @note Il secondo segnale forza l'uscita immediata
 * @note Imposta server_running=0 per terminazione controllata del main loop
 */
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

/**
 * Configura i gestori di segnale per il sistema Unix.
 * Imposta gestori per SIGINT, SIGTERM e ignora SIGPIPE.
 * 
 * @note SIGPIPE viene ignorato per evitare crash quando i client si disconnettono
 * @note I segnali SIGINT e SIGTERM attivano la procedura di shutdown controllato
 */
void setup_signal_handlers() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN); // Ignora SIGPIPE per socket chiusi
}
#endif

/**
 * Funzione principale del server Tris multiplayer.
 * Inizializza tutti i componenti del server, avvia l'ascolto per connessioni client
 * e gestisce il loop principale di accettazione connessioni con threading.
 * 
 * @return 0 se l'esecuzione è completata con successo, 1 in caso di errore
 * 
 * @note Inizializza nell'ordine: rete, lobby, game manager, ascolto
 * @note Usa select() con timeout per rendere l'accept non-bloccante e controllare server_running
 * @note Ogni client viene gestito in un thread separato (Windows: CreateThread, Unix: pthread)
 * @note Include controllo di lobby piena per rifiutare connessioni eccedenti
 * @note Gestisce graceful shutdown tramite signal handler cross-platform
 * @note Pulisce automaticamente tutte le risorse prima della terminazione
 * @note Supporta fino a MAX_CLIENTS connessioni simultanee
 */
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
            
            // Aggiungi questo controllo
            if (lobby_is_full()) {
                printf("Lobby piena, rifiuto connessione\n");
                closesocket(new_client->client_fd);
                free(new_client);
                continue;
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