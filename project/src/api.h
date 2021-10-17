
#ifndef PROG_API_H
#define PROG_API_H
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/un.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <limits.h>
#include <libgen.h>
#include <dirent.h>
#include "api.h"

int msSleep(long time);

int openConnection(const char* nome_sock, int msec, const struct timespec abstime);
int closeConnection(const char* nome_sock);

int openFile(const char* path, int flags);
int writeFile(const char* path, const char* dir);
int removeFile(const char* path);
int lockFile(const char* path);
int unlockFile(const char* path);
int closeFile(const char* path);


#endif