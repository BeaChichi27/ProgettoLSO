#include "tris_client.h"
#include "utils.h"



void display_main_menu(void) {
    CLEAR_SCREEN();
    printf("\n");
    printf("╔══════════════════════════════╗\n");
    printf("║        TRIS CLIENT           ║\n");
    printf("╠══════════════════════════════╣\n");
    printf("║  1. Crea partita             ║\n");
    printf("║  2. Unisciti a partita       ║\n");
    printf("║  3. Lista partite            ║\n");
    printf("║  4. Esci                     ║\n");
    printf("╚══════════════════════════════╝\n");
    printf("\nGiocatore: %s%s%s\n", COLOR_CYAN, g_client.player_name, COLOR_RESET);
}

void display_game_board(const char board[9]) {
    CLEAR_SCREEN();
    
    printf("\n%s=== PARTITA IN CORSO ===%s\n", COLOR_YELLOW, COLOR_RESET);
    printf("Tu: %s%c%s vs %s: %s%c%s\n", 
           COLOR_GREEN, g_client.symbol, COLOR_RESET,
           g_client.opponent_name,
           COLOR_RED, (g_client.symbol == 'X') ? 'O' : 'X', COLOR_RESET);
    
    printf("\n   1   2   3\n");
    printf("A  %c | %c | %c \n", 
           (board[0] == ' ') ? '1' : board[0],
           (board[1] == ' ') ? '2' : board[1],
           (board[2] == ' ') ? '3' : board[2]);
    printf("  ---|---|---\n");
    printf("B  %c | %c | %c \n", 
           (board[3] == ' ') ? '4' : board[3],
           (board[4] == ' ') ? '5' : board[4],
           (board[5] == ' ') ? '6' : board[5]);
    printf("  ---|---|---\n");
    printf("C  %c | %c | %c \n", 
           (board[6] == ' ') ? '7' : board[6],
           (board[7] == ' ') ? '8' : board[7],
           (board[8] == ' ') ? '9' : board[8]);
    printf("\n");
    
    if (g_client.is_my_turn) {
        print_colored(COLOR_GREEN, "È il tuo turno!");
    } else {
        print_colored(COLOR_YELLOW, "Turno dell'avversario...");
    }
    
    printf("\n");
}

void display_game_list(const char* games) {
    CLEAR_SCREEN();
    printf("\n%s=== PARTITE DISPONIBILI ===%s\n", COLOR_CYAN, COLOR_RESET);
    
    if (!games || strlen(games) == 0) {
        print_colored(COLOR_YELLOW, "Nessuna partita disponibile al momento.");
        printf("\nPremi Invio per tornare al menu...");
        wait_for_enter();
        return;
    }
    
    printf("%s\n", games);
}

int get_user_choice(const char* prompt, const char* valid_choices) {
    char input[10];
    
    while (1) {
        printf("%s", prompt);
        fflush(stdout);
        
        if (!fgets(input, sizeof(input), stdin)) {
            if (feof(stdin)) {
                printf("\nEOF rilevato. Uscita...\n");
                return -1;
            }
            continue;
        }
        
        
        input[strcspn(input, "\n")] = 0;
        
        
        if (strlen(input) == 1 && strchr(valid_choices, input[0])) {
            return input[0] - '0';
        }
        
        print_colored(COLOR_RED, "Scelta non valida!");
        printf("Scelte disponibili: ");
        for (int i = 0; valid_choices[i]; i++) {
            printf("%c ", valid_choices[i]);
        }
        printf("\n");
    }
}

int get_move_input(void) {
    char input[10];
    
    while (1) {
        printf("Inserisci la tua mossa (1-9): ");
        fflush(stdout);
        
        if (!fgets(input, sizeof(input), stdin)) {
            if (feof(stdin)) {
                return -1;
            }
            continue;
        }
        
        
        input[strcspn(input, "\n")] = 0;
        
        
        if (validate_move_input(input)) {
            int move = atoi(input);
            if (move >= 1 && move <= 9) {
                return move;
            }
        }
        
        print_colored(COLOR_RED, "Mossa non valida! Inserisci un numero da 1 a 9.");
    }
}

void get_player_name(char* name, size_t max_len) {
    while (1) {
        printf("Inserisci il tuo nome (max %zu caratteri): ", max_len - 1);
        fflush(stdout);
        
        if (!fgets(name, max_len, stdin)) {
            if (feof(stdin)) {
                strcpy(name, "Giocatore");
                break;
            }
            continue;
        }
        
        
        name[strcspn(name, "\n")] = 0;
        
        
        trim_whitespace(name);
        
        
        if (strlen(name) > 0 && strlen(name) < max_len) {
            break;
        }
        
        print_colored(COLOR_RED, "Nome non valido! Deve essere tra 1 e %zu caratteri.");
    }
    
    printf("Benvenuto, %s%s%s!\n", COLOR_CYAN, name, COLOR_RESET);
}

void print_error(const char* message) {
    if (message) {
        print_colored(COLOR_RED, message);
    }
}

void print_info(const char* message) {
    if (message) {
        printf("%s[INFO]%s %s\n", COLOR_BLUE, COLOR_RESET, message);
    }
}

void wait_for_enter(void) {
    printf("\nPremi Invio per continuare...");
    fflush(stdout);
    
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {
        
    }
}

void display_welcome_message(void) {
    CLEAR_SCREEN();
    printf("\n");
    printf("╔══════════════════════════════════════╗\n");
    printf("║     %s🎮 TRIS CLIENT INTEGRATO 🎮%s     ║\n", COLOR_CYAN, COLOR_RESET);
    printf("╠══════════════════════════════════════╣\n");
    printf("║                                      ║\n");
    printf("║  Benvenuto al gioco del Tris!        ║\n");
    printf("║                                      ║\n");
    printf("║  Caratteristiche:                    ║\n");
    printf("║  • Multiplayer online                ║\n");
    printf("║  • Interfaccia colorata              ║\n");
    printf("║  • Gestione partite in tempo reale   ║\n");
    printf("║  • Statistiche giocatore             ║\n");
    printf("║                                      ║\n");
    printf("╚══════════════════════════════════════╝\n");
    printf("\n%s[!]%s Prima di iniziare, inserisci il tuo nome\n", COLOR_YELLOW, COLOR_RESET);
    printf("\n");
}