/*
 * membox Progetto del corso di LSO 2017/2018
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 * 
 */
/**
 * @file config.h
 * @brief file contenente alcune define con valori massimi utilizzati
 * @author Remo Andreoli 535485
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */

#if !defined(CONFIG_H_)
#define CONFIG_H_

#define MAX_NAME_LENGTH                 32

/* aggiungere altre define qui */
#define SUCCESS 			1
#define CONNECTION_FAILURE 		0
#define FAILURE 			-1 //ricordati di settare errno
#define MAX_CONF_STRING_LENGTH 		512 //lunghezza massima stringa di chatty.conf
#define SERVER_ATTRIBUTE 		8 //numero di attributi per inizializzare il server
#define HASHTABLE_SIZE			512 //in quanti bucket suddividere l'hashtable (possibilmente potenze di due)
#define HASHTABLE_REGION		4 //in quante regioni suddividere l'hashtable (possibilmente potenze di due)
#define CONN_MUTEXES			4 //quante mutex dedicare alle connessioni (valore albitrario)
#define WORKER_TERMINATION_SIGNAL	-1 //messaggio speciale per terminare i thread worker

// to avoid warnings like "ISO C forbids an empty translation unit"
typedef int make_iso_compilers_happy;

#endif /* CONFIG_H_ */
