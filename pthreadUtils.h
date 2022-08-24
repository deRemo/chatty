/** 
 * 	@file pthreadUtils.h
 * 	@brief file header per le funzioni di gestione errore di pthread.h  
 * 	@author Remo Andreoli 535485 
 * 	Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
 * 	originale dell'autore  
*/

#include<pthread.h>

void Pthread_create(pthread_t* t, const pthread_attr_t* attr, void * (* function)(), void* arg);
void Pthread_join(pthread_t t, void* status);

void Pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t *restrict attr);
void Pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* attr);

void Pthread_mutex_destroy(pthread_mutex_t * mutex);
void Pthread_cond_destroy(pthread_cond_t *cond);

void Pthread_mutex_lock(pthread_mutex_t* mtx);
void Pthread_mutex_unlock(pthread_mutex_t* mtx);

void Pthread_cond_signal(pthread_cond_t* C);
void Pthread_cond_broadcast(pthread_cond_t* C);
void Pthread_cond_wait(pthread_cond_t* C, pthread_mutex_t* mtx);
