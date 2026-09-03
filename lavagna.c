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

/* ========================================== Funzioni di supporto ========================================== */

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

    if(socket == STDIN_FILENO){
        return 0;
    }
    
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
            // DA IMPLEMENTARE: invio all'utente che gli ho tolto la card

            // la card viene riassegnata se ci sono utenti liberi
            if(assigned_card < utenti_registrati){
                handle_card();
            }
        }
    }

    return;
}

// ordina gli utenti per numero di porta crescente
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

// separa il buffer src, di numero masssimo di campi dim, in dst, utilizzando come separatore sep
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

/* costruisce in dest la riga r di una colonna larga COL_WIDTH:
   r == 0 e' la riga vuota di apertura, poi ogni card occupa 3 righe:
   ID, testo attivita', riga vuota di separazione */
void riga_colonna(char *dest, int r, int *indici, int n_card){

    char contenuto[COL_WIDTH + 1];
    contenuto[0] = '\0';

    if(r > 0){
        int card = (r - 1) / 3;
        int sotto_riga = (r - 1) % 3;

        if(card < n_card){
            int idx = indici[card];

            if(sotto_riga == 0){
                snprintf(contenuto, sizeof(contenuto), " ID:%d", cards[idx].id);
            } else if(sotto_riga == 1){
                snprintf(contenuto, sizeof(contenuto), " %.*s", COL_WIDTH - 2, cards[idx].testo);
            }
        }
    }

    snprintf(dest, COL_WIDTH + 1, "%-*s", COL_WIDTH, contenuto);
}





/* =========================================== Funzionalità del progetto =========================================== */

void hello_handler(int socket_utente,char porta[MAX_MSG]){
    
    int i = 0;
    while( i < MAX_UTENTI && utenti[i].socket != socket_utente ){
        i++;
    }

    if(i == MAX_UTENTI && socket_utente != 0){
        perror("ERRORE LOGICO, HO ACCETTATO UN MESSAGGIO DA UN UTENTE NON CONNESSO \n");
        exit(1);
    }

    if(socket_utente == 0){
        printf("Non è possiile registrarsi con comandi da tastiera \n");
        return;
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
        dim_testo = DIM_TESTO - 1;
    }

    if(numero_card - 1 > ID){
        printf("Impossibile creare una card con ID < di una già esistente, utlimo id: %d \n",numero_card - 1);
        return;
    }

    int i = numero_card;
    cards[i].id = ID;
    cards[i].stato = colonna;
    cards[i].porta_utente = -1;
    strncpy(cards[i].testo, testo, dim_testo);
    time(&cards[i].timestamp);
    numero_card ++;
    
    printf("Creata nuova card: ID = %d, colonna: %d testo: %s\n",ID, cards[i].stato, cards[i].testo);

    show_lavagna();

    return;
}

/* assegna le card in ordine crescente di porta, verifica se ha una card attiva altrimenti gliela assegna e aspetta l'ack*/
void handle_card(){

    sort_utenti();
    
    int card_assegnate = 0;

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
            printf("Le card sono finite, non è possibile assegnarne una nuova all'utente in attesa \n");
            return;
        }
        cards[k].porta_utente = utenti[i].porta;
        cards[k].stato = HANDLED;
        time(&cards[k].timestamp);

        card_assegnate++;

        printf("Assegnata la card con ID: %d \n",cards[k].id);

        // invio della card
        // formato invio ID | TESTO | PORTA1, PORTA2, ... | NUMERO UTENTI 

        memset(BUFFER_OUT,0,DIM_BUFFER);

        int offset = 0;

        offset += sprintf(BUFFER_OUT,"HANDLE_CARD|%d|%s",cards[k].id,cards[k].testo);
        
        for(int j = 0; j < MAX_UTENTI; j++){
            // escludo il richiedente
            if(utenti[j].porta == utenti[i].porta || utenti[j].porta == 0){
                continue;
            }
            offset += sprintf(BUFFER_OUT + offset,"%d,",utenti[j].porta);
        }

        offset += sprintf(BUFFER_OUT + offset,"|%d",utenti_registrati);

        int n = send(utenti[i].socket, BUFFER_OUT,strlen(BUFFER_OUT),0);

        if (n < offset){
            printf("inviata la card con ID:%d, ma il messaggio finale è stato tagliato \n",cards[k].id);
        } else if (n == -1){
            printf("ERRORE: invio della card non riuscito \n");
        } else {
            printf("invio della card con ID: %d avvenuto con successo \n",cards[k].id);
        }

    }

    if(card_assegnate == 0){
        printf("non ci sono utenti liberi per assegnare card \n");
    }

    return;

}

