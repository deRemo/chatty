/**
 * @file historyQueue.c
 * @brief file che implementa le code dei messaggi ricevuti da un client
 * @author Remo Andreoli 535485
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore 
 */

/* REQUIRED LIBS & MACROS */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "config.h"
#include "message.h"
#include "connections.h"
#include "historyQueue.h"
#include "pthreadUtils.h"

/* QUEUE FUNCTIONS */

qHistory* initHistory(int n){
	qHistory* q=(qHistory*) malloc(sizeof(qHistory));
	
	q->head=NULL;
	q->tail=NULL;
	q->qlock=(pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));

	Pthread_mutex_init(q->qlock, NULL);	
	q->freeSpace=n;	
	q->length=n;
	
	return q;
}

void addMsg(qHistory* q, message_t s, int isSent){
        history_node* new_history_node=malloc(sizeof(history_node));
	char* msgBuf=malloc((s.data.hdr.len)*sizeof(char));
	message_t newMsg;
	
	strncpy(msgBuf, s.data.buf, s.data.hdr.len);	
	
	setHeader(&newMsg.hdr, s.hdr.op, s.hdr.sender);
	setData(&newMsg.data, s.data.hdr.receiver, msgBuf, s.data.hdr.len);
	
	new_history_node->info=newMsg;
	new_history_node->sent=isSent;
	new_history_node->next=NULL;
	
	Pthread_mutex_lock(q->qlock);	
	if(q->freeSpace>0){
        	if(q->head==NULL){
                	q->head=new_history_node;
                	q->tail=q->head;
        	}
        	else{
                	q->tail->next=new_history_node;
                	q->tail=q->tail->next;
        	}

		q->freeSpace--;
	}
	else{ //zero spazio, levo il messaggio piu' vecchio
		history_node* secondNode=q->head->next;
		free((q->head->info).data.buf);
		free(q->head);

		q->head=secondNode; //il secondo elemento e' la nuova testa
		q->tail->next=new_history_node;
		q->tail=q->tail->next;
	}
	Pthread_mutex_unlock(q->qlock);
}

message_t popMsg(qHistory* q){
	Pthread_mutex_lock(q->qlock);
	
	history_node* secondNode=NULL;
	message_t popMsg=q->head->info;

	secondNode=q->head->next;	
	free(q->head);
	q->head=secondNode;

	q->freeSpace++;
	Pthread_mutex_unlock(q->qlock);	
        
	return popMsg;
}

void setSent(qHistory* q, int n){
	history_node* curr=NULL;
	int i=0;
	
	Pthread_mutex_lock(q->qlock);
	if(q->head!=NULL){
		curr=q->head;
		for(i=0;i<n;i++){
			curr=curr->next;
		}
		curr->sent=1;
	}
	Pthread_mutex_unlock(q->qlock);
}


history_node** getArrayHistory(qHistory* q){
	int msgCount=(q->length)-(q->freeSpace);
	history_node** nodes=NULL;
	history_node* curr=NULL;
	int i=0;

	Pthread_mutex_lock(q->qlock);
	if(q->head!=NULL){
		nodes=(history_node**) malloc(msgCount*sizeof(history_node));
		for(curr=q->head; curr!=NULL; curr=curr->next){
			nodes[i]=curr;
			i++;
		}
	}
	Pthread_mutex_unlock(q->qlock);

	return nodes;
}

int destroyHistory(qHistory* q){
	if(q==NULL) return FAILURE;
	
	int cnt=0;
	int freeSpace=q->freeSpace;

        while(q->head!=NULL){
                message_t toFree=popMsg(q);
        	free(toFree.data.buf);
		cnt++;
	}

	Pthread_mutex_destroy(q->qlock);
	free(q->qlock);

	if(q!=NULL) free(q);

        return cnt==freeSpace; //SUCCESS se ho liberato correttamente la coda
}
