#ifndef CLIENT_H
#define CLIENT_H

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <dirent.h>

struct msgB {
	long type;
	char t[256];
};

#endif