/* la funzione termina la connnessione con il client, rimuove le card dell'utente 
    e la riassegna ad un utente se è libero
*/
void quit_handler(int socket){

    if(socket == 0){
        printf("Impossibile effettuare una disconnessione dalla riga di comando \n");
        return;
    }
    
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


void show_lavagna(){

    // raccolgo gli indici delle card, divisi per colonna
    int idx_todo[MAX_CARDS], idx_doing[MAX_CARDS], idx_done[MAX_CARDS];
    int n_todo = 0, n_doing = 0, n_done = 0;

    for(int i = 0; i < numero_card; i++){
        switch(cards[i].stato){
            case TO_DO:
                idx_todo[n_todo++] = i;
                break;
            case HANDLED:   // assegnata ma non ancora confermata con ACK: resta in To Do
                idx_todo[n_todo++] = i;
                break;
            case DOING:
                idx_doing[n_doing++] = i;
                break;
            case DONE:
                idx_done[n_done++] = i;
                break;
        }
    }

    // una riga vuota di apertura piu' 3 righe per ogni card, con un'altezza minima fissa
    int righe = RIGHE_LAVAGNA;
    if(1 + n_todo * 3 > righe)  righe = 1 + n_todo * 3;
    if(1 + n_doing * 3 > righe) righe = 1 + n_doing * 3;
    if(1 + n_done * 3 > righe)  righe = 1 + n_done * 3;

    printf("\n    _____________________________________________________________________________________________\n");
    printf("   /                                                                                            /|\n");
    printf("  /                                                                                            / |\n");
    printf(" /                                                                                            /  |\n");
    printf("|============================================================================================|   |\n");
    printf("|                                                                                            |   |\n");
    printf("|                                          LAVAGNA                                           |   |\n");
    printf("|                                                                                            |   |\n");
    printf("|============================================================================================|   |\n");
    printf("|_____________TO_DO_________________________DOING___________________________DONE_____________|   |\n");

    char col_todo[COL_WIDTH + 1], col_doing[COL_WIDTH + 1], col_done[COL_WIDTH + 1];

    for(int r = 0; r < righe; r++){
        riga_colonna(col_todo,  r, idx_todo,  n_todo);
        riga_colonna(col_doing, r, idx_doing, n_doing);
        riga_colonna(col_done,  r, idx_done,  n_done);

        printf("|%s|%s|%s|   |\n", col_todo, col_doing, col_done);
    }

    printf("|______________________________|______________________________|______________________________|   |\n");
    printf("|                                                                                            |  /\n");
    printf("|                                                                                            | / \n");
    printf("|____________________________________________________________________________________________|/  \n");
    return;
}

// manda la lista delle porte all'utente identificato con socket
void user_list_handler(int socket){
    
    // preparo il messaggio
    memset(BUFFER_OUT,0,DIM_BUFFER);
    int offset = 0;

    for(int i = 0; i < MAX_UTENTI; i++){
        if(utenti[i].porta == 0){
            continue;
        }

        offset += sprintf(BUFFER_OUT + offset,"%d,",utenti[i].porta);
    }

    if(offset > 0){
        BUFFER_OUT[offset - 1] = '\0';
    }

    if(socket == STDIN_FILENO){
        printf("Lista utenti: %s \n",BUFFER_OUT);
        return;
    }

    // invio il messaggio
    int size = strlen(BUFFER_OUT);
    int n = send(socket,BUFFER_OUT,size,0);

    if (n < 0){
        printf("errore nell'invio del messaggio nella richiesta: user_list_handler \n");
    } else {
        printf("inviate le porte degli utenti al socket: %d",socket);
    }

    return;
}

void ping_user(){
    return;
}

// in base al comando ricevuto chiamo l'handler corretto per la gestione della richiesta
void call_handler(int socket_utente, char *campo[MAX_CAMPI], int n_campi){
    
    if(n_campi == 0){
        printf("ricevuto comando non valido/non esistente: %s , n_campi: %d, socket chiamante %d \n",BUFFER_IN,n_campi, socket_utente);
        return;
    }
    
    if(strcmp(campo[0],"HELLO") == 0 && n_campi == 2){
        hello_handler(socket_utente,campo[1]);
    } 

    else if (strcmp(campo[0],"CREATE_CARD") == 0 && n_campi == 4){
        int id = atoi(campo[1]);
        int colonna = atoi(campo[2]);
        create_card_handler(id,colonna,campo[3],strlen(campo[3]));
    }

    else if (strcmp(campo[0],"QUIT") == 0){
        quit_handler(socket_utente);
    } 

    else if (socket_utente == STDIN_FILENO && !strcmp(campo[0],"HANDLE_CARD") && n_campi == 1){
        // ho chiamato HANDLE_CARD da riga di comando 
        handle_card();
    }

    else if (strcmp(campo[0],"SEND_USER_LIST") == 0){
        user_list_handler(socket_utente);
    }

    else if (strcmp(campo[0],"SHOW_LAVAGNA") == 0){
        show_lavagna();
    }

    // se nessun comando ha rispettato il formato comunico al client l'errore
    else {
        printf("ricevuto comando non valido/non esistente: %s , n_campi: %d, socket chiamante %d \n",BUFFER_IN,n_campi, socket_utente);
    }
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

    show_lavagna();
    printf("Lavagna online alla porta %d. \n Operazioni possibili: \n| HELLO + numero_porta | \nCREATE_CARD + ID + COLONNA + TESTO_ATTIVITà\n",PORTA_LAVAGNA);

    // ciclo infinito che inizia mettendosi in attesa di una richiesta da un descrittore che ha ricevuto dati
    while(1){
        FD_ZERO(&fd_lettura);
        FD_SET(socket_ascolto,&fd_lettura);
        FD_SET(STDIN_FILENO,&fd_lettura);

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

                else {
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