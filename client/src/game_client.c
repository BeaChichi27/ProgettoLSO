#include "tris_client.h"
#include "protocol.h"
#include "utils.h"


void initialize_board(char board[9]) {
    for (int i = 0; i < 9; i++) {
        board[i] = ' ';
    }
}


void handle_server_message(client_context_t* ctx, const char* message) {
    if (!ctx || !message) return;

    protocol_message_t parsed_msg;
    if (parse_message(message, &parsed_msg) < 0) {
        print_error("Errore nel parsing del messaggio");
        return;
    }

    switch (ctx->state) {
        case STATE_MENU:
            handle_menu_message(ctx, &parsed_msg);
            break;
        case STATE_LOBBY:
            handle_lobby_message(ctx, &parsed_msg);
            break;
        case STATE_PLAYING:
            handle_game_message(ctx, &parsed_msg);
            break;
        default:
            print_error("Stato client non valido");
            break;
    }
}


void process_menu_state(client_context_t* ctx) {
    display_main_menu();
    int choice = get_user_choice("Scegli un'opzione: ", "1234");

    switch (choice) {
        case 1: 
            send_message(ctx, MSG_CLIENT_CREATE);
            break;
        case 2: 
            send_message(ctx, MSG_CLIENT_JOIN);
            break;
        case 3: 
            send_message(ctx, "LIST");
            break;
        case 4: 
            g_running = 0;
            break;
        default:
            print_error("Scelta non valida");
            break;
    }
}


void process_lobby_state(client_context_t* ctx) {
    printf("In attesa di un avversario...\n");
    sleep(1); 
}


void process_playing_state(client_context_t* ctx) {
    if (!ctx->is_my_turn) {
        printf("Aspettando il turno dell'avversario...\n");
        return;
    }

    display_game_board(ctx->board);
    int move = get_move_input();

    if (move >= 1 && move <= 9) {
        send_formatted_message(ctx, "%s %d", MSG_CLIENT_MOVE, move);
    } else {
        print_error("Mossa non valida!");
    }
}