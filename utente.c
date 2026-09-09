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
#include <pthread.h>
#include <time.h>

/* ============================================ Strutture Dati ============================================ */

typedef struct {
    int socket;
    int review_ack; // 1 se l'utente ha già mandato ack per la review
    int attivo; // 0 se non sono connesso con questo utente 1 se sono connesso 
    int porta;
} struct_utenti;

typedef struct {
    int ID;
    char testo[DIM_TESTO];
} struct_curr_card;

/* =========================================== Variabili Globali =========================================== */

fd_set fd_lettura;
fd_set fd_temp;

int socket_P2P;
int max_fd;
int my_port;
struct timeval tv;
char BUFFER_IN[DIM_BUFFER];
char BUFFER_OUT[DIM_BUFFER];
struct_utenti utenti[MAX_UTENTI];
struct_curr_card curr_card;

// variabili per il flusso di lavoro della card

int card_done = 0; // il thread la imposta ad 1 quando ha finito con la card
int reviewed = 0; // viene impostata ad 1 quando ho ricevuto REVIEW_ACK da tutti gli utenti
int in_review = 0; // 1 mentre aspetto le REVIEW_ACK per la card corrente
time_t review_iniziata = 0; // quando è partita l'attesa della review corrente



int socket_lavagna;

/* ========================================== Funzioni di supporto ========================================== */

void close_handler();

// separa il messaggio con i separatori sep e restituisce il numero di elementi
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

// manda il messaggio sul socket aggiungendo il terminatore di fine messaggio ('\n'),
// che definisce il confine tra un messaggio e il successivo sullo stream TCP
// ritorna 1 se l'invio è completo, 0 in caso di errore o invio parziale
int invia_msg(int socket, char* messaggio){
    char buffer[DIM_BUFFER];
    int len = snprintf(buffer, DIM_BUFFER, "%s\n", messaggio);
    int n = send(socket, buffer, len, 0);
    return (n == len);
}

// inizializza la struttura degli utenti
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

    if (n < 0) {
        printf("errore nella richiesta da parte del socket %d",socket);
        return 0;
    }

    BUFFER_IN[n] = '\0';

    if (n == 0){
        printf("il socket %d si è disconnesso \n",socket);
        if(socket == socket_lavagna){
            printf("connessione con la lavagna persa, chiudo il client \n");
            close(socket_lavagna);
            close(socket_P2P);
            exit(0);
        }
        close_handler(socket);
        return 0;
    }

    else if (n > MAX_MSG){
        printf("il testo del messaggio è troppo lungo");
        return 0;
    }

    // tolgo il terminatore di fine messaggio prima del parsing
    char *fine = strchr(BUFFER_IN, '\n');
    if(fine != NULL){
        *fine = '\0';
    }

    return 1;
}

void close_handler(int socket){
    for (int i = 0; i < MAX_UTENTI; i++){
        if(utenti[i].socket == socket){
            close(utenti[i].socket);
            utenti[i].socket = 0;
            utenti[i].porta = 0;
            utenti[i].review_ack = 0;
            utenti[i].attivo = 0;
        }
    }

    if(in_review){
        // il peer perso poteva essere uno di quelli da cui aspettavo la review:
        // richiedo di nuovo la lista aggiornata e rimando la richiesta a chi
        // è rimasto attivo (o a chi si è collegato nel frattempo)
        memset(BUFFER_OUT,0,DIM_BUFFER);
        sprintf(BUFFER_OUT,"REQUEST_USER_LIST");
        invia_msg(socket_lavagna,BUFFER_OUT);
    }

    return;
}

void* process_card(void* arg){
    int secondi = *(int*)arg;
    free(arg);

    // simulazione del tempo di lavoro sulla card
    sleep(secondi);

    printf("ho finito la card con id: %d, chiedo una review\n",curr_card.ID);
    // segnalo al thread principale che il lavoro è finito: si occuperà lui di
    // chiedere la lista utenti aggiornata e far partire la review
    card_done = 1;

    return NULL;
}

void avvia_processo_card(int secondi) {
    pthread_t thread_id;

    // Alloca dinamicamente il parametro per evitare race condition sulle variabili locali
    int *arg = malloc(sizeof(int));
    *arg = secondi;

    if (pthread_create(&thread_id,NULL,process_card,arg) != 0) {
        perror("Errore creazione thread");
        free(arg);
        return;
    }
    pthread_detach(thread_id);
    // Il main prosegue immediatamente senza bloccarsi
}

