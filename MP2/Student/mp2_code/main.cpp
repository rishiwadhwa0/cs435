#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "monitor_neighbors.h"


void listenForNeighbors();
void* announceToNeighbors(void* unusedParam);


int globalMyID = 0;
//last time you heard from each node. TODO: you will want to monitor this
//in order to realize when a neighbor has gotten cut off from you.
struct timeval globalLastHeartbeat[256];

//our all-purpose UDP socket, to be bound to 10.1.1.globalMyID, port 7777
int globalSocketUDP;
//pre-filled for sending to 10.1.1.0 - 255, port 7777
struct sockaddr_in globalNodeAddrs[256];

//rishi
short int MAX_NODES = 256;
FILE *theLogFile;
int graph[256][256];
int myNeighborGivenCosts[256];
int givenIdsAndCosts[256];
int seqNums[256];
//rishi

 
int main(int argc, char** argv)
{
	if(argc != 4)
	{
		fprintf(stderr, "Usage: %s mynodeid initialcostsfile logfile\n\n", argv[0]);
		exit(1);
	}
	
	
	//initialization: get this process's node ID, record what time it is, 
	//and set up our sockaddr_in's for sending to the other nodes.
	globalMyID = atoi(argv[1]);
	int i;
	for(i=0;i<256;i++)
	{
		gettimeofday(&globalLastHeartbeat[i], 0);
		
		char tempaddr[100];
		sprintf(tempaddr, "10.1.1.%d", i);
		memset(&globalNodeAddrs[i], 0, sizeof(globalNodeAddrs[i]));
		globalNodeAddrs[i].sin_family = AF_INET;
		globalNodeAddrs[i].sin_port = htons(7777);
		inet_pton(AF_INET, tempaddr, &globalNodeAddrs[i].sin_addr);
	}
	
	
	//TODO: read and parse initial costs file. default to cost 1 if no entry for a node. file may be empty.
	
	//rishi
	// initial costs file
	for (int i = 0; i < MAX_NODES; i++) {
		givenIdsAndCosts[i] = 1;
	}
	FILE *initialCostsFile = fopen(argv[2], "r");
	char *line = NULL; size_t len = 0;
	short int id;
	int cost;
	while (getline(&line, &len, initialCostsFile) != -1) {
		sscanf(line, "%hd %d", &id, &cost);
		givenIdsAndCosts[id] = cost;
	}
	fclose(initialCostsFile);
	
	// create graph (i.e. adjacency matrix)
	for (int i = 0; i < MAX_NODES; i++) {
		for (int j = 0; j < MAX_NODES; j++) {
			graph[i][j] = -1;
		}
	}

	// open log file for writing
	theLogFile = fopen(argv[3], "w");

	//initialize seq #s to -1
	for (int i = 0; i < MAX_NODES; i++) {
		seqNums[i] = -1;
	}
	//rishi
	
	//socket() and bind() our socket. We will do all sendto()ing and recvfrom()ing on this one.
	if((globalSocketUDP=socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		perror("socket");
		exit(1);
	}
	char myAddr[100];
	struct sockaddr_in bindAddr;
	sprintf(myAddr, "10.1.1.%d", globalMyID);	
	memset(&bindAddr, 0, sizeof(bindAddr));
	bindAddr.sin_family = AF_INET;
	bindAddr.sin_port = htons(7777);
	inet_pton(AF_INET, myAddr, &bindAddr.sin_addr);
	if(bind(globalSocketUDP, (struct sockaddr*)&bindAddr, sizeof(struct sockaddr_in)) < 0)
	{
		perror("bind");
		close(globalSocketUDP);
		exit(1);
	}
	
	//start threads... feel free to add your own, and to remove the provided ones.
	pthread_t announcerThread, broadcastThread;
	pthread_create(&announcerThread, 0, announceToNeighbors, (void*)0);

	//rishi
	pthread_create(&broadcastThread, 0, broadcastToNeighbors, (void*)0);
	//rishi

	//good luck, have fun!
	listenForNeighbors();
}
