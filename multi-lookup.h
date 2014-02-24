/* Multi-threaded DNS Name Resolution Engine that
 * resolves IP addresses to their DNS names.
 * 
 * Shannon Moore - CSCI 3753 PA 2 
 *
 */

#ifndef _MULTI_LOOKUP_H_
#define _MULTI_LOOKUP_H_

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

int dnslookup(const char* hostname, char* firstIPstr, int maxSize);
void *TaskCode(void *argument)


#endif