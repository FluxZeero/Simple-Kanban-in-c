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
#include "stato.h"
#include "rete.h"
#include "handlers.h"

int main(){

    // azzero i set
    FD_ZERO(&fd_lettura);
    FD_ZERO(&fd_temp);

    memset(BUFFER_IN,0,DIM_BUFFER);
    memset(BUFFER_OUT,0,DIM_BUFFER);

    socket_ascolto = socket(AF_INET, SOCK_STREAM, 0); // genero un socket globale,tcp,protocollo standard

    if(socket_ascolto < 0){ // controllo che il socket sia stato generato correttamente
        perror("errore di creazione del socket \n");
        exit(1);
    }

    // permette di riavviare subito la lavagna sulla stessa porta senza aspettare il TIME_WAIT
    int riuso = 1;
    setsockopt(socket_ascolto, SOL_SOCKET, SO_REUSEADDR, &riuso, sizeof(riuso));

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

    show_lavagna();
    printf("Lavagna online alla porta %d. \n Operazioni possibili da tastiera: \nCREATE_CARD|ID|COLONNA|TESTO_ATTIVITA|SHOW_LAVAGNA|SEND_USER_LIST|HANDLE_CARD|CLOSE\n",PORTA_LAVAGNA);

    int stdin_attivo = 1;

    // ciclo infinito che inizia mettendosi in attesa di una richiesta da un descrittore che ha ricevuto dati
    while(1){
        FD_ZERO(&fd_lettura);
        FD_SET(socket_ascolto,&fd_lettura);
        if(stdin_attivo){
            FD_SET(STDIN_FILENO,&fd_lettura);
        }

        // inizializzare tutti gli utenti che si sono collegati alla lavagna
        for(int i = 0; i < MAX_UTENTI; i++){
            if(utenti[i].attivo){
                FD_SET(utenti[i].socket, &fd_lettura);
            }
        }

        tv.tv_sec = 5;
        tv.tv_usec = 0;
        // mi metto in attesa di una richiesta in arrivo su una delle potre
        int n_richieste = select(max_fd + 1, &fd_lettura, NULL, NULL, &tv);

        if (n_richieste < 0){

        }

        if (n_richieste == 0){
            ping_user();
            continue;
        }

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

                else if (i == STDIN_FILENO){
                    // ho rilvato una riga dal terminale

                    memset(BUFFER_IN,0,DIM_BUFFER);
                    if(fgets(BUFFER_IN,DIM_BUFFER,stdin) == NULL){
                        // EOF su stdin: smetto di osservarlo, altrimenti select()
                        // lo segnala "pronto" per sempre e il ciclo gira a vuoto
                        printf("stdin chiuso (EOF): comandi da tastiera disattivati \n");
                        stdin_attivo = 0;
                        continue;
                    }
                    // tolgo il ritorno carrello presente nella riga di comando
                    BUFFER_IN[strcspn(BUFFER_IN, "\n")] = '\0';
                    printf("riga letta: %s \n",BUFFER_IN);
                    char *campi[MAX_CAMPI];
                    int n_campi = parse_msg(campi,BUFFER_IN,MAX_CAMPI,"|");

                    call_handler(i, campi,n_campi);
                }

                else {
                    memset(BUFFER_IN,0,DIM_BUFFER);
                    int n = recv(i,BUFFER_IN,DIM_BUFFER - 1,0);

                    if (n < 0) {
                        printf("errore nella richiesta da parte del socket %d",i);
                        continue;
                    }

                    BUFFER_IN[n] = '\0';

                    if(n == 0){
                        // chiusura della connessione forzata senza quit
                        quit_handler(i);
                    }

                    else if (n > MAX_MSG){
                        printf("il testo del messaggio è troppo lungo");
                    }

                    else {
                        // tolgo il terminatore di fine messaggio prima del parsing
                        char *fine = strchr(BUFFER_IN, '\n');
                        if(fine != NULL){
                            *fine = '\0';
                        }

                        // servo la richiesta: utilizzo una funzione per chiamare il giusto handler in base al messaggio ricevuto

                        char *campo[MAX_CAMPI];
                        char* sep = "|";
                        int n_campi = parse_msg(campo, BUFFER_IN, MAX_CAMPI, sep);

                        call_handler(i, campo, n_campi);
                    }
                }
            }
        }
    }
    close(socket_ascolto);

    return 0;
}
