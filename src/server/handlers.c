#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "handlers.h"
#include "stato.h"
#include "rete.h"

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

    //  tronco il messaggio se troppo lungo
    if (dim_testo > DIM_TESTO - 1){
        dim_testo = DIM_TESTO - 1;
    }

    if(numero_card - 1 >= ID){
        printf("Impossibile creare una card con ID <= di una già esistente, ultimo id: %d \n",numero_card - 1);
        return;
    }

    if(colonna < TO_DO || colonna > DONE){
        printf("Impossibile creare la card: colonna %d non valida (usa %d=TO_DO, %d=DOING, %d=DONE)\n",colonna,TO_DO,DOING,DONE);
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
    handle_card();

    return;
}

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

    // salvo la porta prima di azzerarla: mi serve per liberare le sue card,
    int porta_disconnessa = utenti[k].porta;

    utenti[k].attivo = 0;
    utenti[k].socket = 0;
    utenti[k].ping_timeout_counter = 0;
    utenti[k].porta = 0;

    if(porta_disconnessa != 0){
        utenti_registrati --;
    }

    rimuovi_card_utente(porta_disconnessa);

    return;
}

void close_handler(void){

    for(int i = 0; i < MAX_UTENTI; i++){
        if(utenti[i].attivo){
            close(utenti[i].socket);
        }
    }

    close(socket_ascolto);

    printf("lavagna chiusa \n");
    exit(0);
}

void user_list_handler(int socket){

    // il richiedente non deve comparire nella propria lista, altrimenti si
    // connetterebbe a se stesso (non si applica alla richiesta da tastiera)
    int porta_richiedente = -1;
    if(socket != STDIN_FILENO){
        int richiedente = trova_indice_da_socket(socket);
        if(richiedente >= 0){
            porta_richiedente = utenti[richiedente].porta;
        }
    }

    // preparo la lista delle porte (ogni porta al massimo "65535,", 6 caratteri)
    char lista_porte[MAX_UTENTI * 6 + 1];
    int offset = 0;
    lista_porte[0] = '\0';

    for(int i = 0; i < MAX_UTENTI; i++){
        if(utenti[i].porta == 0 || utenti[i].porta == porta_richiedente){
            continue;
        }

        offset += sprintf(lista_porte + offset,"%d,",utenti[i].porta);
    }

    if(offset > 0){
        lista_porte[offset - 1] = '\0';
    } else {
        // nessun altro utente: placeholder, altrimenti il campo vuoto verrebbe
        // collassato dal parsing e sfaserebbe il numero di campi del messaggio
        strcpy(lista_porte, "-");
    }

    if(socket == STDIN_FILENO){
        printf("Lista utenti: %s \n",lista_porte);
        return;
    }

    // invio il messaggio, con nome del comando e numero di utenti registrati
    memset(BUFFER_OUT,0,DIM_BUFFER);
    snprintf(BUFFER_OUT,DIM_BUFFER,"SEND_USER_LIST|%s|%d",lista_porte,utenti_registrati);

    if(invia_msg(socket, BUFFER_OUT)){
        printf("inviate le porte degli utenti al socket: %d",socket);
    } else {
        printf("errore nell'invio del messaggio nella richiesta: user_list_handler \n");
    }

    return;
}

void ping_user(void){
    time_t adesso = time(NULL);

    for (int i = 0; i < MAX_CARDS; i++){
        if (cards[i].stato != DOING || cards[i].porta_utente == -1){
            continue;
        }

        int k = 0;
        while(k < MAX_UTENTI && utenti[k].porta != cards[i].porta_utente){
            k++;
        }
        if(k == MAX_UTENTI){
            continue;
        }

        if(utenti[k].ping_timeout_counter != 0){
            // gia' pingato: e' scaduto il tempo di risposta?
            if(adesso - utenti[k].ping_timeout_counter >= 30){
                printf("utente %d non risponde al ping, libero la card %d \n", utenti[k].porta, cards[i].id);
                move_card(cards[i].id, DOING, TO_DO, -1);
                utenti[k].ping_timeout_counter = 0;
            }
            continue;
        }

        // non ancora pingato: lo faccio solo se e' ferma da abbastanza tempo
        if(adesso - cards[i].timestamp >= 90){
            memset(BUFFER_OUT,0,DIM_BUFFER);
            sprintf(BUFFER_OUT,"PING_USER");
            invia_msg(utenti[k].socket, BUFFER_OUT);
            utenti[k].ping_timeout_counter = adesso;
        }
    }
}

void pong_handler(int socket_utente){
    int k = trova_indice_da_socket(socket_utente);
    if(k < 0){
        printf("errore logico: in pong lavagna è arrivato un socket associato a nessun utente : %d \n",socket_utente);
        return;
    }
    utenti[k].ping_timeout_counter = 0;
    printf("arrivato correttamente il pong dall'utente %d \n", socket_utente);
}

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

    else if (socket_utente == STDIN_FILENO && strcmp(campo[0],"CLOSE") == 0 && n_campi == 1){
        // ho chiamato CLOSE da riga di comando: termino la lavagna
        close_handler();
    }

    else if (socket_utente == STDIN_FILENO && !strcmp(campo[0],"HANDLE_CARD") && n_campi == 1){
        // ho chiamato HANDLE_CARD da riga di comando
        handle_card();
    }

    else if (strcmp(campo[0],"SEND_USER_LIST") == 0 || strcmp(campo[0],"REQUEST_USER_LIST") == 0){
        user_list_handler(socket_utente);
    }

    else if (strcmp(campo[0],"SHOW_LAVAGNA") == 0){
        show_lavagna();
        if(socket_utente != STDIN_FILENO){
            // richiesta arrivata da un utente: gli rimando anche i dati
            // così può stamparsi la board sul proprio terminale
            show_lavagna_data_handler(socket_utente);
        }
    }

    else if (strcmp(campo[0],"ACK_CARD") == 0 && n_campi == 2){
        int id = atoi(campo[1]);
        move_card(id, HANDLED, DOING, -1);
    }

    else if (strcmp(campo[0],"CARD_DONE") == 0 && n_campi == 2){
        int id = atoi(campo[1]);
        move_card(id,DOING,DONE,-2);
    }

    else if (strcmp(campo[0],"PONG_LAVAGNA") == 0){
        pong_handler(socket_utente);
    }

    // se nessun comando ha rispettato il formato comunico al client l'errore
    else {
        printf("ricevuto comando non valido/non esistente: %s , n_campi: %d, socket chiamante %d \n",BUFFER_IN,n_campi, socket_utente);
    }
}
