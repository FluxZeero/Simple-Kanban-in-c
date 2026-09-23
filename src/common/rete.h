#ifndef RETE_H
#define RETE_H

// separa il buffer src, di numero massimo di campi dim, in dst, utilizzando come separatore sep
int parse_msg(char *dst[], char *src, int numero, char *sep);

// manda il messaggio sul socket aggiungendo il terminatore di fine messaggio ('\n'),
// che definisce il confine tra un messaggio e il successivo sullo stream TCP
// ritorna 1 se l'invio è completo, 0 in caso di errore o invio parziale
int invia_msg(int socket, char *messaggio);

#endif