// si connette all'utente con la porta specificata
void connect_to_user(int porta){
    
    // controlla se si è già connesso utilizzando il campo attivo della struct 
    int i = 0;
    while(i < MAX_UTENTI){
        if(utenti[i].porta == porta){
            return;
        }
        i++;
    }

    struct sockaddr_in ind_utente;
    memset(&ind_utente,0,sizeof(ind_utente));

    ind_utente.sin_family = AF_INET;
    inet_pton(AF_INET,"127.0.0.1", & ind_utente.sin_addr);
    ind_utente.sin_port = htons(porta);

    int socket_utente = socket(AF_INET,SOCK_STREAM, 0);

    socklen_t client_len = sizeof(ind_utente);
    
    if (connect(socket_utente,(struct sockaddr*)&ind_utente,client_len) < 0){
        printf("errore nella connessione con l'utente di porta: %d",porta);
        close(socket_utente);
        return;
    }

    // mi presento all'utente con la mia porta reale di ascolto
    memset(BUFFER_OUT,0,DIM_BUFFER);
    sprintf(BUFFER_OUT,"HELLO_PEER|%d",my_port);
    invia_msg(socket_utente,BUFFER_OUT);

    // salvo i suoi valori nella struct
    for(int i = 0; i < MAX_UTENTI; i++){
        if(utenti[i].attivo == 0){
            utenti[i].porta = porta;
            utenti[i].attivo = 1;
            utenti[i].review_ack = 0;
            utenti[i].socket = socket_utente;
            break;
        } else if( (utenti[i].attivo == 1) && (i == MAX_UTENTI - 1) ){
            printf("massimo di utenti raggiunti impossibile collegarsi al client con porta %d",porta);
            close(socket_utente);
            return;
        }
    }
    return;
}

/* =========================================== Funzionalità del progetto =========================================== */


// riceve la porta reale di ascolto di un peer che si è appena connesso a noi
void hello_peer_handler(int socket_utente, char* porta_str){
    int i = 0;
    while(i < MAX_UTENTI && utenti[i].socket != socket_utente){
        i++;
    }
    if(i == MAX_UTENTI){
        printf("errore logico: HELLO_PEER da un socket non registrato: %d \n", socket_utente);
        return;
    }
    utenti[i].porta = atoi(porta_str);
    printf("peer sulla porta %d registrato correttamente \n", utenti[i].porta);
    return;
}

// formatta una singola cella "ID testo" a partire dal token "id:testo"; NULL = cella vuota
void formatta_cella(char* dest, int size, char* voce){
    if(voce == NULL){
        dest[0] = '\0';
        return;
    }
    char *due_punti = strchr(voce, ':');
    if(due_punti == NULL){
        snprintf(dest, size, "%s", voce);
        return;
    }
    *due_punti = '\0';
    snprintf(dest, size, "%s %s", voce, due_punti + 1);
}

// riceve i dati della board (id:testo per colonna, separati da ';') e li stampa in tabella
void show_lavagna_data_handler(char* lista_todo, char* lista_doing, char* lista_done){
    char *todo[MAX_CAMPI], *doing[MAX_CAMPI], *done[MAX_CAMPI];
    int n_todo  = (strcmp(lista_todo,"-")  == 0) ? 0 : parse_msg(todo,  lista_todo,  MAX_CAMPI, ";");
    int n_doing = (strcmp(lista_doing,"-") == 0) ? 0 : parse_msg(doing, lista_doing, MAX_CAMPI, ";");
    int n_done  = (strcmp(lista_done,"-")  == 0) ? 0 : parse_msg(done,  lista_done,  MAX_CAMPI, ";");

    int righe = n_todo;
    if(n_doing > righe) righe = n_doing;
    if(n_done  > righe) righe = n_done;

    // intestazione delle tre colonne, allineata a sinistra su COL_WIDTH caratteri
    printf("\n%-*s | %-*s | %-*s\n", COL_WIDTH, "TO_DO", COL_WIDTH, "DOING", COL_WIDTH, "DONE");
    // riga di trattini sotto l'intestazione, tagliata a COL_WIDTH caratteri per lato
    printf("%.*s-+-%.*s-+-%.*s\n",
        COL_WIDTH, "------------------------------------------------------------",
        COL_WIDTH, "------------------------------------------------------------",
        COL_WIDTH, "------------------------------------------------------------");

    for(int r = 0; r < righe; r++){
        char cella_todo[COL_WIDTH + 5], cella_doing[COL_WIDTH + 5], cella_done[COL_WIDTH + 5];
        formatta_cella(cella_todo,  sizeof(cella_todo),  r < n_todo  ? todo[r]  : NULL);
        formatta_cella(cella_doing, sizeof(cella_doing), r < n_doing ? doing[r] : NULL);
        formatta_cella(cella_done,  sizeof(cella_done),  r < n_done  ? done[r]  : NULL);

        // "%-*.*s" = allinea a sinistra e taglia il testo a COL_WIDTH caratteri,
        // così le colonne restano allineate anche se il testo di una card è più lungo delle altre
        printf("%-*.*s | %-*.*s | %-*.*s\n",
            COL_WIDTH, COL_WIDTH, cella_todo,
            COL_WIDTH, COL_WIDTH, cella_doing,
            COL_WIDTH, COL_WIDTH, cella_done);
    }
    printf("\n");

    return;
}

