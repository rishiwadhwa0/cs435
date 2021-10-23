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
extern int givenIdsAndCosts[256];
int linkFailureMax = 2000; //milliseconds
int lostSeqNum = 0;
int formSeqNum = 0;
extern int lostSeqs[256];
extern int formSeqs[256];
//rishi


//rishi
short int getNetOrderShort(unsigned char *buf) {
	short int number;
	memcpy(&number, buf, 2);
	return ntohs(number);
}

void printGraph() {
	fprintf(stderr, "\nGRAPH START\n");
    for (int i = 0; i < MAX_NODES; i++) {
		bool firstTime = true;
        for (int j = 0; j < MAX_NODES; j++) {
            if (graph[i][j] >= 0) {
				if (firstTime) { fprintf(stderr, "Node %d: ", globalMyID); firstTime = false; }
				fprintf(stderr, "(%d, cost=%d) ", j, graph[i][j]);
			}
        }
        if (!firstTime) { fprintf(stderr, "\n"); }
    }
	fprintf(stderr, "GRAPH END\n\n");
}

double calcTimeDiff(struct timeval x, struct timeval y) {
	//https://www.binarytides.com/get-time-difference-in-microtime-in-c/
	double x_ms , y_ms , diff;
	x_ms = (double)x.tv_sec*1000000 + (double)x.tv_usec;
	y_ms = (double)y.tv_sec*1000000 + (double)y.tv_usec;
	diff = (double)y_ms - (double)x_ms;
	return diff / 1000; // return unit is milliseconds
}

void broadcastLost(short int neighbord_id) {
	char buf[15];
	sprintf(buf, "lost/%hd/%d/%hd", globalMyID, lostSeqNum, neighbord_id);
	fprintf(stderr, "%s\n", buf);
	// sendto(globalSocketUDP, buf, sizeof(buf), 0,
	// 			  (struct sockaddr*)&globalNodeAddrs[neighbord_id], sizeof(globalNodeAddrs[neighbord_id]));
	lostSeqNum++;
}

void broadcastForm(short int neighbord_id, int cost) {
	char buf[20];
	sprintf(buf, "form/%hd/%d/%hd/%d", globalMyID, formSeqNum, neighbord_id, cost);
	sendto(globalSocketUDP, buf, sizeof(buf), 0,
				  (struct sockaddr*)&globalNodeAddrs[neighbord_id], sizeof(globalNodeAddrs[neighbord_id]));
	formSeqNum++;
}

void* broadcastIfLinkFailure(void* unusedParam) {
	struct timespec sleepFor;
	sleepFor.tv_sec = 0;
	sleepFor.tv_nsec = 1000 * 1000 * 1000; //1000 ms
	while(1) {
		bool linkFailure = false;
		struct timeval now;
		for (short int i = 0; i < MAX_NODES; i++) {
			if (graph[globalMyID][i] >= 0) {
				gettimeofday(&now, 0);
				double time_diff = calcTimeDiff(globalLastHeartbeat[i], now);
				if (time_diff > linkFailureMax) {
					graph[globalMyID][i] = -1;
					linkFailure = true;
					broadcastLost(i);
				}
			}
		}
		nanosleep(&sleepFor, 0);
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
		
		short int heardFrom = -1; // node id from sender of this message OR -1 if manager
		if(strstr(fromAddr, "10.1.1."))
		{
			heardFrom = atoi(
					strchr(strchr(strchr(fromAddr,'.')+1,'.')+1,'.')+1);
			
			//TODO: this node can consider heardFrom to be directly connected to it; do any such logic now.

			//record that we heard from heardFrom just now.
			gettimeofday(&globalLastHeartbeat[heardFrom], 0);

			//rishi
			if (graph[globalMyID][heardFrom] == -1) {
				graph[globalMyID][heardFrom] = givenIdsAndCosts[heardFrom];
				broadcastForm(heardFrom, graph[globalMyID][heardFrom]);
			}
			//rishi
		}
		
		//Is it a packet from the manager? (see mp2 specification for more details)
		//send format: 'send'<4 ASCII bytes>, destID<net order 2 byte signed>, <some ASCII message>
		if(!strncmp((const char*) recvBuf, "send", 4))
		{
			//TODO send the requested message to the requested destination node
			// ...

			//rishi
			fprintf(stderr, "Received ping from manager.\n");
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
		else if(!strncmp((const char*) recvBuf, "form", 4)) {
			fprintf(stderr, "Received LSA: %s\n", recvBuf);
		}

		memset(recvBuf, 0, sizeof(recvBuf));
		//rishi
	}
	//(should never reach here)
	close(globalSocketUDP);
}

