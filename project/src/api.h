#ifndef PROG_API_H
#define PROG_API_H

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

#endif