#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include "costanti.h"
#include "rete.h"

int parse_msg(char *dst[], char *src, int numero, char *sep){

    int n_campi = 0;
    char *token = strtok(src, sep);
    while(token != NULL && n_campi < numero){
        dst[n_campi] = token;
        n_campi++;
        token = strtok(NULL, sep);
    }

    return n_campi;
}

int invia_msg(int socket, char *messaggio){
    char buffer[DIM_BUFFER];
    int len = snprintf(buffer, DIM_BUFFER, "%s\n", messaggio);
    int n = send(socket, buffer, len, 0);
    return (n == len);
}
