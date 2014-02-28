/* Multi-threaded DNS Name Resolution Engine that
 * resolves IP addresses to their DNS names.
 * 
 * Shannon Moore - CSCI 3753 PA 2 
 *
 */

#include "multi-lookup.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <semaphore.h>
#include "util.c"
#include "queue.c"
#include <unistd.h>
#include <time.h> 

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h> 
#include <sys/socket.h>
#include <netdb.h>

#define MINARGS 3
#define USAGE "multi-lookup <inputFilePath> <outputFilePath>"
#define DNS_LENGTH 1025
#define INPUTFS "%1024s"
#define MAX_INPUT_FILES 10
#define MAX_RESOLVER_THREADS 10
#define MIN_RESOLVER_THREADS 2
#define REQUESTER_Q_SIZE 10
#define RESOLVER_Q_SIZE 100 

pthread_mutex_t resolverQLock;
pthread_mutex_t requesterQLock;
pthread_t *requesterPool_p = NULL;
pthread_t *resolverPool_p = NULL;
sem_t requesterSem;
sem_t resolverSem;
queue requesterQ;
queue resolverQ;
char outputFileName[DNS_LENGTH]; 

int readFile(char *fileName){
	int rc = 0; // for error handling
	FILE *file = fopen(fileName, "r");
	
	if(!file) {
		fprintf(stderr, "Error opening input file: %s", fileName);
		rc = -1;
	} else {
		char hostname[DNS_LENGTH];

		while(fscanf(file, INPUTFS, hostname) > 0) {
			int queueFull = 1;
			int rc = 0;

			while(queueFull){
				int queueFail = 0;

				// sleeping until queue not full
				if((queueFull = queue_is_full(&resolverQ)) == 1) {
					srand(time(NULL));
					usleep(rand() % 100);
					continue;
				}

				rc = pthread_mutex_lock(&resolverQLock);

				// error handling, adding to resolver queue
				if(rc != 0) {
					fprintf(stderr, "Failed to lock resolver queue: rc = %d <%s>. \n", rc, strerror(rc));
					rc = -1;
					break;
				}

				rc = queue_push(&resolverQ, hostname, strlen(hostname) + 1);
				if((rc == QUEUE_FAILURE) || (rc == QUEUE_SIZE)) {
					fprintf(stderr, "Adding to resolver queue failed.\n");
					queueFail = 1;
				}

				rc = pthread_mutex_unlock(&resolverQLock);
				if(rc != 0) {
					fprintf(stderr, "Failed to unlock resolver queue: rc = %d <%s>. \n", rc, strerror(rc));
					rc = -1;
					break;
				}

				if(queueFail) {
					break;
				}

				// wake resolver thread to process queue 
				if((rc = sem_post(&resolverSem)) < 0) {
					fprintf(stderr, "sem_post failed to wake resolver thread = %d  <%s> \n", errno, strerror(errno));
					break;
				}

			}

		if(rc < 0) {
			break;
		}	

		}

	fclose(file);

	}

	return rc;

}

void *requesterThread(void *threadNum){
	int rc = 0;
	int queue_rc;

	char fileNames[MAX_INPUT_FILES];

	while(1){
		sem_wait(&requesterSem);
		pthread_mutex_lock(&requesterQLock);
		queue_rc = queue_pop(&requesterQ, fileNames, MAX_INPUT_FILES);
		pthread_mutex_unlock(&requesterQLock);

		if (queue_rc == QUEUE_EMPTY){
			continue;
		}

		if (queue_rc == QUEUE_FAILURE || queue_rc == QUEUE_SIZE) {
			fprintf(stderr, "Queue pop failed\n");
			continue;
		}

		readFile(fileNames);
	}
	return NULL;
}

