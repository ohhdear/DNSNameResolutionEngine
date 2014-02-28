/*
 * File: multi-lookup.c
 * Project: OS Programming Assignment 2
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include "util.c"
#include "queue.c"

#define MIN_ARGS 3
#define USAGE "./multi-lookup <inputFilePath> <inputFilePath> ... <outputFilePath>"
#define SBUFSIZE 1025
#define INPUTFS "%1024s"
#define MAX_INPUT_FILES 10
#define MAX_RESOLVER_THREADS 10
#define MIN_RESOLVER_THREADS 2
#define MAX_NAME_LENGTH 1025
#define MAX_IP_LENGTH INET6_ADDRSTRLEN
#define QUEUE_SIZE 1024

FILE* outFileName;
queue q;
int all_files_read;
//pthread_mutexattr_t attr;
pthread_mutex_t request_mutex = PTHREAD_MUTEX_INITIALIZER;

//pthread_mutexattr_t attr2;
pthread_mutex_t resolve_mutex = PTHREAD_MUTEX_INITIALIZER;

//pthread_mutexattr_t attr3;
pthread_mutex_t output_mutex = PTHREAD_MUTEX_INITIALIZER;


/* Function for Each Thread to Run */
void* request(void *file_name) {
    /* Setup Local Vars */
    FILE* inputFileName = NULL;
	char* hostname;
	char errorstr[SBUFSIZE]; 
	
	 
	/* Open Input File */
	inputFileName = fopen((char *)file_name, "r");
	if(!inputFileName) {
		sprintf(errorstr, "Error Opening Input File: %s", (char *)file_name);
		perror(errorstr);
	}
	
	// malloc hostname for the first time for storage
	hostname = (char*)malloc((MAX_NAME_LENGTH+1)*sizeof(char));
	
	/* Read Each File and Process*/
	while(fscanf(inputFileName, INPUTFS, hostname) > 0){
		
		/* Write each domain to the queue */
		pthread_mutex_lock(&request_mutex);
		
		if(queue_is_full(&q)) {
			pthread_mutex_unlock(&request_mutex); // unlock the queue so stuff can be taken out of it
			usleep(rand()%100);
			pthread_mutex_lock(&request_mutex); // relock the queue for pushing
		}
		
		
		if(queue_push(&q, hostname) == QUEUE_FAILURE) {
			fprintf(stderr, "ERROR: queue_push failed with value: %s\n", hostname);
		}
		pthread_mutex_unlock(&request_mutex);
		
		// create a new pointer for the mutex unlock process
		hostname = (char*)malloc((MAX_NAME_LENGTH+1)*sizeof(char));
	 }
	 
	 fclose(inputFileName);
    
    free(hostname); // free the local chunk we have leftover
    hostname=NULL;
    
    /* Exit, Returning NULL*/
    return NULL;
}

void* resolver() {	
	
	char *hostname;
    char firstIpStr[INET6_ADDRSTRLEN];
	int queue_is_not_empty = 0; // set queue to be empty unless we hear otherwise
	int continue_looping = 0; // don't loop by default
	
	pthread_mutex_lock(&request_mutex);
		queue_is_not_empty = !queue_is_empty(&q); // check to see if the queue is empty - must lock for this behavior
	pthread_mutex_unlock(&request_mutex);
	
	pthread_mutex_lock(&output_mutex);
		continue_looping = (!all_files_read)||(queue_is_not_empty); // all files are not read or the queue is not empty
	pthread_mutex_unlock(&output_mutex);
	
	while(continue_looping) {
		pthread_mutex_lock(&request_mutex);
		if((hostname = (char*)queue_pop(&q)) == NULL) { // pop off the hostname into the queue
			pthread_mutex_unlock(&request_mutex); // unlock the queue for others to use
		} else {	// if it didn't error out, if something actually did pop off the queue
			pthread_mutex_unlock(&request_mutex);
			
			if(dnslookup(hostname, firstIpStr, sizeof(firstIpStr)) == UTIL_FAILURE) {
				fprintf(stderr, "\ndnslookup error: %s\n", hostname);
				strncpy(firstIpStr, "", sizeof(firstIpStr));
			}
			
			pthread_mutex_lock(&resolve_mutex); 
				fprintf(outFileName, "%s,%s\n", hostname, firstIpStr);
			pthread_mutex_unlock(&resolve_mutex);
			
			free(hostname); // free memory from the queue if pop was successful
			hostname = NULL;
		}
			
		// check to see if the while loop should continue
		pthread_mutex_lock(&request_mutex);
			queue_is_not_empty = !queue_is_empty(&q); 
		pthread_mutex_unlock(&request_mutex);

		pthread_mutex_lock(&output_mutex);
			continue_looping = (!all_files_read)||(queue_is_not_empty);
		pthread_mutex_unlock(&output_mutex);
		
	}
	
	pthread_mutex_lock(&request_mutex); // lock the queue
	
	if(queue_is_empty(&q)) { // this is where we should end up after the while loop
		pthread_mutex_unlock(&request_mutex); 
		return NULL;
	} else { // this is where we error out if there's stuff in the queue - we shouldn't get here based on the while loop
		fprintf(stderr, "Queue is NOT empty - THIS IS BAD\n");
		pthread_mutex_unlock(&request_mutex); //unlock the queue for others to use
		return NULL;
	}
	
	// Fail safe unlock	
	pthread_mutex_unlock(&request_mutex); 
    return NULL;
}

