#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <time.h>
#include "costanti.h"
#include "stato.h"
#include "rete.h"
#include "handlers.h"

int main(int argc, char* argv[]){

    if(argc != 2){
        fprintf(stderr, "uso: %s <porta>\n", argv[0]);
        exit(1);
    }

    FD_ZERO(&fd_lettura);
    FD_ZERO(&fd_temp);

    my_port = atoi(argv[1]);

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
    ind_lavagna.sin_port = htons(PORTA_LAVAGNA);
    inet_pton(AF_INET,"127.0.0.1",&ind_lavagna.sin_addr);

    socklen_t len = sizeof(ind_lavagna);

    // creo il socket di ascolto per la connessione P2P
    socket_P2P = socket(AF_INET, SOCK_STREAM, 0);
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
        close(socket_lavagna);
        return 0;
    } else {

        // invio il messaggio di HELLO dopo la connessione
        printf("mi sono connesso \n");
        memset(BUFFER_OUT,0,DIM_BUFFER);
        sprintf(BUFFER_OUT,"HELLO|%d",my_port);

        if(!invia_msg(socket_lavagna,BUFFER_OUT)){
            printf("errore nell'invio del messaggio: %s", BUFFER_OUT);
            return 1;
        }

    }

    init_utenti();

    while(1){
        FD_ZERO(&fd_lettura);
        FD_SET(STDIN_FILENO,&fd_lettura);
        FD_SET(socket_P2P,&fd_lettura);
        FD_SET(socket_lavagna,&fd_lettura);
        max_fd = 0;

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


        tv.tv_sec = 2;
        tv.tv_usec = 0;
        int n_pronti = select(max_fd + 1,&fd_lettura,NULL,NULL,&tv);

        // controllo periodico dei flag impostati dal thread di lavoro / dagli handler
        // di review, indipendentemente dal fatto che sia arrivato un messaggio o no
        if(card_done){
            card_done = 0;
            printf("la card è pronta chiedo la lista degli utenti per la review \n");
            in_review = 1;
            time(&review_iniziata);
            memset(BUFFER_OUT,0,DIM_BUFFER);
            sprintf(BUFFER_OUT,"REQUEST_USER_LIST");
            invia_msg(socket_lavagna,BUFFER_OUT);
        }

        if(reviewed){
            reviewed = 0;
            in_review = 0;
            memset(BUFFER_OUT,0,DIM_BUFFER);
            sprintf(BUFFER_OUT,"CARD_DONE|%d",curr_card.ID);
            invia_msg(socket_lavagna,BUFFER_OUT);
            for(int i = 0; i < MAX_UTENTI; i++){
                utenti[i].review_ack = 0;
            }
        }

        if(in_review && (time(NULL) - review_iniziata >= 5)){
            ritenta_review();
        }

        if(n_pronti <= 0){
            continue;
        }

        for(int i = 0; i <= max_fd; i++){
            if(!FD_ISSET(i, &fd_lettura)){
                continue;
            }

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
                    utenti[k].porta = 0; // sconosciuta finché non arriva HELLO_PEER
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

    }
    return 0;
}
