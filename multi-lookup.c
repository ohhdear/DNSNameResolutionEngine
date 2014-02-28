/*
 * File: multi-lookup.c
 * Author: Shannon Moore
 * Original Creators: Andy Sayler
 * Project: OS Programming Assignment 2
 * Citation: Ryan Roden
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

FILE* outputfp;
queue q;
int all_files_read;
//pthread_mutexattr_t attr;
pthread_mutex_t request_mutex = PTHREAD_MUTEX_INITIALIZER;

//pthread_mutexattr_t attr2;
pthread_mutex_t resolve_mutex = PTHREAD_MUTEX_INITIALIZER;

//pthread_mutexattr_t attr3;
pthread_mutex_t output_mutex = PTHREAD_MUTEX_INITIALIZER;


/* Function for Each Thread to Run */
void* request(void *file_name)
{
//	(char *)file_name;
    /* Setup Local Vars */
    FILE* inputfp = NULL;
	char* hostname;
	char errorstr[SBUFSIZE]; 
	
	 
	/* Open Input File */
	inputfp = fopen((char *)file_name, "r");
	if(!inputfp)
	{
		sprintf(errorstr, "Error Opening Input File: %s", (char *)file_name);
		perror(errorstr);
		//fprintf(stderr, "Error Opening Input File: %s", file_name);
	}
	
	//malloc hostname for the first time for storage
	hostname = (char*)malloc((MAX_NAME_LENGTH+1)*sizeof(char));
	
	/* Read Each File and Process*/
	while(fscanf(inputfp, INPUTFS, hostname) > 0){
		
		/* Write each domain to the queue */
		pthread_mutex_lock(&request_mutex); //lock the queue
		
		//If a thread tries to write to the queue but finds that it is full, it should sleep for a random period of time between 0 and 100 microseconds
		if(queue_is_full(&q))
		{
			pthread_mutex_unlock(&request_mutex); //unlock the queue so stuff can be taken out of it
			/* Sleep for 0 to 100 uSeconds */
			usleep(rand()%100);
			pthread_mutex_lock(&request_mutex); //relock the queue for pushing
		}
		
		
		if(queue_push(&q, hostname) == QUEUE_FAILURE) //push hostname onto the queue
		{
			fprintf(stderr, "ERROR: queue_push failed with value: %s\n", hostname);
		}
		pthread_mutex_unlock(&request_mutex); //unlock the queue
		
		//create a new pointer for the mutex unlock process
		hostname = (char*)malloc((MAX_NAME_LENGTH+1)*sizeof(char));
	 }
	 
	 //after we're done reading the file, close it
	 fclose(inputfp);
    
    free(hostname); //free the local chunk we have leftover
    hostname=NULL;
    
    /* Exit, Returning NULL*/
    //pthread_exit(NULL);
    return NULL;
}

void* resolver()//(void* params)
{	
	char *hostname;
    char firstipstr[INET6_ADDRSTRLEN];
	int queue_is_not_empty = 0; //set queue to be empty unless we hear otherwise
	int continue_looping = 0; //don't loop by default
	
	pthread_mutex_lock(&request_mutex);
		queue_is_not_empty = !queue_is_empty(&q); //check to see if the queue is empty - must lock for this behavior
	pthread_mutex_unlock(&request_mutex);
	
	pthread_mutex_lock(&output_mutex);
		continue_looping = (!all_files_read)||(queue_is_not_empty); //all files are not read or the queue is not empty
	pthread_mutex_unlock(&output_mutex);
	
	while(continue_looping)
	{
		pthread_mutex_lock(&request_mutex);
		if((hostname = (char*)queue_pop(&q)) == NULL) //pop off the hostname into the queue
		{ //if it errored out
			//fprintf(stderr, "ERROR: queue_pop failed with value: %s\n", hostname);
			pthread_mutex_unlock(&request_mutex); //unlock the queue for others to use
		}
		else
		{	//if it didn't error out, if something actually did pop off the queue
			pthread_mutex_unlock(&request_mutex);
			
			// Lookup hostname and get IP string 
			if(dnslookup(hostname, firstipstr, sizeof(firstipstr)) == UTIL_FAILURE)
			{
				fprintf(stderr, "dnslookup error: %s\n", hostname);
				strncpy(firstipstr, "", sizeof(firstipstr)); //copy a NULL value to the IP string
			}
			
			//WRITE RESULT TO THE OUTPUT FILE
			pthread_mutex_lock(&resolve_mutex); //lock the output file
				fprintf(outputfp, "%s,%s\n", hostname, firstipstr);
			pthread_mutex_unlock(&resolve_mutex); //unlock the output file
			
			free(hostname); //free this chunk of memory from the queue if the pop was successful
			hostname = NULL;
		}
			
		//check to see if the while loop should continue
		pthread_mutex_lock(&request_mutex);
			queue_is_not_empty = !queue_is_empty(&q); //check to see if the queue is empty - must lock for this behavior
		pthread_mutex_unlock(&request_mutex);
		pthread_mutex_lock(&output_mutex);
			continue_looping = (!all_files_read)||(queue_is_not_empty); //all files are not read or the queue is not empty
		pthread_mutex_unlock(&output_mutex);
		
	}
	
	pthread_mutex_lock(&request_mutex); //lock the queue
	if(queue_is_empty(&q)) //this is where we should end up after the while loop
	{
		//fprintf(stderr, "Queue is empty\n");
		pthread_mutex_unlock(&request_mutex); //unlock the queue for others to use
		//pthread_exit(NULL); //Exit, Returning NULL
		return NULL;
	}
	else //this is where we error out if there's stuff in the queue - we shouldn't get here based on the while loop
	{
		fprintf(stderr, "Queue is NOT empty - THIS IS A PROBLEM\n");
		pthread_mutex_unlock(&request_mutex); //unlock the queue for others to use
		//pthread_exit(EXIT_FAILURE); //Exit, RETURN AN ERROR CODE
		return NULL;//EXIT_FAILURE;
	}
	
	//shouldn't arrive here, just in case	
	pthread_mutex_unlock(&request_mutex); //unlock the queue for others to use
	/* Exit, Returning NULL*/ 
    //pthread_exit(NULL);
    return NULL;
}

