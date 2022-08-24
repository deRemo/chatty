/**
 * @file listener.c
 * @brief implementazione delle funzioni del listener in server.h
 * @author Remo Andreoli 535485
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore 
 */

#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<unistd.h>
#include<pthread.h>
#include<sys/select.h>
#include"pthreadUtils.h"
#include"config.h"
#include"server.h"
#include"connections.h"
#include"message.h"


void* initWorker(void* args){
	long popFd; //il file descriptor da cui leggere, estratto dalla coda
	int running=1;
	worker_args* workArgs=(worker_args*) args; //parametri del worker
	pthread_mutex_t* mtxPipe=workArgs->mtx;		
	Queue_t* fds=(Queue_t*) workArgs->fds;
	
	while(running){
		int r; //byte letti
		message_t msg; //conterra' il messaggio preso da fd
		popFd=(long) pop(fds);
		
		if(popFd==WORKER_TERMINATION_SIGNAL){ //chiusura server in corso, nessun utente in attesa
			running=0;
			continue;
		}
		
		r=readMsg(popFd, &msg); 
		if(r==SUCCESS){ //tutto ok, esaudisco la richiesta dell'user
			whichOperation(msg.hdr.op, msg, popFd);			
		
			Pthread_mutex_lock(mtxPipe);
               		if(writen(workArgs->wPipe, &popFd, sizeof(popFd))==-1){
				if(errno!=EAGAIN){
					//running=0;
					//ignoro eventuali errori, ci penseranno i messaggi
					//di terminazione del listener a terminare i thread worker
				}
			}
               		Pthread_mutex_unlock(mtxPipe);
		}
		else{ //l'user viene disconnesso(forzata o non)
			close(popFd);
			disconnectUser(popFd);
		}

		if(msg.data.buf!=NULL) free(msg.data.buf);
	}
	
	return NULL;
}
