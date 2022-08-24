/** \file connections.c  
       \author Remo Andreoli 535485 
       Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
       originale dell'autore  
*/ 
#include<stdio.h>
#include<errno.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/un.h>
#include"message.h"
#include"utils.h"
#include"config.h"
#include"connections.h"

int readn(long fd, void *buf, size_t size) {
    size_t left = size;
    int r;
    char *bufptr = (char*)buf;
    while(left>0) {
	if ((r=read((int)fd ,bufptr,left)) == -1) {
	    if (errno == EINTR) continue;
	    return -1;
	}
	if (r == 0) return 0;   // gestione chiusura socket
        left    -= r;
	bufptr  += r;
    }
    return size;
}

int writen(long fd, void *buf, size_t size) {
    size_t left = size;
    int r;
    char *bufptr = (char*)buf;
    while(left>0) {
	if ((r=write((int)fd ,bufptr,left)) == -1) {
	    if (errno == EINTR) continue;
	    return -1;
	}
	if (r == 0) return 0;
        left    -= r;
	bufptr  += r;
    }
    return 1;
}

int openConnection(char* path, unsigned int ntimes, unsigned int secs){
	struct sockaddr_un sa;
    	long cfd;
    	int i;

	if(path==NULL){
		errno=EINVAL;
		return FAILURE;	
	}
	//crea address AF_UNIX
	memset(&sa, '0', sizeof(sa));
    	strncpy(sa.sun_path, path, strlen(path)+1);
	sa.sun_family=AF_UNIX;

	//crea socket
	CHECKER(cfd, socket(AF_UNIX, SOCK_STREAM, 0), "create socket");
	for(i=0;i<ntimes;i++){
		if(connect(cfd, (struct sockaddr*)&sa, sizeof(sa))==FAILURE){
			if(errno==ENOENT){ //timeout perche' il server e' occupato
				sleep(secs);
			}
			else{
				return FAILURE;
			}
		}
		else{
			return cfd;
		}
	}
	
	return FAILURE;
}

int readHeader(long connfd, message_hdr_t *hdr){
#if defined(MAKE_VALGRIND_HAPPY)
	memset((char*)hdr, 0, sizeof(message_hdr_t));
#endif
	if(connfd<0 || hdr==NULL){
		errno=EINVAL;
		return FAILURE;
	}
	
	int r;
	r=readn(connfd, hdr, sizeof(message_hdr_t));
	
	if(r==0) return CONNECTION_FAILURE;
	else if(r<0) return FAILURE;
	else return SUCCESS;
}

int readData(long connfd, message_data_t *data){
#if defined(MAKE_VALGRIND_HAPPY)
	memset((char*)data, 0, sizeof(message_data_t));
	memset((char*)&(data->hdr), 0, sizeof(message_data_hdr_t));
#endif
	if(connfd<0 || data==NULL){
		errno=EINVAL;
		return FAILURE;
	}
	
	int r;
	//prima leggo il data header per sapere quanto e' lungo il buffer
	r=readn(connfd, &data->hdr, sizeof(message_data_hdr_t));

	if(r==0) return CONNECTION_FAILURE;
        else if(r<0) return FAILURE;
	
	//success nella read della buffer len
	if(data->hdr.len==0){
		data->buf=NULL;
	}
	else{
		
		data->buf=calloc(sizeof(char),(data->hdr.len)); //alloco spazio necessario al buffer del messaggio

		//leggo byte per byte
		int byte=0;
		while(byte<data->hdr.len){
			r=read(connfd, (data->buf)+byte,1);
			if(r<0) return FAILURE;
			if(r==0) return CONNECTION_FAILURE;
			byte+=r;
		}
	}
	
	return SUCCESS;
}

int readMsg(long connfd, message_t *msg){
#if defined(MAKE_VALGRIND_HAPPY)
        memset((char*)msg, 0, sizeof(message_t));
#endif
	if(connfd<0 || msg==NULL){
		errno=EINVAL;
		return FAILURE;
	}
	
	int rH;
	rH=readHeader(connfd, &msg->hdr);
	
	if(rH==0) return CONNECTION_FAILURE;
	else if(rH<0) return FAILURE;

	//okay read header, ora read data
	int rD;
	rD=readData(connfd, &msg->data);
	
	if(rD==0) return CONNECTION_FAILURE;
	else if(rD<0) return FAILURE;
	else return SUCCESS;
}

int sendData(long fd, message_data_t *msg){
	if(fd<0 || msg==NULL){
		errno=EINVAL;
		return FAILURE;
	}
	
	//invio header
	int wH;
	wH=writen(fd, &msg->hdr, sizeof(message_data_hdr_t));
	if(wH<=0) return FAILURE;
	
	 //invio buffer byte per byte
         int byte=0;
	 int wB=0;
         while(byte<msg->hdr.len){
		 wB=write(fd, (msg->buf)+byte,1);
                 if(wB<=0) return FAILURE;
		 byte+=wB;
         }
	 return SUCCESS;
}


int sendHeader(long fd, message_hdr_t *hdr){
	if(fd<0 || hdr==NULL){
		errno=EINVAL;
		return FAILURE;
	}
	
	int w;
	w=writen(fd, hdr, sizeof(message_hdr_t));

	if(w<=0) return FAILURE;
	else return SUCCESS;
}

int sendRequest(long fd, message_t *msg){
        if(fd<0 || msg==NULL){
                errno=EINVAL;
                return FAILURE;
        }

        int h=sendHeader(fd, &msg->hdr);
        if(h<=0) return FAILURE;

	//okay send header, invio i data
	int d=sendData(fd, &msg->data);
	if(d<=0) return FAILURE;
	else return SUCCESS; 
}
