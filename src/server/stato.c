#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "stato.h"
#include "rete.h"

/* ============================================ Variabili Globali ============================================ */

struct_utenti utenti[MAX_UTENTI];
struct_card cards[MAX_CARDS];

char BUFFER_IN[DIM_BUFFER];
char BUFFER_OUT[DIM_BUFFER];

int utenti_attivi = 0;
int utenti_registrati = 0;
int numero_card = 0;
int socket_ascolto;

fd_set fd_lettura;
fd_set fd_temp;
int max_fd = 0;

struct timeval tv;

static const char *testi_iniziali[MAX_INIT_CARDS] = {
    "Implementare integrazione per il pagamento",
    "Diagramma delle classi UML",
    "Studio dei requisiti dell'applicazione",
    "Implementare sito web servizio",
    "Scrivere test di integrazione",
    "Configurare pipeline CI",
    "Progettare schema database",
    "Implementare autenticazione utenti",
    "Ottimizzare query principali",
    "Scrivere documentazione API",
    "Revisione modello E-R database",
    "Calcolo costi computazionali algoritmi",
    "Analisi delle ridondanze",
    "Implementare chatbot",
    "Tradurre i testi in inglese"
};

/* ======================================== Inizializzazione e utenti ========================================= */

void init_utenti(void){

    for(int i = 0; i < MAX_UTENTI; i++){
        utenti[i].attivo = 0;
        utenti[i].socket = -1;
        utenti[i].porta = 0;
        utenti[i].ping_timeout_counter = 0;
    }

    return;
}

void init_cards(void){

    for (int i = 0; i < MAX_INIT_CARDS; i++){
        cards[i].id = i;
        cards[i].porta_utente = -1; // non assegnata
        cards[i].stato = TO_DO;
        strcpy(cards[i].testo,testi_iniziali[i]);
        cards[i].testo[DIM_TESTO - 1] = '\0';
        time(&cards[i].timestamp);
        numero_card ++;
    }

    return;
}

int trova_indice_da_socket(int socket){

    int ret = -1;
    int i = 0;

    if(socket == STDIN_FILENO){
        return 0;
    }

    while (i < MAX_UTENTI){
        if(utenti[i].socket == socket){
            ret = i;
            break;
        } else {
            i++;
        }
    }

    return ret;
}

void sort_utenti(void){

    int scambiati = 1;

    while (scambiati){
        scambiati = 0;
        for(int i = 0; i < MAX_UTENTI - 1; i++){
            if(utenti[i].porta > utenti[i + 1].porta){
                struct_utenti temp = utenti[i];
                utenti[i] = utenti[i + 1];
                utenti[i + 1] = temp;
                scambiati = 1;
            }
        }
    }

}

/* ============================================ Logica delle card ============================================= */

void rimuovi_card_utente(int porta){

    for (int i = 0; i < numero_card; i++){
        if(cards[i].porta_utente == porta && cards[i].stato == DOING){
            move_card(cards[i].id, DOING, TO_DO, -1);
            // DA IMPLEMENTARE: invio all'utente che gli ho tolto la card
        }
    }

    return;
}

void move_card(int ID, int src, int dst, int porta){

    if(src == dst){
        return;
    }

    if(src > 4 || src < 1 || dst < 1 || dst > 4){
        printf("impossibile spostare la card, la colonna src o dst non esiste \n");
        return;
    }

    int i = 0;
    while (i < numero_card && cards[i].id != ID){
        i++;
    }
    if (i == numero_card){
        printf("impossibile spostare la card: non trovata");
        return;
    }

    cards[i].stato = dst;
    time(&cards[i].timestamp);
    show_lavagna();

    // se è una card che non era in TO_DO provo a riassegnarla
    if(dst == TO_DO){
        cards[i].porta_utente = -1;
    }

    // se non ho ricevuto l'ack in tempo la card viene rimessa in todo
    // oppure se è completata allora gli assegna un altra card
    if(dst == TO_DO || dst == DONE){
        handle_card();
    }

    if((src == TO_DO && dst == DOING) || (src == TO_DO && dst == HANDLED)){

        if(porta == -1){
            printf("impossibile spostare la card: non si può spostare da TO_DO verso doing/handled\n");
            return;
        }

        if(porta == -2){
            return;
        }
        cards[i].porta_utente = porta;
    }


    return;

}

