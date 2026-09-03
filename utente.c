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
    int socket;
    int review_ack; // 1 se l'utente ha già mandato ack per la review
    int attivo;
} struct_utenti;

typedef struct {
    int ID;
    char* testo;
} struct_curr_card;

/* =========================================== Variabili Globali =========================================== */

fd_set fd_lettura;
fd_set fd_temp;

int n_utenti = 0;
char BUFFER_IN[DIM_BUFFER];
char BUFFER_OUT[DIM_BUFFER];
struct_utenti utenti[MAX_UTENTI];
struct_curr_card curr_card;

int socket_lavagna;

/* ========================================== Funzioni di supporto ========================================== */

int parse_msg(char *dst[],char *src, int numero, char* sep){
    
    int n_campi = 0;
    char *token = strtok(src, sep);
    while(token != NULL && n_campi < numero){
        dst[n_campi] = token;
        n_campi++;
        token = strtok(NULL, sep);
    }

    return n_campi;
}

void init_utenti(){

    for(int i = 0; i < MAX_UTENTI; i++){
        utenti[i].socket = 0;
        utenti[i].review_ack = 0;
        utenti[i].attivo = 0;
    }

    return;
}

// riceve il messaggio dal socket e lo mette in BUFFER_IN restituisce 0 se c'è errore
int get_msg(int socket){

    memset(BUFFER_IN,0,DIM_BUFFER);
    int n = recv(socket,BUFFER_IN,DIM_BUFFER - 1,0);
    BUFFER_IN[n] = '\0';

    if (n < 0) {
        printf("errore nella richiesta da parte del socket %d",socket);
        return 0;
    }

    if (n == 0){
        printf("il socket %d si è disconnesso \n",socket);
        close_handler();
    }
                
    else if (n > MAX_MSG){
        printf("il testo del messaggio è troppo lungo");
        return 0;
    }

    return 1;
}

// DA IMPLEMENTARE
void close_handler(){
    return;
}

// DA IMPLEMENTARE
void connect_to_all_users(){
    return;
}

/* =========================================== Funzionalità del progetto =========================================== */


void pong_handler(){
    // invia il pong alla lavagna
    memset(BUFFER_OUT,0,DIM_BUFFER);
    sprintf(BUFFER_OUT,"PONG");
    if(send(socket_lavagna,BUFFER_OUT,strlen(BUFFER_OUT),0) < 0){
        printf("errore nell'invio del pong");
    }
    return;
}

// inizializza la card se ID = -1 altrimenti aggiorna la struttura current card
void card_handler(int ID, char* testo, int utenti_lav){
    if (ID == -1){
        curr_card.ID = 0;
        curr_card.testo = 0;
    } else {
        curr_card.ID = ID;
        curr_card.testo = testo;
        if(utenti_lav > n_utenti - 1){
            // DA IMPLEMENTARE: devo aggiornare il numero di utenti c'è qualcuno con cui non sono conness
            connect_to_all_users();
        }
    }


    return;
}


void call_handler(int socket_utente, char *campo[MAX_CAMPI], int n_campi){
    
    if(n_campi == 0){
        printf("ricevuto comando non valido/non esistente: %s , n_campi: %d, socket chiamante %d \n",BUFFER_IN,n_campi, socket_utente);
        return;
    }
    if (socket_utente == socket_lavagna && strcmp(campo[0],"PING") == 0 && n_campi == 1){
        pong_handler();
    }

    else if (socket_utente == socket_lavagna && strcmp(campo[0],"HANDLE_CARD") == 0 && n_campi >= 4){
        card_handler(atoi(campo[1]),campo[2],atoi(campo[3]));
    }
    /*
    if(strcmp(campo[0],"HELLO") == 0 && n_campi == 2){
        hello_handler(socket_utente,campo[1]);
    } 
    */

    // se nessun comando ha rispettato il formato comunico al client l'errore
    else {
        printf("ricevuto comando non valido/non esistente: %s , n_campi: %d, socket chiamante %d \n",BUFFER_IN,n_campi, socket_utente);
    }
}

/* ================================================= Main ================================================== */


