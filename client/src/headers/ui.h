#ifndef UI_H
#define UI_H

#ifndef _WIN32
int kbhit(void);
int getch(void);
#else
#include <conio.h>
#define kbhit _kbhit
#define getch _getch
#endif

int ui_show_main_menu();

void ui_show_waiting_screen();

void ui_show_board(const char board[3][3]);

int ui_get_player_move();

void ui_show_message(const char *message);

void ui_show_error(const char *error);

void ui_clear_screen();

int ui_get_player_name(char *name, int max_length);

void ui_show_waiting_screen(void);

#endif