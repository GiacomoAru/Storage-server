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
#include <limits.h>
#include "api.h"

size_t getLastOpWSize();
size_t getLastOpRSize();
int msSleep(long time);

int openConnection(const char* nome_sock, int msec, const struct timespec abstime);
int closeConnection(const char* nome_sock);
int openFile(const char* path, int flags);
int readFile(const char* path, void** buf, size_t* size);
int readNFile(int N, const char* dir);
int writeFile(const char* path, const char* dir);
int appendToFile(const char* path, void* buf, size_t size, const char* dir);
int lockFile(const char* path);
int unlockFile(const char* path);
int closeFile(const char* path);
int removeFile(const char* path);
int readNFiles(int N, const char* dir);

#endif //PROG_API_H