void handle_card(void){

    sort_utenti();

    int card_assegnate = 0;

    for(int i = 0; i < MAX_UTENTI; i++){

        // verifico se l'utente è registrato
        if(utenti[i].porta == 0){
            continue;
        }

        // verifico se ha già una card in corso (le card DONE restano con la sua
        // porta come storico di chi le ha completate, ma non lo tengono occupato)
        int assegnata = 0;
        for(int j = 0; j < MAX_CARDS; j++){
            if(utenti[i].porta == cards[j].porta_utente && (cards[j].stato == HANDLED || cards[j].stato == DOING)){
                assegnata = 1;
                break;
            }
        }
        if(assegnata == 1){
            continue;
        }

        // non ha una card assegnata ed è registrato, quindi ne assegno una
        int k = 0;
        while(k < MAX_CARDS){
            if(cards[k].porta_utente == -1 && cards[k].stato == TO_DO){
                break;
            }
            k++;
        }

        if(k == MAX_CARDS){
            printf("Le card sono finite, non è possibile assegnarne una nuova all'utente in attesa \n");
            return;
        }

        move_card(cards[k].id,cards[k].stato,HANDLED,utenti[i].porta);

        card_assegnate++;

        printf("Assegnata la card con ID: %d \n",cards[k].id);

        // invio della card
        // formato invio ID | TESTO | PORTA1, PORTA2, ... | NUMERO UTENTI

        // preparo la lista delle altre porte; se è vuota ovvero c'è solo 1 utente
        // uso un placeholder "-", altrimenti il campo vuoto tra due "|" verrebbe
        // collassato dal parsing

        char lista_porte[MAX_UTENTI * 6 + 1];
        int offset = 0;
        lista_porte[0] = '\0';

        for(int j = 0; j < MAX_UTENTI; j++){
            // escludo il richiedente
            if(utenti[j].porta == utenti[i].porta || utenti[j].porta == 0){
                continue;
            }
            offset += sprintf(lista_porte + offset,"%d,",utenti[j].porta);
        }

        if(offset > 0){
            lista_porte[offset - 1] = '\0';
        } else {
            strcpy(lista_porte, "-");
        }

        memset(BUFFER_OUT,0,DIM_BUFFER);
        snprintf(BUFFER_OUT,DIM_BUFFER,"HANDLE_CARD|%d|%s|%s|%d",cards[k].id,cards[k].testo,lista_porte,utenti_registrati);

        if(invia_msg(utenti[i].socket, BUFFER_OUT)){
            printf("invio della card con ID: %d avvenuto con successo \n",cards[k].id);
        } else {
            printf("ERRORE: invio della card con ID:%d non riuscito o incompleto \n",cards[k].id);
        }

    }

    if(card_assegnate == 0){
        printf("non ci sono utenti liberi per assegnare card \n");
    } else {
        printf("assegnate: %d cards",card_assegnate);
        show_lavagna();
    }

    return;

}

/* ================================================ Vista board ================================================ */

/* costruisce in dest la riga r di una colonna larga COL_WIDTH:
   r == 0 e' la riga vuota di apertura, poi ogni card occupa 3 righe:
   ID, testo attivita', riga vuota di separazione */
static void riga_colonna(char *dest, int r, int *indici, int n_card){

    char contenuto[COL_WIDTH + 1];
    contenuto[0] = '\0';

    if(r > 0){
        int card = (r - 1) / 3;
        int sotto_riga = (r - 1) % 3;

        if(card < n_card){
            int idx = indici[card];

            if(sotto_riga == 0){
                snprintf(contenuto, sizeof(contenuto), " ID:%d", cards[idx].id);
            } else if(sotto_riga == 1){
                snprintf(contenuto, sizeof(contenuto), " %.*s", COL_WIDTH - 2, cards[idx].testo);
            }
        }
    }

    snprintf(dest, COL_WIDTH + 1, "%-*s", COL_WIDTH, contenuto);
}

