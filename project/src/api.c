#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <sys/un.h>
#include <stdlib.h>
#include <libgen.h>
#include <sys/stat.h>

#include "api.h"


#define STANDARDMSGSIZE 1024
#define SMALLMSGSIZE 128
#define MAXTEXTLENGHT 1048576
#define MAXFILENAMELENGHT 108

#define GESTERRP(a, b) if((a) == NULL){b}
#define GESTERRI(a, b) if((a)){b}

int statoConnessione = 0;//flag indicante lo stato di connessione del client
int socketFD; //fd del socket
char socketName[MAXFILENAMELENGHT]; //nome del socket

int msSleep(long time){
    if(time < 0){
        errno = EINVAL;
    }
    int res;
    struct timespec t;
    t.tv_sec = time/1000;
    t.tv_nsec = (time % 1000) * 1000000;

    do {
        res = nanosleep(&t, &t);
    }while(res && errno == EINTR);

    return res;
}

static int timeOut(struct timespec b){
    struct timespec a;
    clock_gettime(CLOCK_REALTIME, &a);

    //comparo secondi e poi nansecondi
    if(a.tv_sec == b.tv_sec){
        if(a.tv_nsec == b.tv_nsec)
            return 0;
        else if(a.tv_nsec < b.tv_nsec)
            return -1;
        else
            return 1;
    } else if(a.tv_sec < b.tv_sec)
        return -1;
    else
        return 1;
}

int readn(long fd, void *buf, size_t size) {
    int readn = 0;
    int r = 0;

    while ( readn < size ){

        if ( (r = read(fd, buf, size)) == -1 ){
            if( errno == EINTR )
                // se la read è stata interrotta da un segnale riprende
                continue;
            else{
                perror("ERRORE: Readn");
                return -1;
            }
        }
        if ( r == 0 )
            return readn; // Nessun byte da leggere rimasto

        readn += r;
    }

    return readn;
}

int writen(long fd, const void *buf, size_t nbyte){
    int writen = 0;
    int w = 0;

    while ( writen < nbyte ){
        if ( (w = write(fd, buf, nbyte) ) == -1 ){
            /* se la write è stata interrotta da un segnale riprende */
            if ( errno == EINTR )
                continue;
            else if ( errno == EPIPE )
                break;
            else{
                perror("ERRORE: Writen");
                return -1;
            }
        }
        if( w == 0 )
            return writen;

        writen += w;
    }

    return writen;
}

int receiveNFile(int n, const char* dir){
    int i = 0;
    int textSize = 0;
    char firstMsg[STANDARDMSGSIZE] = "";
    char *text;
    char *basePath;
    File newFile;
    char finalPath[MAXFILENAMELENGHT] = "";

    while(i<n){
        GESTERRI(readn(socketFD, firstMsg, STANDARDMSGSIZE) == -1, errno = EREMOTEIO; return -1;)//lettura path

        char *token;
        char *save = NULL;
        token = strtok_r(firstMsg, ";", &save);//esito richiesta
        basePath = basename(token);

        token = strtok_r(NULL, ";", &save);//ricevuta dimensione del testo
        textSize = strtol(token , NULL, 10);

        text = malloc(sizeof(char) * textSize);
        GESTERRP(text, errno = ENOMEM; perror(receiveNFile); return -1;)//fallimento malloc
        GESTERRI(readn(socketFD, text, textSize) == -1, free(text); errno = EREMOTEIO; return -1;)//lettura contenuto di un file


        sprintf(finalPath, "%s/%s", dir, basePath);//path del file da creare trovato
        newFile = fopen(finalPath, "w");//file aperto o creato
        if(newFile == NULL){
            perror("receiveNFile");
        }
        else{
            fprintf(newFile, "%s", text);
            fclose(newFile);
        }
        i++;
    }
    return 0;
}

