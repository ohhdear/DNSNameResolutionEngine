/*
 * File: queue.c
 * Author: Chris Wailes <chris.wailes@gmail.com>
 * Author: Wei-Te Chen <weite.chen@colorado.edu>
 * Author: Andy Sayler <andy.sayler@gmail.com>
 * Project: CSCI 3753 Programming Assignment 2
 * Create Date: 2010/02/12
 * Modify Date: 2011/02/04
 * Modify Date: 2012/02/01
 * Description:
 *  This file contains an implementation of a simple FIFO queue.
 *  
 */

#include <stdlib.h>
#include <string.h>

#include "queue.h"

int queue_init( queue* q, int size, int payloadSize )
{
    int i;

    /* user specified size or default */
    if( size > 0 )
    {
        q->maxSize = size;
    }
    else
    {
        q->maxSize = QUEUEMAXSIZE;
    }

    q->maxPayloadSize = payloadSize;

    /* malloc array */
    q->array = malloc(sizeof(queue_node) * (q->maxSize));
    if( !(q->array) )
    {   
    perror("Error on queue Malloc");
    return QUEUE_FAILURE;
    }

    /* Set to NULL */
    for( i = 0; i < q->maxSize; ++i )
    {
        /* Allocate memory for payload. */
    q->array[i].payload = malloc( payloadSize );

        if( !(q->array[i].payload) )
        {
            perror( "Failed to allocate payload" );
            return QUEUE_FAILURE;
        }

        q->array[i].queued = 0;
    }

    /* setup circular buffer values */
    q->front = 0;
    q->rear  = 0;

    return q->maxSize;
}

int queue_is_empty(queue* q)
{
    if((q->front == q->rear) && (q->array[q->front].queued == 0))
    {
    return 1;
    }
    else
    {
    return 0;
    }
}

int queue_is_full(queue* q)
{
    if((q->front == q->rear) && (q->array[q->front].queued != 0))
    {
    return 1;
    }
    else
    {
    return 0;
    }
}

int queue_pop(queue* q, void *ret_payload, int payloadSize )
{
    if(queue_is_empty(q))
    {
    return QUEUE_EMPTY;
    }

    if( payloadSize < q->array[q->front].size )
    {
        return QUEUE_SIZE;
    }

    memcpy( ret_payload, q->array[q->front].payload, q->array[q->front].size );
    //ret_payload = q->array[q->front].payload;
    //q->array[q->front].payload = NULL;
    q->array[q->front].queued = 0;
    q->front = ((q->front + 1) % q->maxSize);

    return QUEUE_SUCCESS;
}

int queue_push(queue* q, void* new_payload, int size)
{
    if(queue_is_full(q))
    {
    return QUEUE_FULL;
    }

    if( size > q->maxPayloadSize )
    {
        return QUEUE_SIZE;
    }

    memcpy( q->array[q->rear].payload, new_payload, size );

    q->array[q->rear].size = size;

    //q->array[q->rear].payload = new_payload;
    q->array[q->rear].queued = 1;

    q->rear = ((q->rear+1) % q->maxSize);

    return QUEUE_SUCCESS;
}

#if 0
void queue_cleanup(queue* q)
{
    while(!queue_is_empty(q))
    {
    queue_pop(q);
    }

    free(q->array);
}
#endif
