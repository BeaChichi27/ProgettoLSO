#ifndef LOBBY_H
#define LOBBY_H

/*
 * HEADER LOBBY - GESTIONE LOBBY SERVER TRIS
 *
 * Questo header definisce tutte le funzioni necessarie per la gestione
 * della lobby del server del gioco Tris multiplayer.
 * 
 * La lobby è il punto centrale di coordinamento per tutti i client connessi.
 * Gestisce registrazione, disconnessioni, routing dei messaggi e
 * integrazione con il sistema di gestione partite.
 *
 * Supporta fino a MAX_CLIENTS connessioni simultanee con gestione
 * thread-safe per operazioni concorrenti.
 *
 * Compatibile con Windows e sistemi Unix/Linux.
 */

#include "network.h"
#include "game_manager.h"

// ========== CONFIGURAZIONI ==========

#define MAX_CLIENTS 100             // Numero massimo di client simultanei

// ========== FUNZIONI DI INIZIALIZZAZIONE ==========

int lobby_init();
void lobby_cleanup();
int lobby_is_full(); 

// ========== FUNZIONI DI GESTIONE CLIENT ==========

Client* lobby_add_client(socket_t client_fd, const char *name);
void lobby_remove_client(Client *client);
Client* lobby_find_client_by_fd(socket_t fd);
Client* lobby_find_client_by_name(const char *name);

// ========== FUNZIONI DI COMUNICAZIONE ==========

void lobby_handle_client_message(Client *client, const char *message);
void lobby_broadcast_message(const char *message, Client *exclude);
void lobby_broadcast_game_list();

// ========== FUNZIONI DI GESTIONE COMANDI ==========

void lobby_handle_register(Client *client, const char *name);
void lobby_handle_create_game(Client *client);
void lobby_handle_join_game(Client *client, const char *message);
void lobby_handle_approve_join(Client *client, const char *message);
void lobby_handle_list_games(Client *client);
void lobby_handle_move(Client *client, const char *message);
void lobby_handle_rematch(Client *client);
void lobby_remove_client_reference(Client *client);

// ========== FUNZIONI DI ACCESSO THREAD-SAFE ==========

mutex_t* lobby_get_mutex();
Client* lobby_get_client_by_index(int index);
int lobby_add_client_reference(Client *client);

#endif