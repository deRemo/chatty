/*
 * membox Progetto del corso di LSO 2017/2018
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 * 
 */
/**
 * @file chatty.c
 * @brief File principale del server chatterbox
 * @author Remo Andreoli 535485
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore 
 */
#define _POSIX_C_SOURCE 200809L
#include<stdio.h>
#include<fcntl.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<unistd.h>
#include<sys/mman.h>
#include<stdlib.h>
#include<assert.h>
#include<string.h>
#include<signal.h>
#include<pthread.h>
#include<sys/un.h>
#include<sys/socket.h>
#include<sys/select.h>
#include<signal.h>
#include<time.h>
#include"message.h"
#include"connections.h"
#include"ops.h"
#include"message.h"
#include"historyQueue.h"
#include"server.h"
#include"icl_hash.h"
#include"stats.h"
#include"config.h"
#include"utils.h"
#include"pthreadUtils.h"

struct statistics  chattyStats = { 0,0,0,0,0,0,0 }; //memorizza le statistiche del server(definita in stats.h)
pthread_mutex_t* mtxStats; //mutex per accedere a chattyStats

icl_hash_t* registeredUsers; //hashtable contenente gli utenti registrati (definita in icl_hash.h)
Queue_t* onlines; //lista concorrente che mantiene gli utenti online
pthread_mutex_t** mtxOnlines; //pool dei mutex da assegnare agli utenti online
pthread_mutex_t* mtxPick; //mutex per evitare race condition nella fase di assegnazione della mutex all'utente
int* used; //array per appuntare i mutex assegnati(0 mutex libero, 1 mutex associato)

char configArray[SERVER_ATTRIBUTE][MAX_CONF_STRING_LENGTH]; //array di stringhe contenente i parametri di configurazione

static void usage(const char *progname) {
    fprintf(stderr, "Il server va lanciato con il seguente comando:\n");
    fprintf(stderr, "  %s -f conffile\n", progname);
}

void updateStats(unsigned long x0, unsigned long x1, unsigned long x2, unsigned long x3, unsigned long x4, unsigned long x5, unsigned long x6){
        Pthread_mutex_lock(mtxStats);
	
	chattyStats.nusers+=x0;
        chattyStats.nonline+=x1;
        chattyStats.ndelivered+=x2;
        chattyStats.nnotdelivered+=x3;
        chattyStats.nfiledelivered+=x4;
        chattyStats.nfilenotdelivered+=x5;
        chattyStats.nerrors+=x6;
	
	Pthread_mutex_unlock(mtxStats);
}

void* sigHandler(void* noargs){
	sigset_t waitSet; //bitmap dei segnali da aspettare
	int running=1;
	int notused;
	int signal; //conterra' la macro del segnale arrivato

	//setto i segnali da aspettare
	CHECKER(notused, sigemptyset(&waitSet), "zeroing mask");
        CHECKER(notused, sigaddset(&waitSet, SIGQUIT), "wait SIGQUIT1");
        CHECKER(notused, sigaddset(&waitSet, SIGUSR1), "wait SIGUSR1");
        CHECKER(notused, sigaddset(&waitSet, SIGINT), "wait SIGINT");
        CHECKER(notused, sigaddset(&waitSet, SIGTERM), "wait SIGTERM");
	
	while(running){
		if(sigwait(&waitSet, &signal)==0){ //arrivato segnale
			if(signal==SIGUSR1){ //appendo le statistiche nel relativo file
				Pthread_mutex_lock(mtxStats);
				
				FILE* statFile;	
                                statFile=fopen(configArray[StatFileName], "a");
				if(statFile!=NULL){
					printStats(statFile);	
					printf("salvate stats in %s\n", configArray[StatFileName]);	
				}
				else printf("ERRORE APERTURA %s\n", configArray[StatFileName]);
				
				Pthread_mutex_unlock(mtxStats);
			}	
			else{
				running=0;
			}	
		}
		else printf("SIGNAL HANDLER THREAD ERROR\n");	
	}	
	pthread_exit((void*) SUCCESS);
}

