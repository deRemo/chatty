/**
 * 	@file utils.h
 * 	@brief file contenente funzioni/macro/strutture utili in generale  
 * 	@author Remo Andreoli 535485 
 *      Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
 *      originale dell'autore  
 */  
#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include"config.h"

#define FREE(x){ \
	if(x!=NULL){ \
		free(x); \
		x=NULL; \
	} \
}

#define CHECKER(var,sc,err){ \
	if((var=sc)==-1){ \
		perror(#err); \
		exit(EXIT_FAILURE); \
	} \
}

//struttura del file di configurazione
typedef enum {
        UnixPath                = 0,
        MaxConnections          = 1,
        ThreadsInPool           = 2,
        MaxMsgSize              = 3,
        MaxFileSize             = 4,
        MaxHistMsgs             = 5,
        DirName                 = 6,
        StatFileName            = 7,
        ATTR_COUNT              = SERVER_ATTRIBUTE //su config.h
} attribute;

/**
 * @function parseFile
 * @brief Invoca il parser sul file di configurazione desiderato
 *
 * @param filename 	file di configurazione 
 * @param result 	array su cui memorizzare i parametri di configurazione
 *
 * @return <=0 se c'e' stato un errore
 */
int parseFile(char* filename, char returned[SERVER_ATTRIBUTE][MAX_CONF_STRING_LENGTH]);
