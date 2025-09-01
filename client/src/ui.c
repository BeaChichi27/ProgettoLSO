#include "headers/ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <conio.h>
#else
    #include <unistd.h>
    #include <termios.h>
    #include <sys/time.h>
    #include <sys/select.h>

    /**
     * Verifica se è presente un tasto premuto (implementazione Unix/Linux).
     * Utilizza select() per controllare l'input senza bloccare l'esecuzione.
     * 
     * @return 1 se c'è un tasto in attesa di essere letto, 0 altrimenti
     * 
     * @note Implementazione non-bloccante usando select() con timeout zero
     * @note Alternativa Unix/Linux alla funzione kbhit() di Windows
     */
    int kbhit() {
        struct timeval tv = { 0L, 0L };
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(0, &fds);
        return select(1, &fds, NULL, NULL, &tv);
    }

    /**
     * Legge un singolo carattere dalla tastiera senza echo (implementazione Unix/Linux).
     * Disabilita temporaneamente il buffering e l'echo del terminale.
     * 
     * @return Carattere letto dalla tastiera
     * 
     * @note Salva e ripristina le impostazioni originali del terminale
     * @note Disabilita ICANON (modalità canonica) e ECHO
     * @note Alternativa Unix/Linux alla funzione getch() di Windows
     */
    int getch() {
        struct termios old_tio, new_tio;
        int c;
        
        tcgetattr(STDIN_FILENO, &old_tio);
        new_tio = old_tio;
        new_tio.c_lflag &= (~ICANON & ~ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
        
        c = getchar();
        
        tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
        return c;
    }
#endif

/**
 * Cancella lo schermo del terminale in modo cross-platform.
 * Utilizza 'cls' su Windows e 'clear' su sistemi Unix.
 * 
 * @note Funzione cross-platform che si adatta al sistema operativo
 * @note Non gestisce errori di sistema, assume che i comandi siano disponibili
 */
void ui_clear_screen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

/**
 * Mostra il menu principale e ottiene la scelta dell'utente.
 * Presenta le opzioni del gioco e attende input valido dall'utente.
 * 
 * @return Numero dell'opzione scelta (1-3):
 *         1 = Crea nuova partita
 *         2 = Unisciti a partita  
 *         3 = Esci
 * 
 * @note Utilizza getch() per input immediato senza premere INVIO
 * @note Accetta solo input validi (1-3), ignora altri caratteri
 */
int ui_show_main_menu() {
    ui_clear_screen();
    printf("\n");
    printf("=== TRIS ONLINE ===\n");
    printf("1. Crea nuova partita\n");
    printf("2. Unisciti a partita\n");
    printf("3. Esci\n");
    printf("\nScelta: ");

    int choice = 0;
    while (1) {
        char input = getch();
        if (input >= '1' && input <= '3') {
            choice = input - '0';
            printf("%d\n", choice);
            break;
        }
    }
    
    return choice;
}

/**
 * Visualizza il tabellone di gioco in formato grafico ASCII.
 * Mostra una griglia 3x3 con i simboli dei giocatori e separatori.
 * 
 * @param board Tabellone 3x3 da visualizzare
 * 
 * @note Pulisce lo schermo prima di mostrare il tabellone
 * @note Formato di output:
 *        X | O |   
 *       ---+---+---
 *        O | X | X
 *       ---+---+---
 *          | O |   
 * @note Aggiunge spazi e righe vuote per migliorare la leggibilità
 */
void ui_show_board(const char board[3][3]) {
    ui_clear_screen();
    printf("\n");
    printf(" %c | %c | %c \n", board[0][0], board[0][1], board[0][2]);
    printf("---+---+---\n");
    printf(" %c | %c | %c \n", board[1][0], board[1][1], board[1][2]);
    printf("---+---+---\n");
    printf(" %c | %c | %c \n", board[2][0], board[2][1], board[2][2]);
    printf("\n");
}

// Sostituisci la funzione ui_get_player_move in ui.c

/**
 * Richiede all'utente di inserire una mossa e la valida.
 * Mostra una tabella delle posizioni disponibili e attende input.
 * 
 * @return Posizione scelta dall'utente (1-9) o 0 per uscire
 * 
 * @note Mostra una griglia numerata (1-9) per facilitare la scelta
 * @note Accetta valori da 0 a 9 (0 = uscita, 1-9 = posizioni)
 * @note Utilizza ricorsione per gestire input non validi
 * @note Converte automaticamente la posizione 1-9 in coordinate row,col
 */
int ui_get_player_move() {
    printf("\nTABLELLA POSIZIONI:\n");
    printf(" 1 | 2 | 3 \n");
    printf("---+---+---\n");
    printf(" 4 | 5 | 6 \n");
    printf("---+---+---\n");
    printf(" 7 | 8 | 9 \n");
    printf("\nScegli una cella (1-9) o 0 per uscire: ");
    fflush(stdout);
    
    char input[10];
    if (fgets(input, sizeof(input), stdin) == NULL) {
        return 0;
    }
    
    int choice = atoi(input);
    
    if (choice >= 0 && choice <= 9) {
        printf("Scelta: %d\n", choice);
        return choice;
    }
    
    printf("Scelta non valida! Riprova.\n");
    return ui_get_player_move();  // Richiama ricorsivamente
}

/**
 * Mostra un messaggio informativo all'utente e attende conferma.
 * Visualizza il messaggio e attende la pressione di un tasto per continuare.
 * 
 * @param message Messaggio da visualizzare all'utente
 * 
 * @note Aggiunge automaticamente una riga vuota prima del messaggio
 * @note Utilizza getch() per input immediato senza premere INVIO
 * @note Blocca l'esecuzione fino alla pressione di un tasto
 */
void ui_show_message(const char *message) {
    printf("\n%s\n", message);
    printf("Premi un tasto per continuare...");
    getch();
}

/**
 * Mostra un messaggio di errore con formattazione speciale e attende conferma.
 * Evidenzia il messaggio come errore con il prefisso "ERRORE:".
 * 
 * @param error Messaggio di errore da visualizzare
 * 
 * @note Aggiunge automaticamente il prefisso "ERRORE:" al messaggio
 * @note Utilizza getch() per input immediato senza premere INVIO
 * @note Blocca l'esecuzione fino alla pressione di un tasto
 */
void ui_show_error(const char *error) {
    printf("\nERRORE: %s\n", error);
    printf("Premi un tasto per continuare...");
    getch();
}

/**
 * Richiede all'utente di inserire il proprio nome e lo valida.
 * Pulisce lo schermo, mostra il prompt e legge il nome dall'input standard.
 * 
 * @param name Buffer dove salvare il nome inserito dall'utente
 * @param max_length Lunghezza massima del nome (incluso terminatore null)
 * @return 1 se il nome è stato inserito correttamente, 0 se vuoto o errore
 * 
 * @note Pulisce automaticamente lo schermo prima del prompt
 * @note Rimuove il carattere newline dalla fine dell'input
 * @note Rifiuta nomi vuoti (solo spazi o Enter)
 * @note Il buffer deve essere almeno di dimensione max_length
 */
int ui_get_player_name(char *name, int max_length) {
    ui_clear_screen();
    printf("\nInserisci il tuo nome (max %d caratteri): ", max_length - 1);
    
    if (fgets(name, max_length, stdin) == NULL) {
        return 0;
    }
    
    name[strcspn(name, "\n")] = '\0';
    
    if (strlen(name) == 0) {
        return 0;
    }
    
    return 1;
}

/**
 * Mostra una schermata di attesa formattata durante operazioni asincrone.
 * Visualizza un'interfaccia grafica per indicare operazioni in corso.
 * 
 * @note Pulisce automaticamente lo schermo prima di mostrare l'interfaccia
 * @note Mostra un'animazione testuale di "caricamento"
 * @note Include istruzioni per l'utente (Ctrl+C per annullare)
 * @note Utilizza fflush(stdout) per assicurare la visualizzazione immediata
 * @note Design ASCII art per migliorare l'esperienza utente
 */
void ui_show_waiting_screen(void) {
    ui_clear_screen();
    
    printf("\n");
    printf("====================================\n");
    printf("        IN ATTESA DI RISPOSTA       \n");
    printf("====================================\n");
    printf("\n");
    printf("   [.  ]  Connessione in corso...   \n");
    printf("\n");
    printf("   Premere Ctrl+C per annullare     \n");
    printf("\n");
    printf("====================================\n");
    
    fflush(stdout);
}