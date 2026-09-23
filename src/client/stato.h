#ifndef CLIENT_STATO_H
#define CLIENT_STATO_H

#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include "costanti.h"

typedef struct {
    int socket;
    int review_ack; // 1 se l'utente ha già mandato ack per la review
    int attivo; // 0 se non sono connesso con questo utente 1 se sono connesso
    int porta;
} struct_utenti;

typedef struct {
    int ID;
    char testo[DIM_TESTO];
} struct_curr_card;

extern fd_set fd_lettura;
extern fd_set fd_temp;

extern int socket_P2P;
extern int max_fd;
extern int my_port;
extern struct timeval tv;
extern char BUFFER_IN[DIM_BUFFER];
extern char BUFFER_OUT[DIM_BUFFER];
extern struct_utenti utenti[MAX_UTENTI];
extern struct_curr_card curr_card;

// variabili per il flusso di lavoro della card
extern int card_done; // il thread la imposta ad 1 quando ha finito con la card
extern int reviewed; // viene impostata ad 1 quando ho ricevuto REVIEW_ACK da tutti gli utenti
extern int in_review; // 1 mentre aspetto le REVIEW_ACK per la card corrente
extern time_t review_iniziata; // quando è partita l'attesa della review corrente

extern int socket_lavagna;

// inizializza la struttura degli utenti
void init_utenti(void);

// riceve il messaggio dal socket e lo mette in BUFFER_IN, restituisce 0 se c'è errore
int get_msg(int socket);

void close_handler(int socket);

// thread che simula il tempo di lavoro sulla card corrente
void *process_card(void *arg);

void avvia_processo_card(int secondi);

// si connette all'utente con la porta specificata
void connect_to_user(int porta);

#endif