void *resolverThread(void *threadNum){
	int rc = 0;
	int queue_rc;

	char hostNames[DNS_LENGTH];
	char ipAddrs[DNS_LENGTH];
	FILE *outFile;

	while(1){
		sem_wait(&resolverSem);
		pthread_mutex_lock(&resolverQLock);
		queue_rc = queue_pop(&resolverQ, hostNames, MAX_INPUT_FILES);
		pthread_mutex_unlock(&resolverQLock);

		if (queue_rc == QUEUE_EMPTY){
			continue;
		}

		if (queue_rc == QUEUE_FAILURE || queue_rc == QUEUE_SIZE) {
			fprintf(stderr, "Queue pop failed\n");
			continue;
		}

		rc = dnslookup(hostNames, ipAddrs, DNS_LENGTH);

		//explore reason for util failure check?
		if ((ipAddrs[0] == '\0') || (rc == UTIL_FAILURE)) {
			ipAddrs[0] = ' ';
			ipAddrs[1] = '\0';
		}

		outFile = fopen(outputFileName, "a");

		fprintf(outFile, "%s, %s \n", hostNames, ipAddrs);
		fclose(outFile);
	}
	return NULL;
}

int doesFileExist(char *fileName) {
	int rc = 1;
	FILE *file = fopen(fileName, "r");

	if (!file) {
		rc = 0;
		return rc;
	} else {
		fclose(file);
	}
	return rc;
}

int checkOutputFilePath(char *outputFile) {
	int i;
	int len = strlen(outputFile);
	int pathBeforeFile;
	char path[DNS_LENGTH];

	for (i = len; i >= 0; i--) {
		if(outputFile[i] == '/'){
			pathBeforeFile = i - 1;
			break;
		}
	}

	if(pathBeforeFile == len) { // might not be necessary
		return 0;
	}

	memcpy(path, outputFile, pathBeforeFile);
	path[pathBeforeFile + 1] = '\0';
	return !doesFileExist(path);
}

int main(int argc, char* argv []){
	int i;
	int rc;
	int queue_rc;
	int cores = sysconf(_SC_NPROCESSORS_ONLN);

 	sem_init(&requesterSem, 0, 0); // look at man pages for sem_init
 	sem_init(&resolverSem, 0, 0);

 	queue_init(&requesterQ, REQUESTER_Q_SIZE, DNS_LENGTH);
 	queue_init(&resolverQ, RESOLVER_Q_SIZE, DNS_LENGTH);

	pthread_mutex_init(&requesterQLock, NULL); //man pages for why NULL
	pthread_mutex_init(&resolverQLock, NULL);

	requesterPool_p = (pthread_t *)malloc(size_of(pthread_t) * cores);
	resolverPool_p = (pthread_t *)malloc(size_of(pthread_t) * cores);
	
	for (i = 0; i < cores; i++) {
		pthread_create(&requesterPool_p[i], NULL, requesterThread, (void *)((long)i));
		pthread_create(&resolverPool_p[i], NULL, resolverThread, (void *)((long)i)); 
	}

	if(argc < MINARGS){
		fprintf(stderr, "Not enough arguments: %d\n", (argc - 1));
		fprintf(stderr, "Usage:\n %s %s\n", argv[0], USAGE);
		return EXIT_FAILURE;
    }

    strncpy(outputFileName, argv[argc-1], DNS_LENGTH); // figure out what line means

    if(checkOutputFilePath(outputFileName)){
    	fprintf(stderr, "Invalid output path for output file %s. \n", outputFileName);
    	return EXIT_FAILURE;
    }

    FILE *outFile = fopen(outputFileName, "w");
    fclose(outFile);

    for (i = 0; i < (argc - 1); i++) {
    	int queueFull = 1;

    	while(queueFull) {
    		int queueFail = 0;

    		if(doesFileExist(argv[i]) == 0) {
    			fprintf(stderr, "File %s does not exist, exiting.\n", argv[i]);
    			return EXIT_FAILURE;
    		}

    		if((queueFull = queue_is_full(&requesterQ)) == 1) {
    			srand(time(NULL));
				usleep(rand() % 100);
				continue;
    		}

    		rc = pthread_mutex_lock(&requesterQLock);

    		queue_rc = queue_push(&requesterQ, argv[i], strlen(argv[i]) + 1);

    		if ((queue_rc == QUEUE_FAILURE) || (queue_rc == QUEUE_SIZE)) {
    			fprintf(stderr, "Adding to requestor queue fail. \n");
    			queueFail = 1;
    		}

    		rc = pthread_mutex_unlock(&requesterQLock);

    		if (queueFail) {
    			rc = -1;
    			break;
    		}
    	}
    	if (rc < 0) {
    		break;
    	}
    }
    return EXIT_SUCCESS;
}