int main(int argc, char* argv[]) {
    /* Local Vars */
    outFileName = NULL;
    all_files_read=0;

    int numInputFiles = argc-2; //subtract 1 for original call and another for resolve.txt
    int rc;
    long t;
    const int qSize = QUEUE_SIZE;
    
    pthread_t file_threads[numInputFiles];
	/* number of cores */
	int numCPU = sysconf(_SC_NPROCESSORS_ONLN);
    pthread_t resolver_threads[numCPU];

	/* Check Arguments */
	if(argc < MIN_ARGS) {
		fprintf(stderr, "Too few arguments: %d\n", (argc - 1));
		fprintf(stderr, "Usage:\n %s %s\n", argv[0], USAGE);
		return EXIT_FAILURE;
	}
	
    if(argc > MAX_INPUT_FILES+1) {
		fprintf(stderr, "Too many arguments: %d\n", (argc - 1));
		fprintf(stderr, "Usage:\n %s %s\n", argv[0], USAGE);
		return EXIT_FAILURE;
    }

    /* Open Output File */
    outFileName = fopen(argv[(argc-1)], "w");
    if(!outFileName) {
		perror("Error Opening Output File\n");
		return EXIT_FAILURE;
    }

    /* Initialize Queue */
    if(queue_init(&q, qSize) == QUEUE_FAILURE) {
		fprintf(stderr, "ERROR: queue_init failed!\n");
		return EXIT_FAILURE; // if queue does not initialize properly, can't run the program
    }
    
    
    /* Spawn requester pool threads */
    for(t=0; t<numInputFiles; t++) { 
		// create a thread to read input files
		rc = pthread_create(&(file_threads[t]), NULL, request, (void *)argv[t+1]); // pass input filename and output file
		if (rc) { 
			// couldn't create thread successfully
			fprintf(stderr, "ERROR: return code from pthread_create() is %d\n", rc);
			exit(EXIT_FAILURE);
		}
	}
	


	/* spawn resolver pool */
	for(t=0; t<numCPU; t++) { 
		//create a resolver thread 
		rc = pthread_create(&(resolver_threads[t]), NULL, resolver, NULL); // pass input filename and output file
		if (rc) { 
			//couldn't create thread successfully
			fprintf(stderr, "ERROR: return code from pthread_create() is %d\n", rc);
			exit(EXIT_FAILURE);
		}
	}
	

    /* Wait for All Threads to Finish */
    for(t=0;t<numInputFiles;t++) {
		pthread_join(file_threads[t], NULL);
    }

    // the file threads have completed - change all_files_read to TRUE
    pthread_mutex_lock(&output_mutex);
		all_files_read=1; //all files are not read or the queue is not empty
	pthread_mutex_unlock(&output_mutex);
    
    for(t=0;t<numCPU;t++) {
		pthread_join(resolver_threads[t], NULL);
    }
    
    printf("\nAll of the threads were completed!\n");
	fclose(outFileName);
	
    queue_cleanup(&q);

	pthread_mutex_destroy(&request_mutex);
	pthread_mutex_destroy(&resolve_mutex);
	pthread_mutex_destroy(&output_mutex);

    return EXIT_SUCCESS;
}