int openConnection(const char* sockname, int msec, const struct timespec abstime) {

    struct sockaddr_un sa;
    memset(&sa, 0, sizeof(sa));
    strncpy(sa.sun_path, sockname, MAXFILENAMELENGHT);
    sa.sun_family = AF_UNIX;

    GESTERRI((socketFD = socket(AF_UNIX, SOCK_STREAM, 0)) == -1,//prendiamo il file descriptor
                errno = EINVAL;perror("open connection");return -1;)

    int riuscita = connect(socketFD,(struct sockaddr*)&sa,sizeof(sa));
    while ((riuscita == -1) &&
                (timeOut(abstime) == -1)){
        //attesa di msec secondi
        fgetmsSleep(msec);
        riuscita = connect(socketFD,(struct sockaddr*)&sa,sizeof(sa));
    }
    if (riuscita == -1) {
        //se siamo oltre l tempo massimo ci fermiamo
        errno = ETIMEDOUT;
        perror("open connection");
        return -1;
    }

    statoConnessione = 1;//siamo connessi
    strcpy(socketName, sockname);//memorizziamo il nome del socket
    return 0;
}
int closeConnection(const char* sockname){

    GESTERRI(statoConnessione == 0, errno = ENOTCONN; return -1;)//disconnesso

    if (strcmp(socketName,sockname) == 0){// socket corretto
        char buffer [STANDARDMSGSIZE];
        memset(buffer,0,STANDARDMSGSIZE);
        sprintf(buffer,"2");

        GESTERRI(writen(socketFD, buffer, STANDARDMSGSIZE) == -1, errno = EREMOTEIO; return -1;)//inviamo il messaggio di chiusura
        GESTERRI(close(socketFD) == -1, errno = EREMOTEIO; return -1;)//chiudiamo connessione

        return 0;
    }
    else{
        errno = EINVAL;
        return -1;
    }
}
int openFile(const char* pathname, int flags) {

    GESTERRI(statoConnessione == 0, errno = ENOTCONN; return -1;)//client disconnesso

    char buffer [STANDARDMSGSIZE];
    memset(buffer,0,STANDARDMSGSIZE);
    snprintf(buffer, STANDARDMSGSIZE,"3;%d;%s;",flags , pathname);// il comando viene scritto sulla stringa buffer

    GESTERRI(writen(socketFD, buffer, STANDARDMSGSIZE) == -1, errno = EREMOTEIO; return -1;)//scrittura
    char message[SMALLMSGSIZE];
    GESTERRI(readn(socketFD, message, SMALLMSGSIZE) == -1, errno = EREMOTEIO; return -1;)//lettura

    char* token;
    char* save = NULL;
    token = strtok_r(message,";", &save);

    if (strcmp(token, "-1") == 0){ //op fallita
        token = strtok_r(NULL,";", &save);
        errno = (int)strtol(token, NULL, 10);
        return -1;
    }
    else return 0;
}
int closeFile(const char* pathname) {

    GESTERRI(statoConnessione == 0, errno = ENOTCONN; return -1;)//client disconnesso

    char buffer[STANDARDMSGSIZE];
    memset(buffer,0,STANDARDMSGSIZE);
    sprintf(buffer, "10;%s;", pathname);

    GESTERRI(writen(socketFD, buffer, STANDARDMSGSIZE) == -1, errno = EREMOTEIO; return -1;)//scrittura
    char message[SMALLMSGSIZE];
    GESTERRI(readn(socketFD, message, SMALLMSGSIZE) == -1, errno = EREMOTEIO; return -1;)//lettura

    char* token;
    char* save = NULL;
    token = strtok_r(message, ";", &save);

    if (strcmp(token, "-1") == 0){
        token = strtok_r(NULL,";", &save);
        errno = (int)strtol(token, NULL, 10);
        return -1;
    }
    else return 0;

}
int removeFile(const char* path) {

    GESTERRI(statoConnessione == 0, errno = ENOTCONN; return -1;)//client disconnesso

    char buffer [STANDARDMSGSIZE];
    memset(buffer,0,STANDARDMSGSIZE);
    sprintf(buffer, "11;%s;", path);

    GESTERRI(writen(socketFD, buffer, STANDARDMSGSIZE) == -1, errno = EREMOTEIO; return -1;)//scrittura
    char message[SMALLMSGSIZE];
    GESTERRI(readn(socketFD, message, SMALLMSGSIZE) == -1, errno = EREMOTEIO; return -1;)//lettura

    char* token;
    char* save = NULL;
    token = strtok_r(message, ";", &save);

    if (strcmp(token, "-1") == 0){ // operazione fallita
        token = strtok_r(NULL,";",&save);
        errno = (int)strtol(token, NULL, 10);
        return -1;
    }
    else return 0;
}
int lockFile(const char* path) {

    GESTERRI(statoConnessione == 0, errno = ENOTCONN; return -1;)//client disconnesso

    char buffer [STANDARDMSGSIZE];
    memset(buffer,0,STANDARDMSGSIZE);
    sprintf(buffer, "8;%s;", path);

    GESTERRI(writen(socketFD, buffer, STANDARDMSGSIZE) == -1, errno = EREMOTEIO; return -1;)//scrittura
    char message[SMALLMSGSIZE];
    GESTERRI(readn(socketFD, message, SMALLMSGSIZE) == -1, errno = EREMOTEIO; return -1;)//lettura

    char* token;
    char* save = NULL;
    token = strtok_r(message, ";", &save);

    if (strcmp(token, "-1") == 0) { //operazione fallita
        token = strtok_r(NULL,";",&save);
        errno = (int)strtol(token, NULL, 10);
        return -1;
    }
    else return 0;
}
int unlockFile(const char* path){

    GESTERRI(statoConnessione == 0, errno = ENOTCONN; return -1;)//client disconnesso

    char buffer [STANDARDMSGSIZE];
    memset(buffer,0,STANDARDMSGSIZE);
    sprintf(buffer, "9;%s", path);

    GESTERRI(writen(socketFD, buffer, STANDARDMSGSIZE) == -1, errno = EREMOTEIO; return -1;)//scrittura
    char message[SMALLMSGSIZE];
    GESTERRI(readn(socketFD, message, SMALLMSGSIZE) == -1, errno = EREMOTEIO; return -1;)//lettura

    char* token;
    char* save = NULL;
    token = strtok_r(message, ";", &save);

    if (strcmp(token, "-1") == 0){ //operazione fallita
        token = strtok_r(NULL,";", &save);
        errno = (int)strtol(token, NULL, 10);
        return -1;
    }
    else return 0;
}
int writeFile(const char* path, const char* dir) {

    GESTERRI(statoConnessione == 0, errno = ENOTCONN; return -1;)//client disconnesso
    GESTERRP(dir, errno = EINVAL; return -1;)

    if (dir != NULL)// se la directory non esiste viene creata
        if (mkdir(dir, S_IRWXU) == -1)
            if (errno != EEXIST)
                return -1;

    FILE *fileP = fopen(path, "r");
    GESTERRP(fileP, errno = ENOENT; return -1;)//apriamo il file

    char dummy[MAXTEXTLENGHT] = "\0";
    char buffer[50];
    int n = 0;
    int charLetti = fread(buffer, sizeof(char), 50, fileP);
    while(charLetti > 0){

        if(n + charLetti>= MAXTEXTLENGHT){ fclose(fileP); errno = EFBIG; return -1;}//file troppo grande

        memcopy(dummy + n, buffer, charLetti);
        n += charLetti;

        if(charLetti == 50) charLetti = fread(buffer, sizeof(char), 50, fileP);
        else charLetti = -1
    }
    fclose(fileP)//chiudiamo il file
    dummy[n - 1] = ';';
    dummy[n] = '\0';
    n++;//lunghezza di dummy compreso il terminatore

    // preparaione del comando per il server
    char buffer[STANDARDMSGSIZE];
    memset(buffer, 0, STANDARDMSGSIZE);
    sprintf(buffer, "6;%s;%d", path, n);//primo messaggio

    GESTERRI(writen(socketFD, buffer, STANDARDMSGSIZE) == -1, errno = EREMOTEIO; return -1;)//scrittura 1
    GESTERRI(writen(socketFD, dummy, n) == -1, errno = EREMOTEIO; return -1;)//scrittura 2 funziona??, credo di si

    char message[SMALLMSGSIZE];
    GESTERRI(readn(socketFD, message, SMALLMSGSIZE) == -1, errno = EREMOTEIO; return -1;)//lettura

    char *token;
    char *save = NULL;
    token = strtok_r(message, ";", &save);//esito richiesta

    int errnoVal = 0;
    int retVal;

    retVal =  strtol(token, NULL, 10);

    if (retVal != 0) { //se l'operazione è fallita memorizziamo errno
        token = strtok_r(NULL, ";", &save);
        errnoVal = (int) strtol(token, NULL, 10);
    }

    token = strtok_r(NULL, ";", &save);
    int nFile = strtol(token, NULL, 10);//controlliamo se abbiamo file da memorizzare

    if(nFile != 0 && dir != NULL) GESTERRI(receiveNFile(nFile, dir) == -1, return -1);//abbiamo file da memorizzare e lo facciamo

    errno = errnoVal;
    return retVal;//restituiamo l'esito dell'operazione e settiamo errno correttamente
}