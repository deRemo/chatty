/**
 * @file historyQueue.h
 * @brief File header della coda dei messaggi ricevuti da un client
 * @author Remo Andreoli 535485
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore 
 */

#include"message.h"
#include<pthread.h>

/**
 * @struct history_node;
 * @brief  struttura dati per memorizzare i nodi della history
 *
 * @var info		messaggio da memorizzare
 * @var sent		messaggio recapitato al user(0 no, 1 si)
 * @var next		puntatore al prossimo nodo
 */
typedef struct history_node_struct{
        message_t info;
        int sent; //0 not sent, 1 sent
	struct history_node_struct* next;
}history_node;

/**
 * @struct qHistory;
 * @brief  coda dei messaggi inviati ad un user
 *
 * @var head            primo messaggio
 * @var tail		ultimo messaggio
 * @var qlock		lock della coda
 * @var length		lunghezza massima della coda
 * @var freeSpace	celle libere nella coda
 */
typedef struct qHistory_struct{
        history_node* head;
        history_node* tail;
	pthread_mutex_t* qlock;	

	int length;
        int freeSpace;
} qHistory;

/**
 * @function initHistory
 * @brief Inizializza la history queue
 *
 * @param maxLength quanti messaggi memorizzare al massimo
 *
 * @return NULL se c'e' stato un errore
 */
qHistory* 	initHistory(int maxLength);

/**
 * @function addMsg
 * @brief Aggiunge un messaggio alla history
 *
 * @param q      coda in cui aggiungere il messaggio
 * @param msg    messaggio da aggiungere
 * @param isSent flag per indicare se msg e' gia' stato recapitato
 *
 */
void		addMsg(qHistory* q, message_t msg, int isSent);

/**
 * @function popMsg
 * @brief  Estrae il primo messaggio (aka il piu' vecchio) dalla history
 *
 * @param q	 coda da cui estrarre il messaggio
 *
 * @return NULL se c'e' stato un errore
 */
message_t 	popMsg(qHistory* q);

/**
 * @function getArrayHistory
 * @brief Crea un'array contenente i nodi della history 
 *
 * @param q	 coda da cui creare l'array
 *
 * @return NULL se c'e' stato un errore
 */
history_node** 	getArrayHistory(qHistory* q);

/**
 * @function setSent
 * @brief Setta come recapitato il messaggio in posizione n 
 *
 * @param q	 coda su cui operare
 * @param n    	 index del nodo su cui operare
 *
 */
void 		setSent(qHistory* q, int n);

/**
 * @function destroyHistory
 * @brief Chiusura della history
 *
 * @param q	 coda da chiudere
 *
 * @return <=0 se c'e' stato un errore
 */
int 		destroyHistory(qHistory* q);