// ricevuta una richiesta di review manda all'utente che l'ha richiesto un ack
void review_card_handler(int socket_utente, int ID, char* testo){
    printf("richiesta di review per la card %d (%s) \n", ID, testo);

    memset(BUFFER_OUT,0,DIM_BUFFER);
    sprintf(BUFFER_OUT,"REVIEW_ACK|%d",ID);
    if(!invia_msg(socket_utente, BUFFER_OUT)){
        printf("errore nell'invio della review ack all'utente sul socket: %d",socket_utente);
    }

    return;
}

// riceve la lista utenti aggiornata: si connette a chi non conosce ancora e
// manda a tutti i connessi la richiesta di review per la card corrente
void send_user_list_handler(char* lista){

    // "-" e' il placeholder per "nessun altro utente", altrimenti il campo
    // vuoto nel messaggio verrebbe collassato dal parsing lato lavagna
    if(strcmp(lista, "-") != 0){
        char *porte[MAX_UTENTI];
        int n_porte = parse_msg(porte, lista, MAX_UTENTI, ",");
        for(int i = 0; i < n_porte; i++){
            connect_to_user(atoi(porte[i]));
        }
    }

    // se non ho nessun peer attivo (nessuno con cui fare la review, o tutti
    // disconnessi) non posso aspettare all'infinito: completo comunque la card
    int peer_attivi = 0;
    for(int i = 0; i < MAX_UTENTI; i++){
        if(utenti[i].attivo){
            peer_attivi++;
        }
    }

    if(peer_attivi == 0){
        printf("nessun altro utente disponibile per la review: completo comunque la card %d \n", curr_card.ID);
        reviewed = 1;
        return;
    }

    memset(BUFFER_OUT,0,DIM_BUFFER);
    sprintf(BUFFER_OUT,"REVIEW_CARD|%d|%s",curr_card.ID,curr_card.testo);

    for(int i = 0; i < MAX_UTENTI; i++){
        if(utenti[i].attivo == 0){
            continue;
        }
        invia_msg(utenti[i].socket, BUFFER_OUT);
    }

    in_review = 1;
    time(&review_iniziata);

    return;
}

// rimanda REVIEW_CARD a chi non ha ancora risposto, nel caso l'ack si sia perso
void ritenta_review(){
    memset(BUFFER_OUT,0,DIM_BUFFER);
    sprintf(BUFFER_OUT,"REVIEW_CARD|%d|%s",curr_card.ID,curr_card.testo);

    for(int i = 0; i < MAX_UTENTI; i++){
        if(utenti[i].attivo == 0 || utenti[i].review_ack == 1){
            continue;
        }
        invia_msg(utenti[i].socket, BUFFER_OUT);
    }

    time(&review_iniziata);

    return;
}

// riceve la conferma di review da un peer; quando tutti gli utenti attivi hanno
// confermato, segnala che la card può essere chiusa
void review_ack_handler(int socket_utente, int ID){
    int i = 0;
    while(i < MAX_UTENTI && utenti[i].socket != socket_utente){
        i++;
    }
    if(i == MAX_UTENTI){
        printf("errore logico: REVIEW_ACK da un socket non registrato: %d \n", socket_utente);
        return;
    }

    utenti[i].review_ack = 1;

    int tutti_pronti = 1;
    for(int j = 0; j < MAX_UTENTI; j++){
        if(utenti[j].attivo && utenti[j].review_ack == 0){
            tutti_pronti = 0;
            break;
        }
    }

    if(tutti_pronti){
        printf("ho ricevuto tutte le review positive mando CARD_DONE alla lavagna\n");
        reviewed = 1;
    }

    return;
}

void pong_handler(){
    // invia il pong alla lavagna
    memset(BUFFER_OUT,0,DIM_BUFFER);
    sprintf(BUFFER_OUT,"PONG_LAVAGNA");
    if(!invia_msg(socket_lavagna,BUFFER_OUT)){
        printf("errore nell'invio del pong");
    }
    return;
}

