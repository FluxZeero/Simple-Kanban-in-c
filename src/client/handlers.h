#ifndef CLIENT_HANDLERS_H
#define CLIENT_HANDLERS_H

#include "costanti.h"

// riceve la porta reale di ascolto di un peer che si è appena connesso a noi
void hello_peer_handler(int socket_utente, char *porta_str);

void pong_handler(void);

// disconnessione volontaria, richiamata da tastiera: avviso la lavagna e chiudo
void quit_handler(void);

// inizializza la card se ID = -1 altrimenti aggiorna la struttura current card
void card_handler(int ID, char *testo, char *porte_utenti, int utenti_lav);

// formatta una singola cella "ID testo" a partire dal token "id:testo"; NULL = cella vuota
void formatta_cella(char *dest, int size, char *voce);

// riceve i dati della board (id:testo per colonna, separati da ';') e li stampa in tabella
void show_lavagna_data_handler(char *lista_todo, char *lista_doing, char *lista_done);

// ricevuta una richiesta di review manda all'utente che l'ha richiesto un ack
void review_card_handler(int socket_utente, int ID, char *testo);

// riceve la lista utenti aggiornata: si connette a chi non conosce ancora e
// manda a tutti i connessi la richiesta di review per la card corrente
void send_user_list_handler(char *lista);

// rimanda REVIEW_CARD a chi non ha ancora risposto, nel caso l'ack si sia perso
void ritenta_review(void);

// riceve la conferma di review da un peer; quando tutti gli utenti attivi hanno
// confermato, segnala che la card può essere chiusa
void review_ack_handler(int socket_utente, int ID);

void call_handler(int socket_utente, char *campo[MAX_CAMPI], int n_campi);

#endif
