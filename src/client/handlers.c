#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "handlers.h"
#include "stato.h"
#include "rete.h"

/* =========================================== Comandi da/verso la lavagna ==================================== */

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

void pong_handler(void){
    // invia il pong alla lavagna
    memset(BUFFER_OUT,0,DIM_BUFFER);
    sprintf(BUFFER_OUT,"PONG_LAVAGNA");
    if(!invia_msg(socket_lavagna,BUFFER_OUT)){
        printf("errore nell'invio del pong");
    }
    return;
}

void quit_handler(void){
    memset(BUFFER_OUT,0,DIM_BUFFER);
    sprintf(BUFFER_OUT,"QUIT");
    invia_msg(socket_lavagna,BUFFER_OUT);

    close(socket_lavagna);
    close(socket_P2P);

    printf("disconnesso dalla lavagna, chiudo il client \n");
    exit(0);
}

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

/* ================================================ Vista board ================================================ */

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

/* ============================================== Flusso di review ============================================= */

void review_card_handler(int socket_utente, int ID, char* testo){
    printf("richiesta di review per la card %d (%s) \n", ID, testo);

    memset(BUFFER_OUT,0,DIM_BUFFER);
    sprintf(BUFFER_OUT,"REVIEW_ACK|%d",ID);
    if(!invia_msg(socket_utente, BUFFER_OUT)){
        printf("errore nell'invio della review ack all'utente sul socket: %d",socket_utente);
    }

    return;
}

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

void ritenta_review(void){
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

/* ================================================== Dispatch ================================================= */

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
