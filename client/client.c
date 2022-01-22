#include "client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <unistd.h>

#define THREAD_NUM 30
static int fileNum;
static char** fpath;

static void traversepath(char* path) {
    DIR* adir;
    char arrayDir[256][256];
    // checks and open the directory
    if ((adir = opendir(path)) == NULL) return;
    int dirInt = 0;
    int pe = 0;
    struct dirent* pointD;
    while ((pointD = readdir(adir)) != NULL) {
        if (pe <= 1) pe++;
        else if (pointD->d_type == 4) {
            sprintf(arrayDir[dirInt], "%s%s", path, pointD->d_name);
            dirInt++;
        }
        else {
            fileNum++;
            char* txtPath = (char *)malloc(sizeof(path)+sizeof(pointD->d_name)+2);
            sprintf(txtPath, "%s%s", path, pointD->d_name);
            if (fileNum > 1) fpath = (char**)realloc(fpath, sizeof(char**) * fileNum);
            else if (fileNum == 1) fpath = (char**)malloc(sizeof(char**));
            fpath[fileNum-1] = txtPath;
        }
    }
    // traverse 
    for (int i = 0; i < dirInt; i++) {
        strcat(arrayDir[i], "/");
        traversepath(arrayDir[i]);
    }
    closedir(adir); 
}

int main(int argc, char* argv[]) {
    // initialization
    const char *client_input = "ClientInput";
    const char *client_output = "ClientOutput";
    char* dirN = argv[1];
    int clientNum = atoi(argv[2]);
    fileNum = 0;
    time_t timeC;
    char buf[THREAD_NUM];
    time(&timeC);
    strcpy(buf, ctime(&timeC));
    buf[strcspn(buf, "\n")] = 0;
    printf("%s: Start the client.\n", buf);
    if (argc != 3) { // incorrect argument num
        printf("Incorrect Argument num.\n");
        return -1;
    }
    if (clientNum > THREAD_NUM || clientNum < 1) {
		printf("Incorrect client num.\n");
		return -1;
	}

    // File traversal and partitioning starts
    char* nameDir = realpath(dirN, NULL);
    time(&timeC);
    strcpy(buf, ctime(&timeC));
    strcat(nameDir, "/");
    buf[strcspn(buf, "\n")] = 0;
    printf("%s: File traversal and partitioning starts.\n", buf);
    traversepath(nameDir);
    if (fileNum == 0) { // return if no files
        printf("Cannot find the text files.\n");
        free(nameDir);
        return -1;
    }
    struct stat statD;
    if (stat(client_input, &statD) == -1) mkdir(client_input, 0777);
    int ct = 0;
    int clientAllo = fileNum/clientNum;
    int extra = fileNum%clientNum;

    //iterate through clients
    for (int i = 0; i < clientNum; i++) {
        FILE* fp;
        char nameF[THREAD_NUM];
        sprintf(nameF, "%s/Client%d.txt", client_input, i); // append the info
        fp = fopen(nameF, "w");
        if (fp == NULL) { // return if cannot open file
            free(nameDir);
            return -1;
        }
        for (int i = 0; i < clientAllo; i++) {
            fprintf(fp, "%s\n", fpath[ct]);
            ct++;
        }
        if (extra > 0) { // additional file
            fprintf(fp, "%s\n", fpath[ct]);
            ct++; extra--;
        }
        // close the file
        fclose(fp); 
    }
    pid_t pid;
    key_t key = 123;
    int tmp = 0;
    int msgQ = msgget(key, 0666 | IPC_CREAT);
    while (tmp < clientNum && pid != 0) {
        tmp++;
        if (tmp == 0 || pid > 0) pid = fork(); 
    } 

    if (pid == 0) {
        char nameInput[THREAD_NUM];
        sprintf(nameInput, "%s/Client%d.txt", client_input, tmp-1);
        FILE* fileIn = fopen(nameInput, "r");
        if (fileIn == NULL) return -1;
        struct msgB sendBuf;
        struct msgB recvBuf;
        sendBuf.type = tmp;
        // get the txt
        while (fgets(sendBuf.t, 256, fileIn) != NULL) {
            sendBuf.t[strcspn(sendBuf.t, "\n")] = 0;
            time(&timeC);
            strcpy(buf, ctime(&timeC));
            buf[strcspn(buf, "\n")] = 0;
            printf("%s: Process %d has sent path %s\n", buf, tmp-1, sendBuf.t);
            //send file path
            msgsnd(msgQ, (void*)&sendBuf, sizeof(sendBuf), 0);
            //recieve ACK
            msgrcv(msgQ, (void*)&recvBuf, sizeof(recvBuf), tmp + THREAD_NUM, 0);
            time(&timeC);
            strcpy(buf, ctime(&timeC));
            buf[strcspn(buf, "\n")] = 0;
            printf("%s: ACK was sent to client %d for %s\n", buf, tmp - 1, sendBuf.t);
            strcmp(recvBuf.t, "ACK");
        }
        // close the file and send the END
        char fileOut[THREAD_NUM];
        fclose(fileIn);
        strcpy(sendBuf.t, "END");
        msgsnd(msgQ, (void*)&sendBuf, sizeof(sendBuf), 0);
        time(&timeC);
        strcpy(buf, ctime(&timeC));
        buf[strcspn(buf, "\n")] = 0;
        // send END to client
        printf("%s: END was sent to client %d\n", buf, tmp - 1);
        msgrcv(msgQ, (void *)&recvBuf, sizeof(recvBuf), tmp+THREAD_NUM, 0);
        time(&timeC);
        strcpy(buf, ctime(&timeC));
        buf[strcspn(buf, "\n")] = 0;
        // Get the message from server
        printf("%s: Client process %d has received the message from the server\n", buf, tmp-1);
        struct stat statD;
        if (stat(client_output, &statD) == -1) mkdir(client_output, 0777);
        sprintf(fileOut, "%s/Client%d.txt", client_output, tmp-1);
        FILE* fileout = fopen(fileOut, "w");
        fprintf(fileout, "%s\n", recvBuf.t);
        // close and free the variable
        fclose(fileout);
        for (int count = 0; count < fileNum; count++) free(fpath[count]);
        free(fpath); free(nameDir);
        return 0;
    }
    else { 
        for (int j = 0; j < clientNum; j++) {
            if (wait(NULL) < 0) {
                for (int count = 0; count < fileNum; count++) free(fpath[count]);
                free(fpath); free(nameDir);
                return -1;
            }
        }
    }
    time(&timeC);
    strcpy(buf, ctime(&timeC));
    buf[strcspn(buf, "\n")] = 0;
    printf("%s: Client has finished.\n", buf);
    for (int count = 0; count < fileNum; count++) free(fpath[count]);
    free(fpath);
    msgctl(msgQ, IPC_RMID, NULL);
    // free dir
    free(nameDir);
    return 0;
}