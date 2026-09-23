#ifndef SERVER_HANDLERS_H
#define SERVER_HANDLERS_H

#include "costanti.h"

void hello_handler(int socket_utente, char porta[MAX_MSG]);
void create_card_handler(int ID, int colonna, char *testo, int dim_testo);

/* la funzione termina la connnessione con il client, rimuove le card dell'utente
   e la riassegna ad un utente se è libero
*/
void quit_handler(int socket);

// termina la lavagna, richiamato da tastiera: chiude tutti i socket aperti ed esce
void close_handler(void);

// manda la lista delle porte all'utente identificato con socket
void user_list_handler(int socket);

// ogni 5 secondi se non ho ricevuto altre richieste pingo gli user
// se non mi rispondono entro 5 secondi allora gli tolgo la card
void ping_user(void);

void pong_handler(int socket_utente);

// in base al comando ricevuto chiamo l'handler corretto per la gestione della richiesta
void call_handler(int socket_utente, char *campo[MAX_CAMPI], int n_campi);

#endif