int main(int argc, char *argv[]) {
	Queue_t* fds; //coda concorrente dei fd
	
	int* w2l; //pipe con cui i worker comunicano al listener
	pthread_mutex_t* mtxPipe; //mutex condiviso per scrivere/leggere nella pipe
	
	listener_args* lisArgs; //struttura dati che contiene i parametri del thread listener (definita in server.h)
	worker_args* workArgs; //struttura dati che contiene i parametri dei thread worker (definita in server.h)
	tPool* threadPool; //struttura del thread pool (definita in server.h)
	
	struct sockaddr_un sa; //per creare l'indirizzo su cui bindare il connection socket
	struct sigaction sig; //per personalizzare la gestione dei segnali
        sigset_t ignSet; //bitmap dei segnali da ignorare con sigmask
	int notused;
	
	if(argc<3){
                usage(argv[0]);
                return FAILURE;
        }
	
	//Parsing del file di configurazione e preparazione dell'array con i parametri di configurazione
	CHECKER(notused, parseFile(argv[2],configArray), "parsing conf file");	
	
	//Inizializzazione coda concorrente dei file descriptor
	fds=initQueue();

	//Inizializzazione coda concorrente degli utenti online	
	onlines=initQueue();
	
	//Inizializzazione tabella hash
	registeredUsers=(icl_hash_t*) icl_hash_create(HASHTABLE_SIZE, NULL, NULL);
	
	//Inizializzazione mutex per chattyStats
        mtxStats=(pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
        Pthread_mutex_init(mtxStats, NULL);
	
	//Inizializzione mutex per l'assegnamento del mutex all'utente online
	mtxPick=(pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
	Pthread_mutex_init(mtxPick, NULL);

	//Inizializzione pool di mutex da assegnare agli utenti online
	mtxOnlines=(pthread_mutex_t**) malloc(atoi(configArray[MaxConnections])*sizeof(pthread_mutex_t));
	for(int i=0; i<atoi(configArray[MaxConnections]); i++){
		mtxOnlines[i]=(pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));	
		Pthread_mutex_init(mtxOnlines[i], NULL);
	}
	used=(int*) calloc(atoi(configArray[MaxConnections]), sizeof(pthread_mutex_t));

	//Inizializzazione e avvio del thread gestore segnali
	memset(&sig, 0, sizeof(sig));
	sig.sa_handler=SIG_IGN;	//ignora il segnale in arrivo
	CHECKER(notused, sigaction(SIGPIPE, &sig, NULL), "installing gestore"); //ignoro completamente sigpipe(ereditato anche da thread signal handler)	
		
	CHECKER(notused, sigemptyset(&ignSet), "zeroing mask");
	CHECKER(notused, sigaddset(&ignSet, SIGQUIT), "ignoring SIGQUIT1");
	CHECKER(notused, sigaddset(&ignSet, SIGUSR1), "ignoring SIGUSR1");
	CHECKER(notused, sigaddset(&ignSet, SIGINT), "ignoring SIGINT");
	CHECKER(notused, sigaddset(&ignSet, SIGTERM), "ignoring SIGTERM");
	CHECKER(notused, pthread_sigmask(SIG_SETMASK, &ignSet, NULL), "setting main sigmask"); //maschero i segnali nel main
	
	pthread_t sigHandlerThread;
	Pthread_create(&sigHandlerThread, NULL, sigHandler, NULL);
	
	//Inizializzo la pipe non bloccante e la sua mutex
	w2l=(int*) malloc(2*sizeof(int));
	CHECKER(notused, pipe(w2l), "creating pipe");
	CHECKER(notused, fcntl(w2l[0], F_SETFL, O_NONBLOCK), "setting non blocking pipe");
	
	mtxPipe=(pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
	Pthread_mutex_init(mtxPipe, NULL);
		
	//Distruzione di socket precedente e creazione indirizzo
	unlink(configArray[UnixPath]);
	memset(&sa, '0', sizeof(sa));
	strncpy(sa.sun_path, configArray[UnixPath], strlen(configArray[UnixPath])+1);	
	sa.sun_family=AF_UNIX;
		
	//Inizializzazione struttura parametri listener
	lisArgs=(listener_args*) malloc(sizeof(listener_args));
	lisArgs->fds=fds;
	lisArgs->mtx=mtxPipe;
	lisArgs->sa=&sa;
	lisArgs->rPipe=w2l[0];
	lisArgs->workers=atoi(configArray[ThreadsInPool]);
	lisArgs->maxConnections=atoi(configArray[MaxConnections]);

	//Inizializzazione thread listener
	pthread_t listenerThread;
	Pthread_create(&listenerThread, NULL, initListener, (void*) lisArgs);		

	//Iniziliazzazione struttura parametri worker
	workArgs=(worker_args*) malloc(sizeof(worker_args));
	workArgs->fds=fds;
	workArgs->wPipe=w2l[1];
	workArgs->mtx=mtxPipe;	

	//Inizializzazione thread pool e avvio dei thread
        threadPool=(tPool*) initThreadPool(atoi(configArray[ThreadsInPool]));
        startThreads(threadPool, initWorker, workArgs);

	//Attesa termine del signal handler thread
	Pthread_join(sigHandlerThread, NULL);
	
	/* INIZIO FASE DI CHIUSURA DEL SERVER */
	printf("\nCHIUSURA SERVER\n\n");
	
	//Chiusura pipe di comunicazione; causa la procedura di chiusura di thread listener e workers
	Pthread_mutex_lock(mtxPipe);
	close(*w2l);
	free(w2l);
	Pthread_mutex_unlock(mtxPipe);

	//Attesa terminazione del thread listener	
	Pthread_join(listenerThread, NULL);
	printf("chiuso listener\n");

	//Free struttura con i parametri del listener thread
	free(lisArgs);
	
	//Free thread pool
        joinThreads(threadPool);
        destroyThreadPool(threadPool);
	printf("chiuso threadpool\n");

	//Free struttura con i parametri del listener thread
	free(workArgs);
	
	//Free mutex pipe
	Pthread_mutex_destroy(mtxPipe);
	free(mtxPipe);
	
	//Free coda concorrente dei descrittori	
	deleteQueue(fds, NULL);
	printf("chiusa coda fd\n");

	//Free coda concorrente dei user online
	deleteQueue(onlines, freeUser);	
	printf("chiusa coda utenti online\n");
	
	//Free mutexes degli utenti online
        Pthread_mutex_destroy(mtxPick);
	free(mtxPick);

	for(int i=0; i<atoi(configArray[MaxConnections]); i++){
                Pthread_mutex_destroy(mtxOnlines[i]);
		free(mtxOnlines[i]);
        }
        free(used);
        free(mtxOnlines);
	
	//Distruzione della hash table dei client registrati
	//e delle corrispondenti history queue
	icl_hash_destroy(registeredUsers, freeNick, freeHistory);
	printf("chiusa hashtable utenti registrati\n");
	
	//Stampo a schermo le statistiche piu' recenti
	printStats(stdout);
	
	//Distruzione mutex di chattyStats
        Pthread_mutex_destroy(mtxStats);
        free(mtxStats);
	
	return 0;
}

//ritorna l'indice del mtx da associare all'utente connesso
int pickMtx(){
	for(int i=0; i<atoi(configArray[MaxConnections]); i++){
		if(used[i]==0){
			used[i]=1;
			return i;
		}
	}
	return FAILURE;
}

void whichOperation(op_t op, message_t msg, long fd){
	switch(op){
                case REGISTER_OP:
                        registerUser(msg.hdr.sender, fd, atoi(configArray[MaxHistMsgs]));
                        break;
                case CONNECT_OP:
                        connectUser(msg.hdr.sender, fd);
                        break;
                case POSTTXT_OP:
                        sendText(msg, fd);
                        break;
                case POSTTXTALL_OP:
                        sendTextToAll(msg, fd);
                        break;
                case POSTFILE_OP:
                        sendFile(msg, fd);
                        break;
                case GETFILE_OP:
                        getFile(msg, fd);
                        break;
                case GETPREVMSGS_OP:
                        listHistory(msg.hdr.sender, fd);
                        break;
                case USRLIST_OP:
                        listOnlineUsers(msg.hdr.sender, fd);
                        break;
                case UNREGISTER_OP:
                        unregisterUser(msg.hdr.sender, fd);
                        break;
                case DISCONNECT_OP:
                        disconnectUser(fd);
                        break;
                default:{
                        printf("OP NON PRESENTE\n");
                }
        }
}

void registerUser(char* nick, long fd, int maxHistMsgs){
	message_hdr_t replyHeader; //ack di risposta al client
	char* registeredNick=calloc((MAX_NAME_LENGTH+1),sizeof(char)); //key da inserire nell'hashtable
	qHistory* history=NULL;
	
	strncpy(registeredNick, nick, MAX_NAME_LENGTH+1);
        if(icl_hash_find(registeredUsers, registeredNick)!=NULL){ //utente gia' registrato
		setHeader(&replyHeader, OP_NICK_ALREADY, "server");
                sendHeader(fd, &replyHeader);
                
                printf("utente gia' registrato\n");
		updateStats(0,0,0,0,0,0,1);
	
		free(registeredNick);
	}
	else{
                history=initHistory(maxHistMsgs);
		if(icl_hash_insert(registeredUsers, registeredNick, history)!=NULL){
			updateStats(1,0,0,0,0,0,0);
			
			connectUser(registeredNick, fd);
		}
		else{
			setHeader(&replyHeader, OP_FAIL, "server");
                	sendHeader(fd, &replyHeader);
			updateStats(0,0,0,0,0,0,1);
			free(history);
		}
	}
}

void connectUser(char* nick, long fd){
	user* u=NULL;
	message_hdr_t replyHeader;

	if(icl_hash_find(registeredUsers, nick)!=NULL){ //utente registrato
		u=(user*) malloc(sizeof(user));
		strncpy(u->nick, nick, MAX_NAME_LENGTH+1);
		u->fd=fd;
		Pthread_mutex_lock(mtxPick);
		int idx=pickMtx();
		u->idx=idx;
		Pthread_mutex_unlock(mtxPick);

		if(findNick(onlines, nick)==NULL){ //ma non connesso
			push(onlines, u);
			updateStats(0,1,0,0,0,0,0);
			listOnlineUsers(nick, fd);
		}
		else{
			setHeader(&replyHeader, OP_NICK_ALREADY, "server");
            		sendHeader(fd, &replyHeader);
			
			updateStats(0,0,0,0,0,0,1);
			freeUser(u);
		}
	}
	else{
		setHeader(&replyHeader, OP_NICK_UNKNOWN, "server");
         	sendHeader(fd, &replyHeader);
                 
         	updateStats(0,0,0,0,0,0,1);
	}
}

void sendText(message_t msg, long fd){
	message_hdr_t replyHeader;
	message_t toSend;
	user* dest=NULL;
	qHistory* destHist;
	user* mitt=NULL;

	//retrieve struttura mittente per usare la mutex di connessione
        if((mitt=(user*) findNick(onlines, msg.hdr.sender))==NULL){
                printf("MITTENTE NON ONLINE: ERRORE\n");
                exit(FAILURE);
        }
	
	//check lunghezza valida e che mittente!=destinatario
	if(msg.data.hdr.len<atoi(configArray[MaxMsgSize]) && strcmp(msg.data.hdr.receiver, msg.hdr.sender)!=0){
		if((destHist=(qHistory*) icl_hash_find(registeredUsers, msg.data.hdr.receiver))!=NULL){ //destinatario e' registrato
			int update=0; //settato a 1 se il dest e' connesso e l'invio va a buon fine
	
			//se il destinatario e' connesso, gli invio direttamente un nuovo messaggio text
			if((dest=(user*) findNick(onlines, msg.data.hdr.receiver))!=NULL){
				setHeader(&(toSend.hdr), TXT_MESSAGE, msg.hdr.sender);
				setData(&(toSend.data), msg.data.hdr.receiver, msg.data.buf, msg.data.hdr.len);
				
				Pthread_mutex_lock(mtxOnlines[dest->idx]);
				if(sendRequest(dest->fd, &toSend)!=FAILURE){
					update=1;
				}

				Pthread_mutex_unlock(mtxOnlines[dest->idx]);
			}
			addMsg(destHist, msg, update);
			if(update==1) updateStats(0,0,1,-1,0,0,0);
                        updateStats(0,0,0,1,0,0,0);
			
			setHeader(&replyHeader, OP_OK, "server");
            		Pthread_mutex_lock(mtxOnlines[mitt->idx]);
			sendHeader(fd, &replyHeader);
			Pthread_mutex_unlock(mtxOnlines[mitt->idx]);
        	}	  
		else{
			setHeader(&replyHeader, OP_NICK_UNKNOWN, "server");
                        Pthread_mutex_lock(mtxOnlines[mitt->idx]);
			sendHeader(fd, &replyHeader);
                        Pthread_mutex_unlock(mtxOnlines[mitt->idx]);
			
			updateStats(0,0,0,0,0,0,1);
		}
	}
	else{
        	setHeader(&replyHeader, OP_MSG_TOOLONG, "server");
		Pthread_mutex_lock(mtxOnlines[mitt->idx]);
                sendHeader(fd, &replyHeader);
		Pthread_mutex_unlock(mtxOnlines[mitt->idx]);
        	updateStats(0,0,0,0,0,0,1);
	}
}

void sendTextToAll(message_t msg, long fd){
        int i;
        user* mitt=NULL;
	message_t toSend; //messagio da inviare, se l'utente e' connesso
	message_t toHistory; //messaggio da salvare in history
	icl_entry_t *bucket, *curr, *next;
	
	//retrieve struttura mittente per usare la mutex di connessione
        if((mitt=(user*) findNick(onlines, msg.hdr.sender))==NULL){
                printf("MITTENTE NON ONLINE: ERRORE\n");
                exit(FAILURE);
        }

	if(msg.data.hdr.len<atoi(configArray[MaxMsgSize])){ //lunghezza valida
		for(i=0; i<registeredUsers->nbuckets; i++){
                	bucket = registeredUsers->buckets[i];
                	for (curr=bucket; curr!=NULL; ) { //cerco entry valida
                        	next=curr->next;

                        	if(strcmp(curr->key, msg.hdr.sender)!=0){ //non e' il mittente stesso
                                	qHistory* destHist=(qHistory* )curr->data;
					user* dest=NULL;
					int update=0;	
						
					setHeader(&(toHistory.hdr), TXT_MESSAGE, msg.hdr.sender);	
					setData(&(toHistory.data), (char*) curr->key, msg.data.buf, msg.data.hdr.len);
					
					//se e' connesso, gli invio direttamente il messagio
					if((dest=(user*) findNick(onlines, curr->key))!=NULL){
						setHeader(&(toSend.hdr), TXT_MESSAGE, msg.hdr.sender);
                                        	setData(&(toSend.data), dest->nick, msg.data.buf, msg.data.hdr.len);

						Pthread_mutex_lock(mtxOnlines[dest->idx]);
						if(sendRequest(dest->fd, &toSend)!=FAILURE){
                                			update=1;
						}
						
						Pthread_mutex_unlock(mtxOnlines[dest->idx]);
					}

					addMsg(destHist, toHistory, update);				
					if(update==1) updateStats(0,0,1,-1,0,0,0);
                        		updateStats(0,0,0,1,0,0,0);
                        	}
                        	curr=next;
                	}
        	}
		setHeader(&(msg.hdr), OP_OK, "server");
        	
		Pthread_mutex_lock(mtxOnlines[mitt->idx]);
		sendHeader(fd, &(msg.hdr));
		Pthread_mutex_unlock(mtxOnlines[mitt->idx]);
	}
	else{
		setHeader(&(msg.hdr), OP_MSG_TOOLONG, "server");
        	
		Pthread_mutex_lock(mtxOnlines[mitt->idx]);
		sendHeader(fd, &(msg.hdr));
		Pthread_mutex_unlock(mtxOnlines[mitt->idx]);
		
		updateStats(0,0,0,0,0,0,1);
	}
}

void sendFile(message_t msg, long fd){
	message_hdr_t replyHeader; //reply per il mittente
	message_data_t sentFile; //conterra' il contenuto del file inviato
	message_t toSend;
	user* dest=NULL; //struttura del destinatario
        user* mitt=NULL;
	qHistory* destHist; //history del destinatario
	char* dirpath=strncat(configArray[DirName], "/", 2); //path DirName
	
	//retrieve struttura mittente per usare la mutex di connessione
        if((mitt=(user*) findNick(onlines, msg.hdr.sender))==NULL){
                printf("MITTENTE NON ONLINE: ERRORE\n");
                exit(FAILURE);
        }
	
	//se non esiste la directory DirName, la creo
        if (mkdir(dirpath, 0777) && errno != EEXIST){
                
		setHeader(&replyHeader, OP_FAIL, "server");
        	
		Pthread_mutex_lock(mtxOnlines[mitt->idx]);
		sendHeader(fd, &replyHeader);
		Pthread_mutex_unlock(mtxOnlines[mitt->idx]);

        	updateStats(0,0,0,0,0,0,1);
		perror("mkdir DirName");	
		return;
        }
	
        readData(fd, &(sentFile));
	
	//check lunghezza file(in kb) e che mittente!=destinatario
        if((sentFile.hdr.len/1024)<atoi(configArray[MaxFileSize]) && strcmp(msg.data.hdr.receiver,msg.hdr.sender)!=0){
                if((destHist=(qHistory*) icl_hash_find(registeredUsers, msg.data.hdr.receiver))!=NULL){ //destinatario e' registrato
			int pathlen=strlen(dirpath)+strlen(msg.data.buf)+1;
			char* filename=calloc(pathlen, sizeof(char));
		
			//creo il path del nuovo file, cancellando eventuali cartelle discendenti
			strncat(filename, dirpath, pathlen);
			char* last=strrchr(msg.data.buf, '/'); //ritorna la stringa dopo l'ultimo slash	
			if(last!=NULL){
				last++;
				strncat(filename, last, msg.data.hdr.len - (last - msg.data.buf));	
				setData(&(toSend.data), msg.data.hdr.receiver, last, strlen(last));
			}
			else{
				strncat(filename, msg.data.buf, msg.data.hdr.len);
				setData(&(toSend.data), msg.data.hdr.receiver, msg.data.buf, msg.data.hdr.len);
			}
			setHeader(&(toSend.hdr), FILE_MESSAGE, msg.hdr.sender);

			//apro o creo il nuovo file con permessi universali, per poterci scrivere sopra	
			int fp; 
			if((fp=open(filename, O_RDWR | O_CREAT, 0777))==-1){
				perror("open file");
				
				free(filename);
				close(fp);
				free(sentFile.buf);
				return;
			}
			
			//mappo il contenuto del nuovo file in memoria
			char* mappedFile;
			if((mappedFile=mmap(NULL, sentFile.hdr.len, PROT_READ | PROT_WRITE, MAP_SHARED, fp, 0))==(void*) FAILURE){
				perror("mapping file");
				
				setHeader(&replyHeader, OP_NO_SUCH_FILE, "server");
                        
				Pthread_mutex_lock(mtxOnlines[mitt->idx]);
				sendHeader(fd, &replyHeader);
				Pthread_mutex_unlock(mtxOnlines[mitt->idx]);

				close(fp);
				free(filename);
				updateStats(0,0,0,0,0,0,1);
			}
			else{	
				//setto la size del nuovo file, senno' fallisce la memcpy
				ftruncate(fp, sentFile.hdr.len);
			
				//copio il contenuto del file inviato alla area di memoria che conterra' il nuovo file
				memcpy(mappedFile, sentFile.buf, sentFile.hdr.len);	
				sentFile.hdr.len--;

				updateStats(0,0,0,0,0,1,0); //prima aggiungo il messaggio alla history
				int update=0;
				if((dest=(user*) findNick(onlines, msg.data.hdr.receiver))!=NULL){ //destinatario connesso
					Pthread_mutex_lock(mtxOnlines[dest->idx]);
					if(sendRequest(dest->fd, &toSend)!=FAILURE){
						update=1;
					}
					//non devo fare update di nfiledelivered, ci pensa getFile
					Pthread_mutex_unlock(mtxOnlines[dest->idx]);
				}
				addMsg(destHist, toSend, update);
				if(update==1) updateStats(0,0,0,0,1,-1,0);			
				setHeader(&replyHeader, OP_OK, "server");
            			
				Pthread_mutex_lock(mtxOnlines[mitt->idx]);
				sendHeader(fd, &replyHeader);
            			Pthread_mutex_unlock(mtxOnlines[mitt->idx]);

				close(fp);
				free(filename);
			}
                }
                else{
                        setHeader(&replyHeader, OP_NICK_UNKNOWN, "server");
                       	
			Pthread_mutex_lock(mtxOnlines[mitt->idx]); 
			sendHeader(fd, &replyHeader);
			Pthread_mutex_unlock(mtxOnlines[mitt->idx]);
			
			updateStats(0,0,0,0,0,0,1);
                }
        }
        else{
                setHeader(&replyHeader, OP_MSG_TOOLONG, "server");
               
	       	Pthread_mutex_lock(mtxOnlines[mitt->idx]);
		sendHeader(fd, &replyHeader);
                Pthread_mutex_unlock(mtxOnlines[mitt->idx]);

		updateStats(0,0,0,0,0,0,1);
        }
	free(sentFile.buf);	
}

void getFile(message_t msg, long fd){
	message_t toSend;
	message_hdr_t replyHeader;
	qHistory* hist=(qHistory*) icl_hash_find(registeredUsers, msg.hdr.sender);
	user* u=(user*) findNick(onlines, msg.hdr.sender);
	
	if(hist!=NULL && u!=NULL){ //utente connesso e registrato	
		int pathlen=strlen(configArray[DirName])+msg.data.hdr.len;
                char* filename=calloc(pathlen,sizeof(char));
		strncat(filename, configArray[DirName], pathlen);
                strncat(filename, msg.data.buf, msg.data.hdr.len);

		
		int fp;
		if((fp=open(filename, O_RDONLY))==-1){
			perror("open file");
			setHeader(&replyHeader, OP_NO_SUCH_FILE, "server");
                        
			Pthread_mutex_lock(mtxOnlines[u->idx]);
			sendHeader(fd, &replyHeader);
			Pthread_mutex_unlock(mtxOnlines[u->idx]);

			close(fp);	
			free(filename);
			updateStats(0,0,0,0,0,0,1);
			return;
		}
		else{
			//verifica della dimensione del file
			struct stat st;
			if(stat(filename, &st)==-1 || !S_ISREG(st.st_mode)){ //grandezza invalida o file non regolare
				setHeader(&replyHeader, OP_NO_SUCH_FILE, "server");
                        
				Pthread_mutex_lock(mtxOnlines[u->idx]);	
                        	sendHeader(fd, &replyHeader);
                        	Pthread_mutex_unlock(mtxOnlines[u->idx]);

				close(fp);
                        	free(filename);
                        	updateStats(0,0,0,0,0,0,1);
                        	return;
			}		
			int fileSize=st.st_size;
			char* mappedFile;
                        if((mappedFile=mmap(NULL, fileSize, PROT_READ, MAP_PRIVATE, fp, 0))==(void*) FAILURE){
                                perror("mapping file");

                                setHeader(&replyHeader, OP_NO_SUCH_FILE, "server");
                               	
			       	Pthread_mutex_lock(mtxOnlines[u->idx]);	
                        	sendHeader(fd, &replyHeader);
                        	Pthread_mutex_unlock(mtxOnlines[u->idx]);
				
                                close(fp);
                                free(filename);
                                updateStats(0,0,0,0,0,0,1);
                        }
			else{
				setHeader(&(toSend.hdr), OP_OK, "server");
				setData(&(toSend.data), msg.hdr.sender, mappedFile, fileSize);
				
				Pthread_mutex_lock(mtxOnlines[u->idx]);
				if(sendRequest(u->fd, &toSend)==FAILURE){
					printf("IL DESTINATARIO SI E' DISCONNESSO\n");
                                }
				Pthread_mutex_unlock(mtxOnlines[u->idx]);

				free(filename);
				close(fp);
			}
		}	
	}
	else{
		setHeader(&replyHeader, OP_NICK_UNKNOWN, "server");
                sendHeader(fd, &replyHeader);

                updateStats(0,0,0,0,0,0,1);
	}
}

void listHistory(char* nick, long fd){
	message_hdr_t replyHeader;
	message_data_t replyData;
	qHistory* hist=icl_hash_find(registeredUsers, nick);
	user* u=(user*) findNick(onlines, nick);

	if(hist!=NULL && u!=NULL){ //utente registrato e connesso
		int i;
	
		Pthread_mutex_lock(mtxOnlines[u->idx]);
		
		size_t msgCount=(hist->length)-(hist->freeSpace); //numero di messaggi nella history 
		setHeader(&replyHeader, OP_OK, "server");
                sendHeader(fd, &replyHeader);

		setData(&replyData, "server", (const char *) &msgCount, (unsigned int)sizeof(size_t*)); //preparo invio della lunghezza history
		sendData(fd, &(replyData));
		history_node** histMsgs=getArrayHistory(hist);
		
		Pthread_mutex_unlock(mtxOnlines[u->idx]);

		if(msgCount>0){
			if(histMsgs!=NULL){
			int update=0;	
			Pthread_mutex_lock(mtxOnlines[u->idx]);
				for(i=0;i<msgCount;i++){
					if(histMsgs[i]->info.hdr.op==TXT_MESSAGE || histMsgs[i]->info.hdr.op==POSTTXT_OP){
                                                histMsgs[i]->info.hdr.op=TXT_MESSAGE;
                                        }
                                        else if(histMsgs[i]->info.hdr.op==FILE_MESSAGE || histMsgs[i]->info.hdr.op==POSTFILE_OP){
                                                histMsgs[i]->info.hdr.op=FILE_MESSAGE;
                                        }
                                        else{
                                                perror("MESSAGGIO VUOTO");
                                                exit(FAILURE);
                                        }
					
					if(sendRequest(fd, &(histMsgs[i]->info))!=FAILURE){
						update=1;
					}

					if(update==1 && histMsgs[i]->sent==0){ //e' la prima volta che il dest riceve tale messaggio
						if(histMsgs[i]->info.hdr.op==TXT_MESSAGE) updateStats(0,0,1,-1,0,0,0);
						else updateStats(0,0,0,0,1,-1,0);
					
						setSent(hist, i);
					}
				}
			Pthread_mutex_unlock(mtxOnlines[u->idx]);
			free(histMsgs);
			}
		}
	}
	else{
		setHeader(&replyHeader, OP_NICK_UNKNOWN, "server");
               
	       	Pthread_mutex_lock(mtxOnlines[u->idx]);
		sendHeader(fd, &replyHeader);
                Pthread_mutex_unlock(mtxOnlines[u->idx]);

		updateStats(0,0,0,0,0,0,1);
	}
}

void listOnlineUsers(char* nick, long fd){
        message_hdr_t replyHeader;
        message_data_t replyData;
        long n=0;
        char* buf=queue2String(onlines, &n);
	user* u=(user*) findNick(onlines, nick);
	
	if(u!=NULL){ //e' connesso	
		if(buf!=NULL){
                	setData(&replyData, nick, buf, n*(MAX_NAME_LENGTH+1));
        	}
        	else{
                	setData(&replyData, nick, "NESSUNO ONLINE", MAX_NAME_LENGTH+1);
        	}
        	setHeader(&replyHeader, OP_OK, "server");

		Pthread_mutex_lock(mtxOnlines[u->idx]);	
		sendHeader(fd, &replyHeader);
		sendData(fd, &replyData);
        	Pthread_mutex_unlock(mtxOnlines[u->idx]);

		if(buf!=NULL) free(buf);
	}
	else{
		setHeader(&replyHeader, OP_NICK_UNKNOWN, "server");
                sendHeader(fd, &replyHeader);
                updateStats(0,0,0,0,0,0,1);	
	}
}

void disconnectUser(long fd){
		user* u=(user*) findFd(onlines, fd);
		int update=0;

		if(u!=NULL){ //utente connesso
			printf("disconnesso %ld\n", fd);
			Pthread_mutex_lock(mtxPick);
			used[u->idx]=0; //rimuovo associazione mtx-user
			update=removeAtFd(onlines, fd);
			Pthread_mutex_unlock(mtxPick);

			if(update==SUCCESS) updateStats(0,-1,0,0,0,0,0);
		}
}

void unregisterUser(char* nick, long fd){
        message_hdr_t replyHeader;
        user* u=(user*) findNick(onlines, nick);

        if(icl_hash_find(registeredUsers, nick)==NULL){
                printf("utente non registrato\n");
        }
        else{
                icl_hash_delete(registeredUsers, nick, freeNick, freeHistory);
                printf("deregistrato %s\n", nick);

                setHeader(&replyHeader, OP_OK, "server");

                Pthread_mutex_lock(mtxOnlines[u->idx]);
                sendHeader(fd, &replyHeader);
                Pthread_mutex_unlock(mtxOnlines[u->idx]);

                updateStats(-1,0,0,0,0,0,0);
        }
}
