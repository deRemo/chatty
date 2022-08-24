/** 
 * 	@file parser.c
 * 	@brief file che implementa il parser(in utils.h) del file di configurazione del server  
 * 	@author Remo Andreoli 535485 
 * 	Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
 * 	originale dell'autore  
 */

#include<stdio.h>
#include<string.h>
#include<ctype.h>
#include"utils.h"

/* STRUTTURA DEFAULT DEGLI ATTRIBUTI */

/*typedef enum {
	UnixPath 		= 0,
	MaxConnections 		= 1, 
	ThreadsInPool		= 2,
	MaxMsgSize		= 3,
	MaxFileSize		= 4,
      	MaxHistMsgs		= 5,
	DirName			= 6,
	StatFileName		= 7,
	ATTR_COUNT		= SERVER_ATTRIBUTE //in config.h 
} attribute;
*/

char buf[ATTR_COUNT][MAX_CONF_STRING_LENGTH]; //conterra' gli attributi per inizializzare il server

int findAttr(char* str){
	if(strcmp(str, "UnixPath")==0){
		return (int) UnixPath;
	}
	else if(strcmp(str, "MaxConnections")==0){
		return (int) MaxConnections;
	}
	else if(strcmp(str, "ThreadsInPool")==0){
		return (int) ThreadsInPool;
	}
	else if(strcmp(str, "MaxMsgSize")==0){
		return (int) MaxMsgSize;
	}
	else if(strcmp(str, "MaxFileSize")==0){
		return (int) MaxFileSize;
	}
	else if(strcmp(str, "MaxHistMsgs")==0){
		return (int) MaxHistMsgs;
	}
	else if(strcmp(str, "DirName")==0){
		return (int) DirName;
	}
	else if(strcmp(str, "StatFileName")==0){
		return (int) StatFileName;
	}
	else{
		return FAILURE;
	}
}

//stack overflow
//"ripulisce" una stringa dai caratteri speciali, dal delimitatore e mette '\0'
char* clean(char* str){
	int i=0; //index
	int j=0; //writeback index
	char tmp;

	while((tmp=str[i]) != '\0'){
		if(!iscntrl(tmp) && tmp!=' ' && tmp!='='){
			str[j]=tmp;
			j++;
		}
		i++;
	}

	str[j]='\0';

	return str;
}

void retrieveVal(char* str){
	const int ch='=';
	char* value=strrchr(str, ch);
	char* attr=strtok(str, " ");
	int i=0;
	
	if(value!=NULL && attr!=NULL){
		i=findAttr(clean(attr));
		strncpy(buf[i], clean(value), strlen(value));
	}
}

int parseFile(char* filename, char returned[ATTR_COUNT][MAX_CONF_STRING_LENGTH]){
	FILE* f;
	char str[MAX_CONF_STRING_LENGTH];
	int i;

	//verifico se il file esiste	
	if((f=fopen(filename,"r"))==NULL){
		return FAILURE;
	}
	
	while((fgets(str, MAX_CONF_STRING_LENGTH, f))!=NULL){
		if(str[0]!='\n' && str[0]!='#'){ //ignora linee vuote e linee commento
			retrieveVal(str);
		}
	}

	for(i=0;i<ATTR_COUNT;i++){
		strncpy(returned[i], buf[i], strlen(buf[i])+1);
	}

	fclose(f);

	return SUCCESS;
}
