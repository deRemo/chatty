/**
 *  @file threadPool.c
 *  @brief Implementazione del threadpool
 *  @author Remo Andreoli 535485
 *  Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 *  originale dell'autore
 */

#include<pthread.h>
#include<unistd.h>
#include<stdlib.h>
#include"server.h"
#include"pthreadUtils.h"

//crea il threadpool
tPool* initThreadPool(int n){
	tPool* newPool=malloc(sizeof(tPool));
	
	newPool->threadsInPool=n;
	newPool->threads=(pthread_t*) malloc(n*sizeof(pthread_t));

	return newPool;
}

//fa partire i singoli worker thread
void startThreads(tPool* threadPool, void* (*initWorker)(void *), void* args){
	int i;
	
	for(i=0;i<threadPool->threadsInPool;i++){
		Pthread_create(&(threadPool->threads[i]), NULL, initWorker, (void*) args);	
	}	
}

void joinThreads(tPool* threadPool){
	int i;
	
	//Pthread_cond_broadcast(&(threadPool->fds->emptyList));
	for(i=0;i<threadPool->threadsInPool;i++){
		Pthread_join(threadPool->threads[i], NULL);
	}
}

void destroyThreadPool(tPool* threadPool){
	free(threadPool->threads);
	free(threadPool);
}