int main(int argc, char* argv[]){

    if(argc != 2){
        fprintf(stderr, "uso: %s <porta>\n", argv[0]);
        exit(1);
    }

    FD_ZERO(&fd_lettura);
    FD_ZERO(&fd_temp);

    int my_port = atoi(argv[1]);

    // creo un socket per la comunicazione con la lavagna
    socket_lavagna = socket(AF_INET, SOCK_STREAM,0);
    if (socket_lavagna < 0){
        perror("errore nella creazione del socket");
        exit(1);
    }

    // inizializzo la struttura per la comunicazione con la lavagna
    struct sockaddr_in ind_lavagna;
    memset(&ind_lavagna,0,sizeof(ind_lavagna));
    ind_lavagna.sin_family = AF_INET;
    ind_lavagna.sin_port = htons(5678);
    inet_pton(AF_INET,"127.0.0.1",&ind_lavagna.sin_addr);

    socklen_t len = sizeof(ind_lavagna);

    // creo il socket di ascolto per la connessione P2P
    int socket_P2P = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_P2P < 0){
        perror("errore nella creazione del socket");
        exit(1);
    }

    // inizializzo la stuttura in ascolto
    struct sockaddr_in ind_P2P;
    memset(&ind_P2P,0,sizeof(ind_P2P));
    ind_P2P.sin_family = AF_INET;
    ind_P2P.sin_port = htons(my_port);
    inet_pton(AF_INET,"127.0.0.1",&ind_P2P.sin_addr);

    len = sizeof(ind_P2P);
    if (bind(socket_P2P, (struct sockaddr*)&ind_P2P, len) < 0){
        perror("errore nel bind del socket P2P");
        exit(1);
    }

    if(listen(socket_P2P,MAX_UTENTI) < 0){
        perror("errore listen socket_P2P");
        exit(1);
    }

    // adesso posso provare a connettermi
    len = sizeof(ind_lavagna);
    int ret = connect(socket_lavagna,(struct sockaddr*)&ind_lavagna,len);
    if(ret < 0){
        perror("errore di connessione col server\n");
        return 0;
    } else {

        // invio il messaggio di HELLO dopo la connessione
        printf("mi sono connesso \n");
        memset(BUFFER_OUT,0,DIM_BUFFER);
        sprintf(BUFFER_OUT,"HELLO|%d",my_port);
        BUFFER_OUT[strlen(BUFFER_OUT)] = '\0';

        int n = send(socket_lavagna,BUFFER_OUT,strlen(BUFFER_OUT),0);

        if (n < strlen(BUFFER_OUT)){
            printf("errore nell'invio del messaggio: %s", BUFFER_OUT);
            return;
        }

    }

    init_utenti();

    while(1){
        FD_ZERO(&fd_lettura);
        FD_SET(STDIN_FILENO,&fd_lettura);
        FD_SET(socket_P2P,&fd_lettura);
        FD_SET(socket_lavagna,&fd_lettura);
        int max_fd = 0;
        
        if(socket_P2P > socket_lavagna){
            max_fd = socket_P2P;
        } else {
            max_fd = socket_lavagna;
        }

        for (int i = 0; i < MAX_UTENTI; i++){
            if (utenti[i].attivo){
                FD_SET(utenti[i].socket,&fd_lettura);
            }
            if(max_fd < utenti[i].socket){
                max_fd = utenti[i].socket;
            }
        }

        
        select(max_fd + 1,&fd_lettura,NULL,NULL,NULL);

        // gli faccio fare la request_user_list per updatare il numero di persone connesse alla lavagna

        for(int i = 0; i <= max_fd; i++){
            if(i == socket_P2P){
                // mi è arrivata una richiesta di connessione da un utente
                struct sockaddr_in ind_utente;
                len = sizeof(ind_utente);

                int socket_utente = accept(socket_P2P,(struct sockaddr*)&ind_utente,&len);
                if(socket_utente < 0) {
                    perror("impossibile creare un nuovo socket");
                    exit(1);
                }
                
                int k = 0;
                while(k < MAX_UTENTI && utenti[k].attivo){
                    k++;
                }
                if(k == MAX_UTENTI){
                    printf("massimo di utenti raggiunti, aspettare che un utente esca \n");
                    close(socket_utente);
                } else {
                    utenti[k].socket = socket_utente;
                    utenti[k].attivo = 1;
                    n_utenti ++;
                    if(max_fd < socket_utente){
                        max_fd = socket_utente;
                    }
                }
            }

            else if (i == STDIN_FILENO){

                // ho rilvato una riga dal terminale 
                memset(BUFFER_IN,0,DIM_BUFFER);
                fgets(BUFFER_IN,DIM_BUFFER,stdin);
                // tolgo il ritorno carrello presente nella riga di comando 
                BUFFER_IN[strcspn(BUFFER_IN, "\n")] = '\0';
                printf("riga letta: %s \n",BUFFER_IN);
                char *campi[MAX_CAMPI];
                int n_campi = parse_msg(campi,BUFFER_IN,MAX_CAMPI,"|");
                    
                call_handler(i, campi,n_campi);
            }

            else if (i == socket_lavagna){
                // ho ricevuto un messaggio in entrata dalla lavagna: handle_card | ping 
                get_msg(i);
                char *campi[MAX_CAMPI];
                int n_campi = parse_msg(campi,BUFFER_IN,MAX_CAMPI,"|");
                call_handler(i,campi,n_campi);

            } else {

                // controllo se è il socket di un utente del kanban
                int k = 0;
                while(k < MAX_UTENTI){
                    if (utenti[k].socket == i){
                        break;
                    }
                    k ++;
                }
                if (get_msg(i) == 0){
                    continue;
                }
                char *campi[MAX_CAMPI];
                int n_campi = parse_msg(campi,BUFFER_IN,MAX_CAMPI,"|");
                call_handler(i,campi,n_campi);
            }
        }

        //gestisco i ritorni P2P

    }   
    return 0;
}