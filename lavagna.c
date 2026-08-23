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
#include <time.h>

/* ============================================ Strutture Dati ============================================ */

typedef struct {
    int porta;    /* porta dell'utente (il suo "nome") se è 0 non si è registrato*/
    int socket;   /* socket TCP su cui gli parlo */
    int attivo;   /* 1 = presente, 0 = slot libero */
} struct_utenti;

typedef struct {
    int id;
    char testo[DIM_TESTO];
    int porta_utente;
    int stato; // -1 non valid
    time_t timestamp;
} struct_card;

/* =========================================== Variabili Globali =========================================== */

struct_utenti utenti[MAX_UTENTI];
struct_card cards[MAX_CARDS];

int utenti_attivi = 0;
int utenti_registrati = 0;

fd_set fd_lettura; //selezine degli utenti da cui mi aspetto di leggere
fd_set fd_temp; // temporaneo, utilizzato per salvare il contenuto di fd_lettura prima dell'uso di select
int max_fd = 0;

const char *testi_iniziali[15] = {
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

/* =========================================== Funzioni di supporto =========================================== */


void init_utenti(){
    
    for(int i = 0; i < MAX_UTENTI; i++){
        utenti[i].attivo = 0;
        utenti[i].socket = -1;
        utenti[i].porta = 0;
    }
    return;
}

void init_cards(){
    for (int i = 0; i < 15; i++){
        cards[i].id = i;
        cards[i].porta_utente = -1; // non assegnata
        cards[i].stato = TO_DO;
        strcpy(cards[i].testo,testi_iniziali[i]);
        cards[i].testo[DIM_TESTO - 1] = '\0';
        time(&cards[i].timestamp);
    }
}

void hello_handler(int socket_utente,char porta[MAX_MSG]){
    int i = 0;
    while(utenti[i].socket != socket_utente && i < MAX_UTENTI){
        i++;
    }

    if(i == MAX_UTENTI){
        perror("ERRORE LOGICO, HO ACCETTATO UN MESSAGGIO DA UN UTENTE NON CONNESSO");
        exit(1);
    }

    int intporta = atoi(porta);
    utenti[i].porta = intporta;
    utenti_registrati++;
    printf("Registrato utente con porta: %s \n", porta);
    return;
}

void create_card_handler(int ID, int colonna, char* testo){
    return;
}

void rimuovi_card_utente(int socket){
    
    return;
}


/* ================================================= Main ================================================== */
int main(){

    // azzero i set
    FD_ZERO(&fd_lettura);
    FD_ZERO(&fd_temp);
    char BUFFER_IN[DIM_BUFFER];
    char BUFFER_OUT[DIM_BUFFER];
    memset(BUFFER_IN,0,DIM_BUFFER);
    memset(BUFFER_OUT,0,DIM_BUFFER);

    int socket_ascolto = socket(AF_INET, SOCK_STREAM, 0); // genero un socket globale,tcp,protocollo standard

    if(socket_ascolto < 0){ // controllo che il socket sia stato generato correttamente
        perror("errore di creazione del socket \n");
        exit(1);
    }

    max_fd = socket_ascolto;
    
    init_utenti();
    init_cards();

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
        for(int i = 0; i <= max_fd; i++){
            if(FD_ISSET(i, &fd_lettura)){
                if(i == socket_ascolto) {
                    //nuova richiesta di connessione
                    struct sockaddr_in ind_utente;
                    
                    socklen_t len = sizeof(ind_utente);
                    int socket_client = accept(socket_ascolto, (struct sockaddr*)&ind_utente, &len);

                    if(socket_client < 0) {
                        perror("impossibile creare un nuovo socket");
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
                        utenti_attivi++;
                        if(max_fd < socket_client){
                            max_fd = socket_client;
                        }
                    }
                    
                } 

                else {
                // gestione del dato
                    memset(BUFFER_IN,0,DIM_BUFFER);
                    int n = recv(i,BUFFER_IN,DIM_BUFFER - 1,0);
                    BUFFER_IN[DIM_BUFFER - 1] = '\0';
                    if(n == 0){
                        // chiusura della connessione forzata senza quit
                        utenti[i].attivo = 0;
                        utenti[i].socket = 0;
                        FD_CLR(i,&fd_lettura);
                        rimuovi_card_utente(i);
                    } 

                    else if (n < 0) {
                        printf("errore nella richiesta da parte del socket %d",i);
                        continue;
                    } 
                    
                    else {
                        // servo la richiesta

                        // implementazione in c di split()
                        char *campo[MAX_CAMPI];
                        int n_campi = 0;

                        char *token = strtok(BUFFER_IN, "|");
                        while(token != NULL && n_campi < MAX_CAMPI){
                            campo[n_campi] = token;
                            n_campi++;
                            token = strtok(NULL, "|");
                        }

                        if(strcmp(campo[0],"HELLO") == 0 && n_campi == 2){
                            hello_handler(i,campo[1]);
                        } 

                        else if (strcmp(campo[0],"CREATE_CARD") == 0 && n_campi == 4){
                            int id = atoi(campo[1]);
                            int colonna = atoi(campo[2]);
                            create_card_handler(id,colonna,campo[3]);
                        }

                        else {
                            printf("ricevuto comando non valido: %s",BUFFER_IN);
                        }
                    }

                    
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