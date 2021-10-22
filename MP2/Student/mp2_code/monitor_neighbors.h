#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>

//rishi
#include <sys/time.h>
//rishi


extern int globalMyID;
//last time you heard from each node. TODO: you will want to monitor this
//in order to realize when a neighbor has gotten cut off from you.
extern struct timeval globalLastHeartbeat[256];

//our all-purpose UDP socket, to be bound to 10.1.1.globalMyID, port 7777
extern int globalSocketUDP;
//pre-filled for sending to 10.1.1.0 - 255, port 7777
extern struct sockaddr_in globalNodeAddrs[256];


//rishi
extern short int MAX_NODES;
extern FILE *theLogFile;
extern int graph[256][256];
//rishi


//rishi
short int getNetOrderShort(unsigned char *buf) {
	short int number;
	memcpy(&number, buf, 2);
	return ntohs(number);
}

void printNeighbors() {
	// https://stackoverflow.com/questions/2741784/printing-a-2d-array-in-c
    for (int i = 0; i < MAX_NODES; i++) {
		bool firstTime = true;
        for (int j = 0; j < MAX_NODES; j++) {
            if (graph[i][j] >= 0) {
				if (firstTime) { printf("Node %d: ", globalMyID); firstTime = false; }
				printf("%d ", graph[i][j]);
			}
        }
        if (!firstTime) { printf("\n"); }
    }
}
//rishi


//Yes, this is terrible. It's also terrible that, in Linux, a socket
//can't receive broadcast packets unless it's bound to INADDR_ANY,
//which we can't do in this assignment.
void hackyBroadcast(const char* buf, int length)
{
	int i;
	for(i=0;i<256;i++)
		if(i != globalMyID) //(although with a real broadcast you would also get the packet yourself)
			sendto(globalSocketUDP, buf, length, 0,
				  (struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));
}

void* announceToNeighbors(void* unusedParam)
{
	struct timespec sleepFor;
	sleepFor.tv_sec = 0;
	sleepFor.tv_nsec = 300 * 1000 * 1000; //300 ms
	while(1)
	{
		hackyBroadcast("HEREIAM", 7);
		nanosleep(&sleepFor, 0);
	}
}

void listenForNeighbors()
{
	char fromAddr[100];
	struct sockaddr_in theirAddr;
	socklen_t theirAddrLen;
	unsigned char recvBuf[1000];

	int bytesRecvd;
	while(1)
	{
		theirAddrLen = sizeof(theirAddr);
		if ((bytesRecvd = recvfrom(globalSocketUDP, recvBuf, 1000 , 0, 
					(struct sockaddr*)&theirAddr, &theirAddrLen)) == -1)
		{
			perror("connectivity listener: recvfrom failed");
			exit(1);
		}
		
		inet_ntop(AF_INET, &theirAddr.sin_addr, fromAddr, 100);
		
		short int heardFrom = -1;
		if(strstr(fromAddr, "10.1.1."))
		{
			heardFrom = atoi(
					strchr(strchr(strchr(fromAddr,'.')+1,'.')+1,'.')+1);
			
			//TODO: this node can consider heardFrom to be directly connected to it; do any such logic now.
			
			//rishi
			// node id from sender of this message
			// if from neighbor, heardFrom short int will be positive
			// otherwise the heardFrom will remain -1

			// also used to determine live nodes 
			//rishi

			//record that we heard from heardFrom just now.
			gettimeofday(&globalLastHeartbeat[heardFrom], 0);
		}
		
		//Is it a packet from the manager? (see mp2 specification for more details)
		//send format: 'send'<4 ASCII bytes>, destID<net order 2 byte signed>, <some ASCII message>
		if(!strncmp((const char*) recvBuf, "send", 4))
		{
			//TODO send the requested message to the requested destination node
			// ...

			//rishi
			printNeighbors();
			//rishi

		}
		//'cost'<4 ASCII bytes>, destID<net order 2 byte signed> newCost<net order 4 byte signed>
		else if(!strncmp((const char*) recvBuf, "cost", 4))
		{
			//TODO record the cost change (remember, the link might currently be down! in that case,
			//this is the new cost you should treat it as having once it comes back up.)
			// ...
		}
		
		//TODO now check for the various types of packets you use in your own protocol
		//else if(!strncmp(recvBuf, "your other message types", ))
		// ... 
		//rishi
		// if you discover a new link, you broadcast this info to all of your neighbors
		// as well as link failure
		// LSA21\7,3\10,4 
		// 2 -7 3
		// \10 4
		// source node
		// seq number
		// neighbors and edges and costs
		//rishi
	}
	//(should never reach here)
	close(globalSocketUDP);
}

