#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

int main(){

    int socket_invio = socket(AF_INET, SOCK_STREAM,0);
    if (socket_invio < 0){
        perror("errore nella creazione del socket");
        return 0;
    }

    // inizializzo la struttura dati

    struct sockaddr_in my_addr;
    memset(&my_addr,0,sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(5678);
    inet_pton(AF_INET,"127.0.0.1",&my_addr.sin_addr);


    // adesso posso provare a connettermi
    int len = sizeof(my_addr);
    int ret = connect(socket_invio,(struct sockaddr*)&my_addr,len);
    if(ret < 0){
        perror("errore di connessione col server\n");
        return 0;
    }
    
    // gli invio dei messaggi
    char msg[30];
    strcpy(msg,"Hello server! \n");

    ret = send(socket_invio,msg,strlen(msg),0);

    if(ret < strlen(msg)){
        perror("sono stati inviati meno dati dei previsti");
        return 0;
    }

    return 0;
}   