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
#include <set>
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
int linkFailureMin = 2000; //milliseconds
int linkSuccessMax = 1000; //milliseconds
int seqNum = 0;
extern int seqNums[256];
std::set<int> neighbors;
//rishi


//rishi
short int getNetOrderShort(unsigned char *buf) {
	short int number;
	memcpy(&number, buf, 2);
	return ntohs(number);
}

void printGraph() {
	fprintf(stderr, "\nGRAPH START\n");
    for (short int i = 0; i < MAX_NODES; i++) {
		bool firstTime = true;
        for (short int j = 0; j < MAX_NODES; j++) {
			int cost = graph[i][j];
            if (cost >= 0) {
				if (firstTime) { fprintf(stderr, "Node %hd: ", i); firstTime = false; }
				fprintf(stderr, "<%hd, cost=%d> ", j, cost);
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

void sendBroadcast() {
	char buf[1000];
	memset(buf, 0, sizeof(buf));
	// int bufCounter = 0;
	// bufCounter += sprintf(buf, "LSA%hd,%d", globalMyID, seqNum);
	// for (short int i = 0; i < MAX_NODES; i++) {
	// 	int cost = graph[globalMyID][i];
	// 	if (cost >= 0) {
	// 		bufCounter += sprintf(buf+bufCounter, "/%hd,%d", i, cost);
	// 	}
	// }
	int bufCounter = 0;
	memcpy(buf+bufCounter, "LSA", 3); bufCounter += 3;
	memcpy(buf+bufCounter, &globalMyID, sizeof(short int)); bufCounter += sizeof(short int);
	memcpy(buf+bufCounter, &seqNum, sizeof(int)); bufCounter += sizeof(int);
	for (short int i = 0; i < MAX_NODES; i++) {
		int cost = graph[globalMyID][i];
		if (cost >= 0) {
			memcpy(buf+bufCounter, &i, sizeof(short int)); bufCounter += sizeof(short int);
			memcpy(buf+bufCounter, &cost, sizeof(int)); bufCounter += sizeof(int);
		}
	}

	for (int i = 0; i < MAX_NODES; i++) {
		if (graph[globalMyID][i] >= 0) {
			sendto(globalSocketUDP, buf, bufCounter, 0,
					(struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));
		}
	}
	
	seqNum++;
}

void* broadcastToNeighbors(void* unusedParam) {
	struct timespec sleepFor;
	sleepFor.tv_sec = 2;
	sleepFor.tv_nsec = 0;
	nanosleep(&sleepFor, 0);
	while(1) {
		struct timeval now;
		bool neighborsChanged = false;
		for (short int i = 0; i < MAX_NODES; i++) {
			gettimeofday(&now, 0);
			double time_diff = calcTimeDiff(globalLastHeartbeat[i], now);
			if (graph[globalMyID][i] >= 0 && time_diff >= linkFailureMin) {
				graph[globalMyID][i] = -1;
				graph[i][globalMyID] = -1;
				neighbors.erase(i);
				neighborsChanged = true;
				fprintf(stderr, "LOSE CONNECTION WITH %hd\n", i);
			} else if (graph[globalMyID][i] == -1 && time_diff < linkSuccessMax) {
				graph[globalMyID][i] = givenIdsAndCosts[i];
				graph[i][globalMyID] = givenIdsAndCosts[i];
				neighbors.insert(i);
				neighborsChanged = true;
				fprintf(stderr, "FORM CONNECTION WITH %hd\n", i);
			} else if (graph[globalMyID][i] >= 0 && neighbors.count(i) == 0) {
				neighbors.insert(i);
				neighborsChanged = true;
				fprintf(stderr, "RECOGNIZE CONNECTION WITH %hd\n", i);
			}
		}
		if (neighborsChanged) { 
			printGraph();
			sendBroadcast();
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
		}
		
		//Is it a packet from the manager? (see mp2 specification for more details)
		//send format: 'send'<4 ASCII bytes>, destID<net order 2 byte signed>, <some ASCII message>
		if(!strncmp((const char*) recvBuf, "send", 4))
		{
			//TODO send the requested message to the requested destination node
			// ...

			//rishi
			printGraph();
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
		else if(!strncmp((const char*) recvBuf, "LSA", 3)) {
			int bufCounter = 3;
			
			short int src_id;
			int seq_num;
			memcpy(&src_id, recvBuf+bufCounter, sizeof(short int)); bufCounter += sizeof(short int);
			memcpy(&seq_num, recvBuf+bufCounter, sizeof(int)); bufCounter += sizeof(int);
			fprintf(stderr, "%hd: heardFrom=%hd, src_id=%hd, seq_id=%d ", globalMyID, heardFrom, src_id, seq_num);

			if (seq_num > seqNums[src_id]) {
				seqNums[src_id] = seq_num;

				int copy_graph[256][256];
				memcpy(&copy_graph, &graph, sizeof(graph));
				
				for (int i = 0; i < MAX_NODES; i++) {
					graph[src_id][i] = -1;
					graph[i][src_id] = -1;
				}
				
				while (bufCounter < bytesRecvd) {
					short int neighbor_id;
					int cost;
					memcpy(&neighbor_id, recvBuf+bufCounter, sizeof(short int)); bufCounter += sizeof(short int);
					memcpy(&cost, recvBuf+bufCounter, sizeof(int)); bufCounter += sizeof(int);
					graph[src_id][neighbor_id] = cost;
					graph[neighbor_id][src_id] = cost;
					fprintf(stderr, "/ %hd,%d ", neighbor_id, cost);
				}
				fprintf(stderr, "\n");

				for (short int i = 0; i < MAX_NODES; i++) {
					if (i != globalMyID && i != heardFrom) {
						sendto(globalSocketUDP, recvBuf, bytesRecvd, 0,
							(struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));
					}
				}

				if (memcmp(&copy_graph, &graph, sizeof(graph)) != 0) { sendBroadcast(); }

			} else {
				fprintf(stderr, " [DISCARD]\n");
			}
		}

		memset(recvBuf, 0, sizeof(recvBuf));
		//rishi
	}
	//(should never reach here)
	close(globalSocketUDP);
}

