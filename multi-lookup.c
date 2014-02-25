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
#include "util.h"
#include "queue.h"
#include <unistd.h> //???????????
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

pthread_mutex_t resolverQLock;
pthread_mutex_t requesterQLock;
pthread_t *requesterPool_p = NULL;
pthread_t *resolverPool_p = NULL;
sem_t requestorSem;
sem_t resolverSem;
queue requestorQ;
queue resolverQ;
char outputFileName[DNS_LENGTH]; 

int readFile(char *fileName){
	int rc = 0; // for error handling
	FILE *file = fopen(fileName, "r");
	
	if(!file) {
		sprintf(errorstr, "Error opening input file: %s", fileName);
		perror(errorstr);
		rc = -1;
	} else {
		char hostname[DNS_LENGTH];

		while(fscanf(file, INPUTFS, hostname) > 0) {
			int queueFull = 1;
			int rc = 0;

			while(queueFull){
				int queueFail = 0;

				// sleeping until queue not full
				if(queueFull = queue_is_full(&resolverQ) == 1) {
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
					fprintf(strerror, "Adding to resolver queue failed.\n");
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

int main(int argc, char* argv []){
 	//fprintf(stderr, "something\n");


}



