/**
 * @file listener.c
 * @brief implementazione delle funzioni del listener in server.h
 * @author Remo Andreoli 535485
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore 
 */

#include<unistd.h>
#include<sys/select.h>
#include<sys/time.h>
#include<sys/socket.h>
#include<pthread.h>
#include"pthreadUtils.h"
#include"config.h"
#include"utils.h"
#include"message.h"
#include"server.h"
#include"stats.h"
#include"connections.h"

static listener_args* lisArgs; //conterra' la struttura dati dei parametri del listener thread

void* initListener(void* args){
	int notused=0;
	int running=1;
	struct timeval time; //struttura usata dalla select per indicare l'intervallo di tempo tra un controllo e un altro

	long fd_sk=0; //fd comunicazione listener-clients
	long fd_c=0; //fd di I/O di un client
	long fd_num=0; //numero massimo di fd attivi
	long fd=0; //per verificare i risultati di select
		
	lisArgs=(listener_args*) args; //inizializzo la struttura dati dei parametri
	Queue_t* fds=lisArgs->fds; //coda concorrente dei fd da far processare al worker
	pthread_mutex_t* mtxPipe=lisArgs->mtx;
	fd_set set; //l'insieme di fd attivi
        fd_set precSet; //l'insieme di fd attivi prima dell'ultima richiesta
        fd_set rdset; //insieme di fd in attesa di lettura
	
	//bind e listening su fd_sk
	CHECKER(fd_sk, socket(AF_UNIX, SOCK_STREAM, 0), "creating socket");
	CHECKER(notused, bind(fd_sk, (struct sockaddr*) lisArgs->sa, sizeof(*(lisArgs->sa))), "binding socket to fd_sk");
	CHECKER(notused, listen(fd_sk, SOMAXCONN), "listening on fd_sk");	
	if(fd_sk>fd_num) fd_num=fd_sk;
	
	printf("Created socket at '%s'\n", (lisArgs->sa)->sun_path);

	//setto a 0 le maschere e a 1 il bit di comunicazione
	FD_ZERO(&set);
	FD_ZERO(&rdset);
	FD_ZERO(&precSet);
	FD_SET(fd_sk, &set);
	precSet=set;

	//setto il timeout
	time.tv_usec=500;
        time.tv_sec=0;
	
	while(running){
		int selectRes=0; //usato per controllare il valore di select(in caso di signal, timeout o altro)
		struct timeval tmpTime=time; //uso una variabile temporanea, perche' Linux modifica timeval per indicare il tempo rimanente 
		long pipedFd=0;
               		
		Pthread_mutex_lock(mtxPipe);
                if(readn(lisArgs->rPipe, &pipedFd, sizeof(pipedFd))!=-1){
                        FD_SET(pipedFd, &set);
                }
		else{
			if(errno!=EAGAIN){
				running=0;
			}
		}
	        Pthread_mutex_unlock(mtxPipe);
		if(running==0) continue;

		rdset=set; //rinizializzo ogni volta perche' la select modifica rdset
		selectRes=select(fd_num+1, &rdset, NULL, NULL, &tmpTime); //mi blocco qui fino a quanto non arriva una read o scatta time
		if(selectRes==-1){ //ritornato errore
			perror("errore select");
			exit(FAILURE);
		}
		else if(selectRes==0){ //e' scaduto il quanto di tempo
			//set=precSet;
			continue;
		}
		
		//se arrivo qua non ci sono stati ne' errori, ne' segnali e non e' scaduto il quanto di tempo nell'attesa di select
		precSet=set; //salvo il set prima di modificarlo
		for(fd=0; fd<=fd_num;fd++){
			if(FD_ISSET(fd, &rdset)){
				if(fd==fd_sk){ //e' pronto il socket di comunicazione, devo creare un socket di I/O per un nuovo client
					CHECKER(fd_c, accept(fd_sk, NULL, 0), "creating fd for chatting");
					FD_SET(fd_c, &set);
					
					Pthread_mutex_lock(mtxStats);
					if(chattyStats.nonline>=(lisArgs->maxConnections)){ //troppi utenti, chiudo connessione
						message_hdr_t reply;
						setHeader(&reply, OP_FAIL, "server");
                                                sendHeader(fd_c, &reply);
						
						FD_CLR(fd_c, &set);
						close(fd_c);
					}
					else{
						if(fd_c>fd_num) fd_num = fd_c;
					}
					Pthread_mutex_unlock(mtxStats);
				}
				else{ //e' pronto un socket I/O, lo aggiungo alla coda dei fd per darlo in pasto ai thread worker
					FD_CLR(fd, &set); //levo dalla lista (eventualmente rimesso dal worker)
					push(fds, (void*) fd);
				}
			}
		}		
	}
	
	//chiudo i fd	
	close(fd_c);
	for (fd = 0; fd <= fd_num; fd++){
		shutdown(fd, SHUT_RDWR);
	}

	int i;
	long term=WORKER_TERMINATION_SIGNAL;	
	//poi la riempio con i segnali di terminazione
	//tanti quanti sono i thread piu' quanti messaggi sono in coda	
	for(i=0;i<lisArgs->workers;i++){
		push(fds, (void*) term);	
	}
	
	return NULL;
}
