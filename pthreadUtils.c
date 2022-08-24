/* CONCURRENCY FUNCTIONS */

/**
 * 	@file pthreadUtils.c 
 * 	@brief file che implementa la gestione degli errori per pthread.h 
 * 	@author Remo Andreoli 535485 
 * 	Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
 * 	originale dell'autore  
*/

#include<stdio.h>
#include<pthread.h>
#include<stdlib.h>
#include"pthreadUtils.h"

void Pthread_create(pthread_t* t, const pthread_attr_t* attr, void* (*function)(), void* arg){
  	int err;

	if((err=pthread_create(t, attr, function, arg))!=0){
    		perror("pthread create error");
    		exit(err);
  	}
}

void Pthread_join(pthread_t t, void* status){
	int err;
	
	if((err=pthread_join(t,status))!=0){
    		perror("pthread join error");
    		exit(err);
  	}
}

void Pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t *restrict attr){
	int err;
	
	if((err=pthread_mutex_init(mutex,attr))!=0){
		perror("pthread mutex init error");
		exit(err);
	}
}

void Pthread_mutex_destroy(pthread_mutex_t* mutex){
	int err;
	if((err=pthread_mutex_destroy(mutex))!=0){
		perror("pthread mutex destroy error");
		exit(err);
	}
}

void Pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t* attr){
	int err;

	if((err=pthread_cond_init(cond,attr))!=0){
		perror("pthread cond init error");
		exit(err);
	}
}

void Pthread_cond_destroy(pthread_cond_t *cond){
        int err;
        if((err=pthread_cond_destroy(cond))!=0){
                perror("pthread cond destroy error");
                exit(err);
        }
}


void Pthread_mutex_lock(pthread_mutex_t* mtx){
        int err;

        if((err=pthread_mutex_lock(mtx))!=0){ //error
                perror("lock error");
                exit(err);
        }
}

void Pthread_mutex_unlock(pthread_mutex_t* mtx){
        int err;

        if((err=pthread_mutex_unlock(mtx))!=0){ //error
                perror("unlock error");
                exit(err);
        }
}

void Pthread_cond_signal(pthread_cond_t* C){
	int err;
	if((err=pthread_cond_signal(C))!=0){ //error
	    	perror("cond signal error");
		exit(err);
    	}
}

void Pthread_cond_broadcast(pthread_cond_t* C){
        int err;
        if((err=pthread_cond_broadcast(C))!=0){ //error
                perror("cond signal error");
                exit(err);
        }
}

void Pthread_cond_wait(pthread_cond_t* C, pthread_mutex_t* mtx){
        int err;

        if((err=pthread_cond_wait(C,mtx))!=0){ //error
                perror("cond wait error");
		exit(err);
        }
}

