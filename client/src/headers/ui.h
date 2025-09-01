#ifndef UI_H
#define UI_H

/*
 * HEADER UI - INTERFACCIA UTENTE DEL CLIENT TRIS
 *
 * Questo header definisce tutte le funzioni per l'interfaccia utente
 * testuale del gioco Tris. Include funzioni per visualizzazione,
 * input utente e gestione della console.
 * 
 * Supporta sia Windows che sistemi Unix/Linux con gestione
 * cross-platform dell'input da tastiera.
 *
 * L'interfaccia è completamente testuale e user-friendly.
 */

// ========== CONFIGURAZIONE CROSS-PLATFORM INPUT ==========

#ifndef _WIN32
/**
 * Verifica se è presente un tasto premuto (versione Unix/Linux).
 * @return 1 se c'è un tasto in attesa, 0 altrimenti
 */
int kbhit(void);

/**
 * Legge un carattere dalla tastiera senza echo (versione Unix/Linux).
 * @return Carattere letto dalla tastiera
 */
int getch(void);
#else
    #include <conio.h>
    #define kbhit _kbhit
    #define getch _getch
#endif

// ========== FUNZIONI DI MENU E NAVIGAZIONE ==========

int ui_show_main_menu();

void ui_show_waiting_screen();

// ========== FUNZIONI DI VISUALIZZAZIONE GIOCO ==========

void ui_show_board(const char board[3][3]);

int ui_get_player_move();

// ========== FUNZIONI DI MESSAGGISTICA ==========

void ui_show_message(const char *message);

void ui_show_error(const char *error);

// ========== FUNZIONI DI UTILITÀ SCHERMO ==========

void ui_clear_screen();

// ========== FUNZIONI DI INPUT UTENTE ==========

int ui_get_player_name(char *name, int max_length);

void ui_show_waiting_screen(void);

#endif