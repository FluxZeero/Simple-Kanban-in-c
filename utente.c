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



fd_set fd_lettura;
fd_set fd_temp;
char BUFFER_IN[DIM_BUFFER];
char BUFFER_OUT[DIM_BUFFER];

int main(int argc, char* argv[]){

    if(argc != 2){
        fprintf(stderr, "uso: %s <porta>\n", argv[0]);
        exit(1);
    }

    FD_ZERO(&fd_lettura);
    FD_ZERO(&fd_temp);

    int my_port = atoi(argv[1]);

    // creo un socket per la comunicazione con la lavagna
    int socket_lavagna = socket(AF_INET, SOCK_STREAM,0);
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

    // adesso posso provare a connettermi
    len = sizeof(ind_lavagna);
    int ret = connect(socket_lavagna,(struct sockaddr*)&ind_lavagna,len);
    if(ret < 0){
        perror("errore di connessione col server\n");
        return 0;
    }



    while(1){
        
        FD_SET(0,&fd_lettura);
        FD_SET(socket_P2P,&fd_lettura);
        FD_SET(socket_lavagna,&fd_lettura);
        int max_fd;

        if(socket_P2P > socket_lavagna){
            max_fd = socket_P2P;
        } else {
            max_fd = socket_lavagna;
        }
        
        select(max_fd,&fd_lettura,NULL,NULL,NULL);

        }

    return 0;
}   

/* DA RIMUOVERE
    
    // gli invio dei messaggi
    char msg[30];
    strcpy(msg,"Hello server! \n");

    ret = send(socket_lavagna,msg,strlen(msg),0);

    if(ret < strlen(msg)){
        perror("sono stati inviati meno dati dei previsti");
        return 0;
    }
*/