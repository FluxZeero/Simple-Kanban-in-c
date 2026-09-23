#ifndef SERVER_STATO_H
#define SERVER_STATO_H

#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include "costanti.h"

typedef struct {
    int porta;    /* porta dell'utente se è 0 non si è registrato*/
    int socket;   /* socket TCP su cui gli parlo */
    int attivo;   /* 1 = presente, 0 = slot libero */
    time_t ping_timeout_counter;
} struct_utenti;

typedef struct {
    int id;
    char testo[DIM_TESTO];
    int porta_utente; // -1 = non assegnata a nessun utente
    int stato; // -1 non valid
    time_t timestamp;
} struct_card;

extern struct_utenti utenti[MAX_UTENTI];
extern struct_card cards[MAX_CARDS];

extern char BUFFER_IN[DIM_BUFFER];
extern char BUFFER_OUT[DIM_BUFFER];

extern int utenti_attivi;
extern int utenti_registrati;
extern int numero_card;
extern int socket_ascolto;

extern fd_set fd_lettura; //selezine degli utenti da cui mi aspetto di leggere
extern fd_set fd_temp; // temporaneo, utilizzato per salvare il contenuto di fd_lettura prima dell'uso di select
extern int max_fd;

extern struct timeval tv;

// inizializza la struttura degli utenti
void init_utenti(void);

// inizializza le card di partenza
void init_cards(void);

// dato un socket di un utente trova l'indice corrispondente nel vettore utenti, ritorna -1 se non esiste
int trova_indice_da_socket(int socket);

// ordina gli utenti per numero di porta crescente
void sort_utenti(void);

// data la porta di un utente rimuove tutte le card assegnate e le rimette nella colonna TO_DO
void rimuovi_card_utente(int porta);

// muove la card dalla colonna src alla colonna dst; con porta = -2 non cambia la porta
// può essere chiamata solo da altre funzioni lato server
void move_card(int ID, int src, int dst, int porta);

// assegna le card in ordine crescente di porta, verifica se ha una card attiva altrimenti gliela assegna e aspetta l'ack
void handle_card(void);

// stampa la board sul terminale della lavagna
void show_lavagna(void);

// costruisce l'elenco "id:testo;id:testo;..." delle card nello stato indicato
// dest_size e' la dimensione vera del buffer dest, per non scriverci fuori
void lista_compatta(char *dest, int dest_size, int stato1, int stato2);

// manda al richiedente i dati della board in forma compatta
void show_lavagna_data_handler(int socket);

#endif
