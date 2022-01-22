#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <stdarg.h>

#define NUM_LETTER 26
#define THREAD_NUM 30

int threadsNum;
int doneClients;
int queue;
static int arrayA[NUM_LETTER];
static pthread_mutex_t lockA = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t lockB = PTHREAD_MUTEX_INITIALIZER;

void* client_thread(void* param) {
    struct msgB smsg;
	struct msgB rmsg;
    rmsg.t[0] = 'A';
    smsg.t[0] = 'A';
	smsg.t[1] = 'C';
	smsg.t[2] = 'K';
    int threadsNum = *((int*)param);
    if (msgrcv(queue, (void*)&rmsg, sizeof(rmsg), threadsNum, 0) == -1) {
		perror("Path Error.\n");
		exit(1);
	}

    int arrayB[NUM_LETTER];
    int nextI = 0;
    int countW = 0;
    char c;
	char stamp[THREAD_NUM];
    time_t timeCC;
	time(&timeCC);
	strcpy(stamp, ctime(&timeCC));
    stamp[strcspn(stamp, "\n")] = 0;
    printf("%s: Thread %d recieved %s from client %d\n", stamp, threadsNum-1, rmsg.t, threadsNum-1);
    FILE* input; 
	while (strcmp(rmsg.t, "END") != 0) { // when it's not finished
        // open and check if it's valid
		input = fopen(rmsg.t, "r");
		if (input == NULL) return NULL;
		c = fgetc(input);
		if (c - 'a' < 0) c += 32;
		arrayB[c - 'a']++; // increment the array

		while (true) {
			c = fgetc(input); // get the char
			if (feof(input)) break;
			if (nextI) {
				if (c - 'a' < 0) c += 32;
				arrayB[c - 'a']++; 
			}
			if (c == '\n') nextI = 1; // get the next line
			else nextI = 0;
		}

		fclose(input);
        // do in a sequence using mutex lock
        pthread_mutex_lock(&lockA);
	    for (int i = 0; i < NUM_LETTER; i++) {
		    arrayA[i] = arrayA[i] + arrayB[i];
	    }
	    pthread_mutex_unlock(&lockA);
		for (int i = 0; i < NUM_LETTER; i++) {
			arrayB[i] = 0;
		}
        // send the msg
		smsg.type = threadsNum + THREAD_NUM;
		msgsnd(queue, (void*)&smsg, sizeof(smsg), 0);
		time(&timeCC);
		strcpy(stamp, ctime(&timeCC));
    	stamp[strcspn(stamp, "\n")] = 0;   
        // print the ACK
    	printf("%s: ACK was sent by the thread %d for the path %s\n", stamp, threadsNum-1, rmsg.t);
		// receive the files
		msgrcv(queue, (void*)&rmsg, sizeof(rmsg), threadsNum, 0);
		time(&timeCC);
		strcpy(stamp, ctime(&timeCC));
    	stamp[strcspn(stamp, "\n")] = 0;
        // print the receive
    	printf("%s: Thread %d recieved %s from the client %d\n", stamp, threadsNum-1, rmsg.t, threadsNum-1);
	}

    // use mutex lock to do clients
    char temp[256];
	pthread_mutex_lock(&lockB);
	doneClients++;
	pthread_mutex_unlock(&lockB);
	while (doneClients < threadsNum) { //wait for the clients
    } 
	for (int i = 0; i < NUM_LETTER; i++) {
		sprintf(temp + strlen(temp), "%d#", arrayA[i]);
	} 
	strcpy(smsg.t, temp); 
	if(msgsnd(queue, (void*)&smsg, sizeof(smsg), 0) == -1) { //send the result
		return NULL;
	}
	time(&timeCC);
	strcpy(stamp, ctime(&timeCC));
    stamp[strcspn(stamp, "\n")] = 0;
    printf("%s: Thread %d sending final letter count to client %d\n", stamp, threadsNum-1, threadsNum-1);
	free(param);
	return 0;
}

int main(int argc, char* argv[]) {
	char buf[THREAD_NUM];
    time_t timeCC;
	time(&timeCC);
	strcpy(buf, ctime(&timeCC));
    buf[strcspn(buf, "\n")] = 0;
    printf("%s: Start the server.\n", buf);
	if (argc != 2) {
        printf("Incorrect Argument num.\n");
		return -1;
	}
	int countT = atoi(argv[1]);
	if (countT < 1 || countT > THREAD_NUM) {
		printf("Thread num is incorrect.\n");
		return -1;
	}

    // initialize variables
	key_t key = 123;
    threadsNum = countT;
	doneClients = 0;
	queue = msgget(key, 0666 | IPC_CREAT);
    msgctl(queue, IPC_RMID, NULL);
    queue = msgget(key, 0666 | IPC_CREAT);
	pthread_t tid[countT];
    // create and join the threads
	for (int i = 0; i < countT; i++) { // create threads to server
		int* clientNum = (int*)malloc(sizeof(*clientNum)); *clientNum = i + 1;
		pthread_create(&tid[i], NULL, client_thread, (void*)clientNum);
    }
	for (int i = 0; i < countT; i++) { // wait for threads to exit
		pthread_join(tid[i], NULL);
	}

	time(&timeCC);
	strcpy(buf, ctime(&timeCC));
    buf[strcspn(buf, "\n")] = 0;
    printf("%s: Server ends.\n", buf);
	return 0;
}