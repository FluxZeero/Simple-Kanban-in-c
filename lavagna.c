#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include "costanti.h"

/* ============================================ Strutture Dati ============================================ */

typedef struct {
    int porta;    /* porta dell'utente (il suo "nome") */
    int socket;   /* socket TCP su cui gli parlo */
    int attivo;   /* 1 = presente, 0 = slot libero */
} struct_utenti;

typedef struct {
    int id;
    int colonna;
    char testo[DIM_TESTO];
    int porta_utente;
    int stato;
    char* timestamp;
} struct_card;

/* =========================================== Variabili Globali =========================================== */

struct_utenti utenti[MAX_UTENTI];

int utenti_attivi = 0;
int utenti_registrati = 0;

fd_set fd_lettura; //selezine degli utenti da cui mi aspetto di leggere
fd_set fd_temp; // temporaneo, utilizzato per salvare il contenuto di fd_lettura prima dell'uso di select
int max_fd = 0;

/* =========================================== Funzioni di supporto =========================================== */


void init_utenti(){
    
    for(int i = 0; i < MAX_UTENTI; i++){
        utenti[i].attivo = 0;
        utenti[i].socket = -1;
        utenti[i].porta = (PORTA_LAVAGNA + 1) + i;
    }
    return;
}


/* ================================================= Main ================================================== */
int main(){

    // azzero i set
    FD_ZERO(&fd_lettura);
    FD_ZERO(&fd_temp);

    int socket_ascolto = socket(AF_INET, SOCK_STREAM, 0); // genero un socket globale,tcp,protocollo standard

    if(socket_ascolto < 0){ // controllo che il socket sia stato generato correttamente
        perror("errore di creazione del socket \n");
        exit(1);
    }

    max_fd = socket_ascolto;
    
    init_utenti();

    struct sockaddr_in ind_lavagna;
    memset(&ind_lavagna,0,sizeof(ind_lavagna));
    ind_lavagna.sin_family = AF_INET; // comunicazioni globali
    ind_lavagna.sin_port = htons(5678); // gli assegno una porta
    inet_pton(AF_INET,"127.0.0.1",&ind_lavagna.sin_addr.s_addr); // gli assegno l'indirizzo IP della porta di loopback 

    // adesso attacco il socket all'indirizzo della lavagna
    if(bind(socket_ascolto, (struct sockaddr*)&ind_lavagna, sizeof(ind_lavagna))==-1){
        perror("bind non creato \n");
        exit(1);
    }

    if(listen(socket_ascolto,MAX_UTENTI + 1) < 0){
        perror("impossibile ascoltare sul socket \n");
        exit(1);
    };

    printf("Lavagna online alla porta %d. Operazioni possibili | \n",PORTA_LAVAGNA);


    // ciclo infinito che inizia mettendosi in attesa di una richiesta da un descrittore che ha ricevuto dati
    while(1){
        FD_ZERO(&fd_lettura);
        FD_SET(socket_ascolto,&fd_lettura);

        // inizializzare tutti gli utenti che si sono collegati alla lavagna
        for(int i = 0; i < MAX_UTENTI; i++){
            if(utenti[i].attivo){
                FD_SET(utenti[i].socket, &fd_lettura);
            }
        }

        // mi metto in attesa di una richiesta in arrivo su una delle potre
        select(max_fd + 1, &fd_lettura, NULL, NULL, NULL);

        // trovo la richiesta che mi ha fatto sbloccare
        for(int i = 0; i<max_fd; i++){
            if(FD_ISSET(i, &fd_lettura)){
                if(i == socket_ascolto) {
                    //nuova richiesta di connessione
                    struct sockaddr_in ind_utente;
                    
                    socklen_t len = sizeof(ind_utente);
                    int socket_client = accept(socket_ascolto, (struct sockaddr*)&ind_utente, &len);
                    
                    if(socket_client < 0){
                        perror("errore di creazione socket client \n");
                        exit(1);
                    }

                    int k = 0;
                    while(k < MAX_UTENTI && utenti[k].attivo){
                        k++;
                    }

                    if(k == MAX_UTENTI){
                        printf("massimo di utenti raggiunti, aspettare che un utente esca \n");
                        close(socket_client);
                    } else {
                        utenti[k].socket = socket_client;
                        utenti[k].attivo = 1;
                        if(socket_client > max_fd){
                            max_fd = socket_client;
                        }
                    }

                } 
                //
                else {
                // gestione del dato
                }
           }
        }
    }
    close(socket_ascolto);

    return 0;
}


/* DA RIMUOVERE -> Parte di setup iniziale "Hello server"


    // mi metto in ascolto delle richieste scon il socket, posso avere un massimo di richieste in coda
    int ret = listen(socket_ascolto,10); 
    if(ret < 0){
        perror("impossibile effettuare listen \n");
    }

    struct sockaddr_in ind_utente;
    int len = sizeof(ind_utente);
    int socket_client = accept(socket_ascolto, (struct sockaddr*)&ind_utente, &len); 

    // mi passa il socket della conversazione e inizializza la struttura che gli ho passato con i dati dell'utente
    // adesso ho accettato la connessione dal client e adesso posso mettermi a ricevere la roba 
    
    char buffer[1024];
    int n = recv(socket_client, buffer, sizeof(buffer) - 1, 0);
    //ho ricevuto dal socket_client, nel buffer, un messaggio di dimensione che io prefisso, con flag 0
    buffer[n] = '\0'; // metto la marca di fine stringa per stampare e utilizzare il buffer come una stringa, se il messaggio dell'utente è troppo lungo ce la devo mettere io

    printf("ricevuto %s \n",buffer);
    //chiudo le connessioni
*/