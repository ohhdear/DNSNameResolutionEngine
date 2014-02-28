/* Multi-threaded DNS Name Resolution Engine that
 * resolves IP addresses to their DNS names.
 * 
 * Shannon Moore - CSCI 3753 PA 2 
 *
 */

#ifndef _MULTI_LOOKUP_H_
#define _MULTI_LOOKUP_H_

int readFile(char *fileName);
void *requesterThread(void *threadNum);
void *resolverThread(void *threadNum);
int doesFileExist(char *fileName);
int checkOutputFilePath(char *outputFile);	

#endif