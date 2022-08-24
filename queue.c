#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <queue.h>
#include <string.h>
#include "pthreadUtils.h"
#include "config.h"
#include "server.h"
/**
 * @file queue.c
 * @brief File di implementazione dell'interfaccia per la coda, aggiunte alcune funzioni in fondo
 */


/* ------------------- funzioni di utilita' -------------------- */

static Node_t *allocNode()         { return malloc(sizeof(Node_t));  }
static Queue_t *allocQueue()       { return malloc(sizeof(Queue_t)); }
static void freeNode(Node_t *node) { free((void*)node); }
static void LockQueue(Queue_t* q)  { pthread_mutex_lock(q->qlock);   }
static void UnlockQueue(Queue_t* q) { pthread_mutex_unlock(q->qlock); }
static void UnlockQueueAndWait(Queue_t* q) { pthread_cond_wait(q->qcond, q->qlock); }
static void UnlockQueueAndSignal(Queue_t* q) {
    pthread_cond_signal(q->qcond);
    pthread_mutex_unlock(q->qlock);
}

/* ------------------- interfaccia della coda ------------------ */

Queue_t *initQueue() {
    Queue_t *q = allocQueue();
    
    q->qlock=(pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
    q->qcond=(pthread_cond_t*) malloc(sizeof(pthread_cond_t));

    Pthread_mutex_init(q->qlock, NULL);
    Pthread_cond_init(q->qcond, NULL);

    if (!q) return NULL;
    q->head = allocNode();
    if (!q->head) return NULL;
    q->head->data = NULL; 
    q->head->next = NULL;
    q->tail = q->head;    
    q->qlen = 0;
    return q;
}

void deleteQueue(Queue_t *q, void (*free_data)(void*)) {
    while(q->head != q->tail) {
	Node_t *p = (Node_t*)q->head;
	q->head = q->head->next;
	
	if (*free_data && p->next->data) (*free_data)(p->next->data);
	freeNode(p);
    }
    if (q->head) freeNode((void*)q->head);
	
    Pthread_mutex_destroy(q->qlock);
    Pthread_cond_destroy(q->qcond);
    free(q->qlock);
    free(q->qcond);
    free(q);
}

int push(Queue_t *q, void *data) {
    Node_t *n = allocNode();
    n->data = data; 
    n->next = NULL;
    LockQueue(q);
    q->tail->next = n;
    q->tail       = n;
    q->qlen      += 1;
    UnlockQueueAndSignal(q);
    return 0;
}

void *pop(Queue_t *q) {        
    LockQueue(q);
    while(q->head == q->tail) {
	UnlockQueueAndWait(q);
    }
    //locked
    assert(q->head->next);
    Node_t *n  = (Node_t *)q->head;
    void *data = (q->head->next)->data;
    q->head    = q->head->next;
    q->qlen   -= 1;
    assert(q->qlen>=0);
    UnlockQueue(q);
    freeNode(n);
    return data;
} 

// WARNING: accesso in sola lettura non in mutua esclusione
unsigned long length(Queue_t *q) {
    unsigned long len = q->qlen;
    return len;
}

/* FUNZIONI AGGIUNTE DA ME PER SALVARE GLI UTENTI ONLINE*/

void freeUser(void* data){
	user* u=(user*) data;
	
	//free(u->nick);
	free(u);
}

char* queue2String(Queue_t* q, long* n){
	LockQueue(q);
        char* buf=NULL;
	*n=length(q);
	int offset=0;
        
	if(q->head!=q->tail){
                buf=(char*) calloc(length(q)*(MAX_NAME_LENGTH+1), sizeof(char));
                Node_t* curr=(Node_t*) q->head;

                buf[0]='\0';
                while(curr!=q->tail){
			user* u=(user*) curr->next->data;
			strncat(buf+offset*(MAX_NAME_LENGTH+1), u->nick, MAX_NAME_LENGTH+1);
			
			offset++;
                        curr=curr->next;
                }
        }

	UnlockQueue(q);
        return buf;
}

void* findNick(Queue_t* q, char* nick){
        LockQueue(q);
        
	int found=FAILURE; //non trovato
        char* str=calloc(MAX_NAME_LENGTH+1, sizeof(char));
        Node_t* curr=NULL;
	user* u=NULL;

        if(q->head==q->tail) found=FAILURE;
        else{
                curr=(Node_t*) q->head;
                while(curr!=q->tail && found==FAILURE){
                        user* tmp=(user*) curr->next->data;
                        curr=curr->next;

                        if(strcmp(nick, tmp->nick)==0){
                                found=SUCCESS; //trovato
                        	u=tmp;
			}
                }
        }

        free(str);
        UnlockQueue(q);

        return (void*) u;
}

void* findFd(Queue_t* q, long fd){
	LockQueue(q);

        int found=FAILURE; //non trovato
        Node_t* curr=NULL;
        user* u=NULL;

        if(q->head==q->tail) found=FAILURE;
        else{
                curr=(Node_t*) q->head;
                while(curr!=q->tail && found==FAILURE){
                        user* tmp=(user*) curr->next->data;
                        curr=curr->next;

                        if(fd==tmp->fd){
                                found=SUCCESS; //trovato
                                u=tmp;
                        }
                }
        }

        UnlockQueue(q);

        return (void*) u;
}

int removeAtFd(Queue_t* q, long fd){
        LockQueue(q);
        int found=FAILURE;

        if(q->head!=q->tail){
                Node_t* curr=q->head->next;
                Node_t* prec=q->head;
                
		while(prec!=q->tail && found==FAILURE){
			user* u=(user*) curr->data;	
			if(u->fd==fd){
				if(curr!=q->tail){
                                	prec->next=curr->next;
				}
                        	else{
					q->tail=prec;
                        		q->tail->next=prec;
				}
				q->qlen--;
				freeUser(u);
				freeNode(curr);
				found=SUCCESS;
                        }
			else{
				prec=curr;
                        	curr=curr->next;
			}
                }
        }
	
        UnlockQueue(q);
	return found;
}

int removeAtNick(Queue_t* q, char* nick){
	LockQueue(q);
        int found=FAILURE;

        if(q->head!=q->tail){
                Node_t* curr=q->head->next;
                Node_t* prec=q->head;

                while(prec!=q->tail && found==FAILURE){
                        user* u=(user*) curr->data;
                        if(strcmp(u->nick, nick)==0){
                                if(curr!=q->tail){
                                        prec->next=curr->next;
                                }
                                else{
                                        q->tail=prec;
                                        q->tail->next=prec;
                                }
                                q->qlen--;
                                freeUser(u);
                                freeNode(curr);
                                found=SUCCESS;
                        }
                        else{
                                prec=curr;
                                curr=curr->next;
                        }
                }
        }

        UnlockQueue(q);
        return found;
}	
