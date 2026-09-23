# Simple-Kanban-in-c
The project features a simple Kanban written in c as a university project for the course Reti Informatiche, a network foundation exam at Università di Pisa.

## Struttura del progetto

- `src/common/` — codice condiviso tra server e client (framing/parsing dei messaggi sul socket)
- `src/server/` — sorgenti della lavagna (server)
- `src/client/` — sorgenti dell'utente (client/peer)
- `include/` — header condivisi (costanti del protocollo)
- `docs/` — documentazione e note di progetto
- `tests/` — checklist di test manuali

## Compilazione

```
make
```

Genera i due eseguibili `lavagna` e `utente` nella root del progetto. `make clean` rimuove i binari e gli object file.

## Esecuzione

```
./lavagna
./utente <porta>
```
