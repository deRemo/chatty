/*
 * @file server.h
 * @brief header delle strutture/funzioni eseguite dal server, implementate su piu' file 
 * @author Remo Andreoli 535485
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */

#include<sys/un.h>
#include<pthread.h>
#include<sys/select.h>
#include"config.h"
#include"stats.h"
#include"icl_hash.h"
#include"message.h"
#include"ops.h"
#include"queue.h"

/**
 * @struct listener_args;
 * @brief  struttura dati per memorizzare i parametri del thread listener
 *
 * @var fds		pointer alla coda concorrente degli fd
 * @var sa		indirizzo su cui bindare il socket del listener
 * @var rPipe		descrittore di lettura della pipe di comunicazione
 * @var mtx		mutex della pipe
 * @var workers		quanti workers ha il threadpool
 * @var maxConnections  quante connessioni puo' sostenere al massimo il server
 */
typedef struct listener_args_struct{
        Queue_t* fds; 
	struct sockaddr_un* sa;
        int rPipe;
	pthread_mutex_t* mtx; 
	int workers; 
	int maxConnections;
} listener_args;


/**
 * @struct worker_args;
 * @brief  struttura dati per memorizzare i parametri dei thread worker
 *
 * @var fds           pointer alla coda concorrente degli fd
 * @var Pipe          descrittore di scrittura della pipe di comunicazione
 * @var mtx           mutex della pipe
 */
typedef struct worker_args_struct{
	Queue_t* fds; //coda fd(condivisa con il thread listener)
	int wPipe;
	pthread_mutex_t* mtx; //pipe mutex
} worker_args;


/**
 * @struct tPool;
 * @brief  struttura dati per memorizzare i parametri del threadpool 
 *
 * @var threadsInPool 	quanti thread contiene il threadpool
 * @var threads		pointer ai threads
 */
typedef struct threadpool_struct{
	int threadsInPool;
	pthread_t* threads;	
} tPool;

/**
 * @struct  user 
 * @brief  struttura dati per memorizzare i parametri degli user connessi
 *
 * @var nick		nome dell'user 
 * @var fd		descrittore di connessione dell'user
 * @var idx		indice del mutex di connessione che gli e' stato associato 
 */
typedef struct user_struct{
	char nick[MAX_NAME_LENGTH+1];
	long fd;
	int idx;
} user;


//ops functions(chatty.c)
/**
 * @function registerUser
 * @brief  registra l'user
 *
 * @param nick	   nickname da registrare     
 * @param fd       descrittore della connessione
 *
 */
void registerUser(char* nick, long fd, int maxHistMsgs);

/**
 * @function unregisterUser 
 * @brief  deregistra l'user
 *
 * @param nick     nickname da deregistrare
 * @param fd       descrittore della connessione
 *
 */
void unregisterUser(char* nick, long fd);

/**
 * @function sendText 
 * @brief  invia un messaggio a un utente diverso dal mittente
 *
 * @param msg      il messaggio da inviare
 * @param fd       descrittore della connessione del mittente
 *
 */
void sendText(message_t msg, long fd);

/**
 * @function sendTextToAll 
 * @brief  invia un messaggio a tutti gli user diversi dal mittente 
 *
 * @param msg      il messaggio da inviare a tutti
 * @param fd       descrittore della connessione del mittente
 *
 */
void sendTextToAll(message_t msg, long fd);

/**
 * @function sendFile 
 * @brief invia il file ad un utente diverso dal mittente
 *
 * @param msg      messaggio con le informazioni relative alla richiesta
 * @param fd       descrittore della connessione del mittente
 *
 */
void sendFile(message_t msg, long fd);

/**
 * @function getFile 
 * @brief richiede un file al server 
 *
 * @param msg      messaggio con le informazioni relative alla richiesta
 * @param fd       descrittore della connessione del mittente
 *
 */
void getFile(message_t msg, long fd);

/**
 * @function listHistory 
 * @brief  richiede la lista degli ultimi messaggi inviati ad un user
 *
 * @param nick     nickname dell'user che richiede la lista
 * @param fd       descrittore della connessione
 *
 */
void listHistory(char* nick, long fd);

/**
 * @function listOnlineUsers 
 * @brief richiede la lista degli utenti connessi 
 *
 * @param nick     nickname dell'user che richiede la lista
 * @param fd       descrittore della connessione
 *
 */
void listOnlineUsers(char* nick, long fd);

/**
 * @function connectUser 
 * @brief  connette l'user
 *
 * @param nick	   nickname dell'utente da connettere 
 * @param fd       descrittore della connessione
 *
 */
void connectUser(char* nick, long fd); 

/**
 * @function disconnectUser 
 * @brief  disconnette l'user
 *
 * @param fd       descrittore della connessione
 *
 */
void disconnectUser(long fd);

//listener functions(listener.c)
/**
 * @function initListener
 * @brief  funzione che eseguita dal thread listener
 *
 * @param args       parametri di inizializzazione
 *
 */
void* initListener(void* args);

//threadpool & worker functions(threadPool.c & worker.c)
/**
 * @function initWorker 
 * @brief  funzione eseguita dai thread worker
 *
 * @param args	     parametri di inizializzazione
 *
 */
void* initWorker(void* args);

/**
 * @function initThreadPool 
 * @brief  inizializza il threadpool
 *
 * @param n       quanti thread worker allocare
 *
 */
tPool* initThreadPool(int n);

/**
 * @function startThreads 
 * @brief  inizializza i thread worker
 *
 * @param threadPool 	pointer al threadPool
 * @param initWorker	la funzione da far eseguire ai worker
 * @param args		i parametri da passare ai worker
 */
void startThreads(tPool* threadPool, void* (*initWorker)(void *), void* args);

/**
 * @function joinThreads 
 * @brief attende e raccoglie i valori di terminazione dei thread worker
 *
 * @param threadPool	pointer al threadPool
 */
void joinThreads(tPool* threadPool);

/**
 * @function destroyThreadPool
 * @brief  distrugge il threadPool e libera la memoria
 *
 * @param threadPool 	pointer al threadPool
 */
void destroyThreadPool(tPool* threadPool);


//funzioni ausiliarie
/**
 * @function freeUser
 * @brief  libera la memoria della struct User
 *
 * @param data 		memoria da liberare
 */
void freeUser(void* data); //per la free della strutture user nella coda degli utenti online

/**
 * @function updateStats
 * @brief  aggiorna le statistiche del server
 *
 * @param x*		di quanto aumentare/diminuire la relativa statistica
 */
void updateStats(unsigned long x0, unsigned long x1, unsigned long x2, unsigned long x3, unsigned long x4, unsigned long x5, unsigned long x6); //update chattyStats

/**
 * @function whichOperation
 * @brief  sceglie che funzione eseguire per soddisfare la richiesta del client
 *
 * @param op		operazione da eseguire
 * @param msg		messaggio inviato
 * @param fd		descrittore di connessione
 */
void whichOperation(op_t op, message_t msg, long fd); //invoca una delle ops function