// disconnessione volontaria, richiamata da tastiera: avviso la lavagna e chiudo
void quit_handler(){
    memset(BUFFER_OUT,0,DIM_BUFFER);
    sprintf(BUFFER_OUT,"QUIT");
    invia_msg(socket_lavagna,BUFFER_OUT);

    close(socket_lavagna);
    close(socket_P2P);

    printf("disconnesso dalla lavagna, chiudo il client \n");
    exit(0);
}

// inizializza la card se ID = -1 altrimenti aggiorna la struttura current card
void card_handler(int ID, char* testo, char* porte_utenti, int utenti_lav){
    if (ID == -1){
        curr_card.ID = 0;
        curr_card.testo[0] = '\0';
    } else {
        curr_card.ID = ID;
        strncpy(curr_card.testo, testo, DIM_TESTO - 1);
        curr_card.testo[DIM_TESTO - 1] = '\0';

        // ottengo le porte degli utenti passati nel formato del messaggio
        // "-" e' il placeholder per "nessun altro utente"
        if(strcmp(porte_utenti, "-") != 0){
            char *porte[MAX_UTENTI];
            int n_porte = parse_msg(porte, porte_utenti, MAX_UTENTI, ",");

            for(int i = 0; i < n_porte; i++){
                int porta = atoi(porte[i]);
                connect_to_user(porta);
            }
        }
        // mando l'ack alla lavagna
        memset(BUFFER_OUT,0,DIM_BUFFER);
        sprintf(BUFFER_OUT,"ACK_CARD|%d",ID);
        if(!invia_msg(socket_lavagna,BUFFER_OUT)){
            printf("errore nell'invio dell'ack");
            return;
        }
        
        avvia_processo_card(10);
    }



    return;
}


void call_handler(int socket_utente, char *campo[MAX_CAMPI], int n_campi){
    
    if(n_campi == 0){
        printf("ricevuto comando non valido/non esistente: %s , n_campi: %d, socket chiamante %d \n",BUFFER_IN,n_campi, socket_utente);
        return;
    }
    if (socket_utente == socket_lavagna && strcmp(campo[0],"PING_USER") == 0 && n_campi == 1){
        pong_handler();
    }

    else if (socket_utente == socket_lavagna && strcmp(campo[0],"HANDLE_CARD") == 0 && n_campi >= 5){
        card_handler(atoi(campo[1]),campo[2],campo[3],atoi(campo[4]));
    }

    else if (strcmp(campo[0],"HELLO_PEER") == 0 && n_campi == 2){
        hello_peer_handler(socket_utente,campo[1]);
    }
    
    else if (strcmp(campo[0],"REVIEW_CARD") == 0 && n_campi >= 3){
        review_card_handler(socket_utente, atoi(campo[1]), campo[2]);
    }

    else if (socket_utente == socket_lavagna && strcmp(campo[0],"SEND_USER_LIST") == 0 && n_campi >= 2){
        send_user_list_handler(campo[1]);
    }

    else if (socket_utente == socket_lavagna && strcmp(campo[0],"SHOW_LAVAGNA_DATA") == 0 && n_campi == 4){
        show_lavagna_data_handler(campo[1], campo[2], campo[3]);
    }

    else if (strcmp(campo[0],"REVIEW_ACK") == 0 && n_campi == 2){
        review_ack_handler(socket_utente, atoi(campo[1]));
    }

    else if (socket_utente == STDIN_FILENO && strcmp(campo[0],"QUIT") == 0 && n_campi == 1){
        quit_handler();
    }

    else if (socket_utente == STDIN_FILENO && strcmp(campo[0],"SHOW_LAVAGNA") == 0 && n_campi == 1){
        // inoltro il comando alla lavagna, che stampa la board sul suo terminale
        memset(BUFFER_OUT,0,DIM_BUFFER);
        sprintf(BUFFER_OUT,"SHOW_LAVAGNA");
        invia_msg(socket_lavagna,BUFFER_OUT);
    }

    else if (socket_utente == STDIN_FILENO && strcmp(campo[0],"CREATE_CARD") == 0 && n_campi == 4){
        // inoltro il comando alla lavagna così com'è, la creazione la fa lei
        memset(BUFFER_OUT,0,DIM_BUFFER);
        snprintf(BUFFER_OUT,DIM_BUFFER,"CREATE_CARD|%s|%s|%s",campo[1],campo[2],campo[3]);
        invia_msg(socket_lavagna,BUFFER_OUT);
    }

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