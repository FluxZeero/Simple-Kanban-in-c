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
    int porta;    /* porta dell'utente se è 0 non si è registrato*/
    int socket;   /* socket TCP su cui gli parlo */
    int attivo;   /* 1 = presente, 0 = slot libero */
} struct_utenti;

typedef struct {
    int id; 
    char testo[DIM_TESTO];
    int porta_utente; // -1 = non assegnata a nessun utente
    int stato; // -1 non valid
    time_t timestamp;
} struct_card;

/* =========================================== Variabili Globali =========================================== */

struct_utenti utenti[MAX_UTENTI];
struct_card cards[MAX_CARDS];

char BUFFER_IN[DIM_BUFFER];
char BUFFER_OUT[DIM_BUFFER];

int utenti_attivi = 0;
int utenti_registrati = 0;
int numero_card = 0;
int assigned_card = 0;

fd_set fd_lettura; //selezine degli utenti da cui mi aspetto di leggere
fd_set fd_temp; // temporaneo, utilizzato per salvare il contenuto di fd_lettura prima dell'uso di select
int max_fd = 0;

const char *testi_iniziali[MAX_INIT_CARDS] = {
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

void handle_card();

void init_utenti(){
    
    for(int i = 0; i < MAX_UTENTI; i++){
        utenti[i].attivo = 0;
        utenti[i].socket = -1;
        utenti[i].porta = 0;
    }

    return;
}

void init_cards(){

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

// dato un socket di un utente trova l'indice corrispondente nel vettore utenti ritorna -1 se non esiste
int trova_indice_da_socket(int socket){
    
    int ret = -1;
    int i = 0;
    
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

//data la porta di un utente rimuove tutte le card assegnate e le rimette nella colonna TO_DO
void rimuovi_card_utente(int porta){

    for (int i = 0; i < numero_card; i++){
        if(cards[i].porta_utente == porta && cards[i].stato == DOING){
            cards[i].stato = TO_DO;
            cards[i].porta_utente = -1;
            time(&cards[i].timestamp);
            assigned_card --;

            // la card viene riassegnata se ci sono utenti liberi
            if(assigned_card < utenti_registrati){
                handle_card();
            }
        }
    }

    return;
}

void sort_utenti(){
    
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



/* =========================================== Funzionalità del progetto =========================================== */



void hello_handler(int socket_utente,char porta[MAX_MSG]){
    
    int i = 0;
    while( i < MAX_UTENTI && utenti[i].socket != socket_utente ){
        i++;
    }

    if(i == MAX_UTENTI){
        perror("ERRORE LOGICO, HO ACCETTATO UN MESSAGGIO DA UN UTENTE NON CONNESSO");
        exit(1);
    }

    if(utenti[i].porta != 0){
        printf("un utente ha provato a registrarsi due volte, il comando non ha effettuato modifiche \n");
        return;
    }

    int intporta = atoi(porta);
    utenti[i].porta = intporta;
    utenti_registrati++;
    printf("Registrato utente con porta: %s \n", porta);

    handle_card();

    return;
}

void create_card_handler(int ID, int colonna, char* testo, int dim_testo){
    if(numero_card >= MAX_CARDS){
        printf("impossibile creare la card: limite massimo raggiunti\n");
        return;
    }

    if (dim_testo > DIM_TESTO - 1){
        dim_testo = DIM_TESTO- 1;
    }

    int i = numero_card;
    cards[i].id = ID;
    cards[i].stato = colonna;
    cards[i].porta_utente = -1;
    strncpy(cards[i].testo, testo, dim_testo);
    time(&cards[i].timestamp);

    numero_card ++;

    printf("Creata nuova card: ID = %d, testo: %s\n",ID, cards[i].testo);

    return;
}

/* assegna le card in ordine crescente di porta, verifica se ha una card attiva altrimenti gliela assegna e aspetta l'ack*/
void handle_card(){

    sort_utenti();
    
    for(int i = 0; i < MAX_UTENTI; i++){

        // verifico se l'utente è registrato
        if(utenti[i].porta == 0){
            continue;
        }

        // verifico se ha già una card assegnata
        int assegnata = 0;
        for(int j = 0; j < MAX_CARDS; j++){
            if(utenti[i].porta == cards[j].porta_utente){
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
            printf("Le card sono finite, non è possibile assegnarne una nuova all'utente in attesa");
            return;
        }
        cards[k].porta_utente = utenti[i].porta;
        cards[k].stato = HANDLED;
        time(&cards[k].timestamp);

        // invio della card
        // formato invio ID | TESTO | PORTA1, PORTA2, ... | NUMERO UTENTI 

        memset(BUFFER_OUT,0,DIM_BUFFER);

        int offset = 0;

        offset += sprintf(BUFFER_OUT,"HANDLE_CARD|%d|%s|",cards[k].id,cards[k].testo);
        
        for(int j = 0; j < MAX_UTENTI; j++){
            // escludo il richiedente
            if(utenti[j].porta == utenti[i].porta || utenti[j].porta == 0){
                continue;
            }
            offset += sprintf(BUFFER_OUT + offset,"%d,",utenti[j].porta);
        }
        BUFFER_OUT[offset - 1] = "";

        offset += sprintf(BUFFER_OUT + offset,"|%d",utenti_registrati);

        send(utenti[i].socket, BUFFER_OUT,strlen(BUFFER_OUT),0);

    }


}


/* la funzione termina la connnessione con il client, rimuove le card dell'utente 
    e la riassegna ad un utente se è libero
*/
void quit_handler(int socket){

    int k = trova_indice_da_socket(socket);
    if (k<0){
        perror("impossibile trovare l'indice corrispondente al socket per la gestione del comando \n");
        exit(1);
    }
    
    close(socket);
    utenti_attivi --;
    FD_CLR(socket,&fd_lettura);

    utenti[k].attivo = 0;
    utenti[k].socket = 0;

    rimuovi_card_utente(utenti[k].porta);
    if(utenti[k].porta != 0){
        utenti_registrati --;
    }

    utenti[k].porta = 0;
    return;
}


/* ================================================= Main ================================================== */
int main(){

    // azzero i set
    FD_ZERO(&fd_lettura);
    FD_ZERO(&fd_temp);

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

    printf("Lavagna online alla porta %d. Operazioni possibili | HELLO |\n",PORTA_LAVAGNA);


    // ciclo infinito che inizia mettendosi in attesa di una richiesta da un descrittore che ha ricevuto dati
    // DA IMPLEMENTARE: GESTIONE DEI COMANDI DA TASTIERA 
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
                    BUFFER_IN[n] = '\0';
                    if(n == 0){
                        // chiusura della connessione forzata senza quit

                        quit_handler(i);

                    } 

                    else if (n < 0) {
                        printf("errore nella richiesta da parte del socket %d",i);
                        continue;
                    } 

                    else if (n > MAX_MSG){
                        printf("il testo del messaggio è troppo lungo");
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

                        // in base al comando ricevuto chiamo una funzione handler diversa
                        if (n_campi == 0){
                            printf("comando vuoto/non valido \n");
                        }

                        else if(strcmp(campo[0],"HELLO") == 0 && n_campi == 2){
                            hello_handler(i,campo[1]);
                        } 

                        else if (strcmp(campo[0],"CREATE_CARD") == 0 && n_campi == 4){
                            int id = atoi(campo[1]);
                            int colonna = atoi(campo[2]);
                            create_card_handler(id,colonna,campo[3],strlen(campo[3]));
                        }

                        else if (strcmp(campo[0],"QUIT") == 0){
                            quit_handler(i);
                        }

                        // se nessun comando ha rispettato il formato comunico al client l'errore
                        else {
                            printf("ricevuto comando non valido/non esistente: %s",BUFFER_IN);
                        }
                    }

                    
                }
           }
        }
    }
    close(socket_ascolto);

    return 0;
}