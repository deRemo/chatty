#include<pthread.h>

#ifndef QUEUE_H_
#define QUEUE_H_
/** Elemento della coda.
 *
 */
typedef struct Node {
    void        * data;
    struct Node * next;
} Node_t;

/** Struttura dati coda.
 *
 */
typedef struct Queue {
    Node_t        *head;
    Node_t        *tail;

    pthread_mutex_t* qlock;
    pthread_cond_t* qcond;    
    unsigned long  qlen;
} Queue_t;


/** Alloca ed inizializza una coda. Deve essere chiamata da un solo 
 *  thread (tipicamente il thread main).
 *
 *   \retval NULL se si sono verificati problemi nell'allocazione (errno settato)
 *   \retval q puntatore alla coda allocata
 */
Queue_t *initQueue();

/** Cancella una coda allocata con initQueue. Deve essere chiamata da
 *  da un solo thread (tipicamente il thread main).
 *  
 *   \param q puntatore alla coda da cancellare
 */
void deleteQueue(Queue_t *q, void (*free_data)(void*)); //lasciare free_data NULL se q non contiene user online

/** Inserisce un dato nella coda.
 *   \param data puntatore al dato da inserire
 *  
 *   \retval 0 se successo
 *   \retval -1 se errore (errno settato opportunamente)
 */
int    push(Queue_t *q, void *data);

/** Estrae un dato dalla coda.
 *
 *  \retval data puntatore al dato estratto.
 */
void  *pop(Queue_t *q);


unsigned long length(Queue_t *q);

/* HEADERS AGGIUNTI DA ME */
/**
 * @function queue2String
 * @brief trasforma la coda degli utenti online in array di stringhe 
 *
 * @param q		pointer alla coda degli utenti online
 * @param n		pointer ad una variabile per memorizzare il numero di utenti online
 * 
 * @return		un'array di stringhe contenente i nomi degli utenti online
 */
char* queue2String(Queue_t* q, long* n);

/**
 * @function findNick
 * @brief ricerca un utente online per nome
 *
 * @param q		pointer alla coda degli utenti online
 * @param nick		nome dell'utente da ricercare
 *
 * @return		la struct User corrispondente se l'utente e' online, NULL altrimenti
 */
void* findNick(Queue_t* q, char* nick); //ritorna la struct corrispondente al nick(se l'user e' online)

/**
 * @function findNick
 * @brief ricerca un utente online per descrittore
 *
 * @param q             pointer alla coda degli utenti online
 * @param fd		descrittore dell'utente
 *
 * @return              la struct User corrispondente se l'utente e' online, NULL altrimenti
 */
void* findFd(Queue_t* q, long fd); //ritorna la struct user corrispondente al fd(se l'user e' online)

/**
 * @function removeAtFd
 * @brief rimuove utente per descrittore 
 *
 * @param q             pointer alla coda degli utenti online
 * @param fd		descrittore dell'utente
 *
 * @return              SUCCESS(1) se rimuove l'utente dalla cosa, FAILURE(0) altrimenti
 */
int removeAtFd(Queue_t* q, long fd);

/**
 * @function removeAtFd
 * @brief rimuove utente per nickname
 *
 * @param q             pointer alla coda degli utenti online
 * @param fd            nome dell'utente
 *
 * @return              SUCCESS(1) se rimuove l'utente dalla cosa, FAILURE(0) altrimenti
 */
int removeAtNick(Queue_t* q, char* nick);
#endif /* QUEUE_H_ */
