#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include "stato.h"
#include "rete.h"

/* ============================================ Variabili Globali ============================================ */

fd_set fd_lettura;
fd_set fd_temp;

int socket_P2P;
int max_fd;
int my_port;
struct timeval tv;
char BUFFER_IN[DIM_BUFFER];
char BUFFER_OUT[DIM_BUFFER];
struct_utenti utenti[MAX_UTENTI];
struct_curr_card curr_card;

int card_done = 0;
int reviewed = 0;
int in_review = 0;
time_t review_iniziata = 0;

int socket_lavagna;

/* ============================================ Inizializzazione ============================================= */

void init_utenti(void){

    for(int i = 0; i < MAX_UTENTI; i++){
        utenti[i].socket = 0;
        utenti[i].review_ack = 0;
        utenti[i].attivo = 0;
    }

    return;
}

/* ======================================= Ricezione messaggi e connessioni ==================================== */

int get_msg(int socket){

    memset(BUFFER_IN,0,DIM_BUFFER);
    int n = recv(socket,BUFFER_IN,DIM_BUFFER - 1,0);

    if (n < 0) {
        printf("errore nella richiesta da parte del socket %d",socket);
        return 0;
    }

    BUFFER_IN[n] = '\0';

    if (n == 0){
        printf("il socket %d si è disconnesso \n",socket);
        if(socket == socket_lavagna){
            printf("connessione con la lavagna persa, chiudo il client \n");
            close(socket_lavagna);
            close(socket_P2P);
            exit(0);
        }
        close_handler(socket);
        return 0;
    }

    else if (n > MAX_MSG){
        printf("il testo del messaggio è troppo lungo");
        return 0;
    }

    // tolgo il terminatore di fine messaggio prima del parsing
    char *fine = strchr(BUFFER_IN, '\n');
    if(fine != NULL){
        *fine = '\0';
    }

    return 1;
}

void close_handler(int socket){
    for (int i = 0; i < MAX_UTENTI; i++){
        if(utenti[i].socket == socket){
            close(utenti[i].socket);
            utenti[i].socket = 0;
            utenti[i].porta = 0;
            utenti[i].review_ack = 0;
            utenti[i].attivo = 0;
        }
    }

    if(in_review){
        // il peer perso poteva essere uno di quelli da cui aspettavo la review:
        // richiedo di nuovo la lista aggiornata e rimando la richiesta a chi
        // è rimasto attivo (o a chi si è collegato nel frattempo)
        memset(BUFFER_OUT,0,DIM_BUFFER);
        sprintf(BUFFER_OUT,"REQUEST_USER_LIST");
        invia_msg(socket_lavagna,BUFFER_OUT);
    }

    return;
}

void *process_card(void *arg){
    int secondi = *(int*)arg;
    free(arg);

    // simulazione del tempo di lavoro sulla card
    sleep(secondi);

    printf("ho finito la card con id: %d, chiedo una review\n",curr_card.ID);
    // segnalo al thread principale che il lavoro è finito: si occuperà lui di
    // chiedere la lista utenti aggiornata e far partire la review
    card_done = 1;

    return NULL;
}

void avvia_processo_card(int secondi) {
    pthread_t thread_id;

    // Alloca dinamicamente il parametro per evitare race condition sulle variabili locali
    int *arg = malloc(sizeof(int));
    *arg = secondi;

    if (pthread_create(&thread_id,NULL,process_card,arg) != 0) {
        perror("Errore creazione thread");
        free(arg);
        return;
    }
    pthread_detach(thread_id);
    // Il main prosegue immediatamente senza bloccarsi
}

void connect_to_user(int porta){

    // controlla se si è già connesso utilizzando il campo attivo della struct
    int i = 0;
    while(i < MAX_UTENTI){
        if(utenti[i].porta == porta){
            return;
        }
        i++;
    }

    struct sockaddr_in ind_utente;
    memset(&ind_utente,0,sizeof(ind_utente));

    ind_utente.sin_family = AF_INET;
    inet_pton(AF_INET,"127.0.0.1", & ind_utente.sin_addr);
    ind_utente.sin_port = htons(porta);

    int socket_utente = socket(AF_INET,SOCK_STREAM, 0);

    socklen_t client_len = sizeof(ind_utente);

    if (connect(socket_utente,(struct sockaddr*)&ind_utente,client_len) < 0){
        printf("errore nella connessione con l'utente di porta: %d",porta);
        close(socket_utente);
        return;
    }

    // mi presento all'utente con la mia porta reale di ascolto
    memset(BUFFER_OUT,0,DIM_BUFFER);
    sprintf(BUFFER_OUT,"HELLO_PEER|%d",my_port);
    invia_msg(socket_utente,BUFFER_OUT);

    // salvo i suoi valori nella struct
    for(int i = 0; i < MAX_UTENTI; i++){
        if(utenti[i].attivo == 0){
            utenti[i].porta = porta;
            utenti[i].attivo = 1;
            utenti[i].review_ack = 0;
            utenti[i].socket = socket_utente;
            break;
        } else if( (utenti[i].attivo == 1) && (i == MAX_UTENTI - 1) ){
            printf("massimo di utenti raggiunti impossibile collegarsi al client con porta %d",porta);
            close(socket_utente);
            return;
        }
    }
    return;
}