int main(int argc, char* argv[])
{
    /* Local Vars */
    outputfp = NULL;
    int num_input_files = argc-2; //subtract 1 for the original call and another for resolve.txt
	const int qSize = QUEUE_SIZE;
    int rc;
    long t;
    all_files_read=0;
    
    pthread_t file_threads[num_input_files];
	/* number of cores */
	int numCPU = sysconf(_SC_NPROCESSORS_ONLN);
    pthread_t resolver_threads[numCPU];
/*
	pthread_mutexattr_settype(&attr, NULL);
	pthread_mutex_init(&request_mutex, &attr);

	pthread_mutexattr_settype(&attr2, NULL);
	pthread_mutex_init(&resolve_mutex, &attr2);
	
	pthread_mutexattr_settype(&attr3, NULL);
	pthread_mutex_init(&output_mutex, &attr3);
*/
	/* Check Arguments */
	if(argc < MIN_ARGS){
		fprintf(stderr, "Too few arguments: %d\n", (argc - 1));
		fprintf(stderr, "Usage:\n %s %s\n", argv[0], USAGE);
		return EXIT_FAILURE;
	}
	
    if(argc > MAX_INPUT_FILES+1){
		fprintf(stderr, "Too many arguments: %d\n", (argc - 1));
		fprintf(stderr, "Usage:\n %s %s\n", argv[0], USAGE);
		return EXIT_FAILURE;
    }

    /* Open Output File */
    outputfp = fopen(argv[(argc-1)], "w");
    if(!outputfp){
		perror("Error Opening Output File\n");
		return EXIT_FAILURE;
    }

    /* Initialize Queue */
    if(queue_init(&q, qSize) == QUEUE_FAILURE)
    {
		fprintf(stderr, "ERROR: queue_init failed!\n");
		return EXIT_FAILURE; //if the queue didn't initialize properly, we can't run the rest of the program
    }
    
    
    /* Spawn requester pool threads */
    for(t=0; t<num_input_files; t++)
    { 
		//create a thread to read input files
		rc = pthread_create(&(file_threads[t]), NULL, request, (void *)argv[t+1]); //pass it the input filename and the output file
		if (rc)
		{ //couldn't create thread successfully
			fprintf(stderr, "ERROR: return code from pthread_create() is %d\n", rc);
			exit(EXIT_FAILURE);
		}
	}
	


	/* spawn resolver pool */
	for(t=0; t<numCPU; t++)
	{ 
		//create a resolver thread 
		rc = pthread_create(&(resolver_threads[t]), NULL, resolver, NULL); //pass it the input filename and the output file
		if (rc)
		{ //couldn't create thread successfully
			fprintf(stderr, "ERROR: return code from pthread_create() is %d\n", rc);
			exit(EXIT_FAILURE);
		}
	}
	

    /* Wait for All Threads to Finish */
    for(t=0;t<num_input_files;t++)
    {
		pthread_join(file_threads[t], NULL);
    }
    //the file threads have completed - change all_files_read to TRUE
    pthread_mutex_lock(&output_mutex);
		all_files_read=1; //all files are not read or the queue is not empty
	pthread_mutex_unlock(&output_mutex);
    
    for(t=0;t<numCPU;t++)
    {
		pthread_join(resolver_threads[t], NULL);
    }
    
    printf("All of the threads were completed!\n");
	fclose(outputfp); //close the output file when all the threads are done with it
	
    queue_cleanup(&q); //cleanup queue

	//clean up locks
	pthread_mutex_destroy(&request_mutex);
	pthread_mutex_destroy(&resolve_mutex);
	pthread_mutex_destroy(&output_mutex);

    return EXIT_SUCCESS;
}
