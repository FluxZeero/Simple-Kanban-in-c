#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>


int main(){
    int socket_ascolto = socket(AF_INET, SOCK_STREAM, 0); // genero un socket globale,tcp,protocollo standard

    if(socket_ascolto < 0){ // controllo che il socket sia stato generato correttamente
        perror("errore di creazione del socket \n");
        return 0;
    }
    
    struct sockaddr_in ind_lavagna;
    memset(&ind_lavagna,0,sizeof(ind_lavagna));
    ind_lavagna.sin_family = AF_INET; // comunicazioni globali
    ind_lavagna.sin_port = htons(5678); // gli assegno una porta
    inet_pton(AF_INET,"127.0.0.1",&ind_lavagna.sin_addr.s_addr);

    // adesso attacco il socket all'indirizzo della lavagna
    if(bind(socket_ascolto, (struct sockaddr*)&ind_lavagna, sizeof(ind_lavagna))==-1){
        perror("bind non creato \n");
        return 0;
    }

    // mi metto in ascolot delle richieste scon il socket, posso avere un massimo di richieste in coda
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
    close(socket_client);
    close(socket_ascolto);

    return 0;
}