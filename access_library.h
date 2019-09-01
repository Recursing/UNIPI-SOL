#ifndef ACCESS_LIB_H
#define ACCESS_LIB_H

#include <stdlib.h>

// Inizia la connessione all'object store, registrando il cliente con il
// name dato. Restituisce true se la connessione ha avuto successo, false altrimenti.
// Notate che la connessione all'object store è globale per il client.
int os_connect(char *name);

// richiede all'object store la memorizzazione dell'oggetto puntato da block,
// per una lunghezza len, con il nome name.
// Restituisce true se la memorizzazione ha avuto successo, false altrimenti.
int os_store(char *name, void *block, size_t len);

// Recupera dall'object store l'oggetto precedentemente memorizzatato sotto il nome `name`.
// Se il recupero ha avuto successo, restituisce un puntatore a un blocco di memoria,
// allocato dalla funzione, contenente i dati precedentemente memorizzati.
// In caso di errore, restituisce NULL.
void *os_retrieve(char *name);

// Cancella l'oggetto di nome `name` precedentemente memorizzato.
// Restituisce true se la cancellazione ha avuto successo, false altrimenti.
int os_delete(char *name);

// chiude la connessione all'object store.
// Restituisce true se la disconnessione ha avuto successo, false in caso contrario.
int os_disconnect();

// Restituisce il numero di bytes ricevuti dall'ultima retrieve
// Necessaria per distinguere oggetti vuoti da errori
// e per gestire oggetti di dimensione non salvata
int get_retrieve_len();

// Se vale true gli errori di overwrite vengono ignorati
// in modo da poter salvare oggetti solo se assenti senza ricevere errori
extern int ignore_overwrite_errors;
#endif