void show_lavagna(void){

    // raccolgo gli indici delle card, divisi per colonna
    int idx_todo[MAX_CARDS], idx_doing[MAX_CARDS], idx_done[MAX_CARDS];
    int n_todo = 0, n_doing = 0, n_done = 0;

    for(int i = 0; i < numero_card; i++){
        switch(cards[i].stato){
            case TO_DO:
                idx_todo[n_todo++] = i;
                break;
            case HANDLED:   // assegnata ma non ancora confermata con ACK: resta in To Do
                idx_todo[n_todo++] = i;
                break;
            case DOING:
                idx_doing[n_doing++] = i;
                break;
            case DONE:
                idx_done[n_done++] = i;
                break;
        }
    }

    // una riga vuota di apertura piu' 3 righe per ogni card, con un'altezza minima fissa
    int righe = RIGHE_LAVAGNA;
    if(1 + n_todo * 3 > righe){
        righe = 1 + n_todo * 3;
    }
    if(1 + n_doing * 3 > righe){
        righe = 1 + n_doing * 3;
    }
    if(1 + n_done * 3 > righe){
        righe = 1 + n_done * 3;
    }

    printf("\n    _____________________________________________________________________________________________\n");
    printf("   /                                                                                            /|\n");
    printf("  /                                                                                            / |\n");
    printf(" /                                                                                            /  |\n");
    printf("|============================================================================================|   |\n");
    printf("|                                                                                            |   |\n");
    printf("|                                          LAVAGNA                                           |   |\n");
    printf("|                                                                                            |   |\n");
    printf("|============================================================================================|   |\n");
    printf("|_____________TO_DO_________________________DOING___________________________DONE_____________|   |\n");

    char col_todo[COL_WIDTH + 1], col_doing[COL_WIDTH + 1], col_done[COL_WIDTH + 1];

    for(int r = 0; r < righe; r++){
        riga_colonna(col_todo,  r, idx_todo,  n_todo);
        riga_colonna(col_doing, r, idx_doing, n_doing);
        riga_colonna(col_done,  r, idx_done,  n_done);

        printf("|%s|%s|%s|   |\n", col_todo, col_doing, col_done);
    }

    printf("|______________________________|______________________________|______________________________|   |\n");
    printf("|                                                                                            |  /\n");
    printf("|                                                                                            | / \n");
    printf("|____________________________________________________________________________________________|/  \n");
    return;
}

void lista_compatta(char *dest, int dest_size, int stato1, int stato2){
    int offset = 0;
    dest[0] = '\0';

    for(int i = 0; i < numero_card; i++){
        if(cards[i].stato != stato1 && cards[i].stato != stato2){
            continue;
        }

        // tolgo dal testo i separatori che usiamo nel messaggio, altrimenti
        // romperebbero il parsing lato utente
        char testo_sicuro[COL_WIDTH + 1];
        snprintf(testo_sicuro, sizeof(testo_sicuro), "%.30s", cards[i].testo);
        for(char *p = testo_sicuro; *p; p++){
            if(*p == '|' || *p == ';' || *p == ':'){
                *p = ' ';
            }
        }

        offset += snprintf(dest + offset, dest_size - offset, "%d:%s;", cards[i].id, testo_sicuro);
    }

    if(offset == 0){
        strcpy(dest, "-");
    } else {
        dest[offset - 1] = '\0';
    }
}

void show_lavagna_data_handler(int socket){
    char lista_todo[600], lista_doing[600], lista_done[600];

    lista_compatta(lista_todo, sizeof(lista_todo), TO_DO, HANDLED);
    lista_compatta(lista_doing, sizeof(lista_doing), DOING, -1);
    lista_compatta(lista_done, sizeof(lista_done), DONE, -1);

    memset(BUFFER_OUT,0,DIM_BUFFER);
    snprintf(BUFFER_OUT,DIM_BUFFER,"SHOW_LAVAGNA_DATA|%s|%s|%s",lista_todo,lista_doing,lista_done);

    invia_msg(socket, BUFFER_OUT);

    return;
}
