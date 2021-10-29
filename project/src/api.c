#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/un.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <libgen.h>
#include "api.h"
#include <limits.h>


#define STANDARDMSGSIZE 1024   // dimensione messaggio standard tra api e server
#define SMALLMSGSIZE 128 // dimensione messaggio small tra api e server
#define MAXTEXTLENGHT 1048576 // grandezza massima del contenuto di un file : 1MB
#define MAXFILENAMELENGHT 108 // lunghezza standard unix

//errori
#define GESTERRP(a, b) if((a) == NULL){b}
#define GESTERRI(a, b) if((a)){b}

static int statoConnessione = 0;//flag indicante lo stato di connessione del client
static int socketFD; //fd del socket
static char socketName[MAXFILENAMELENGHT]; //nome del socket

//dimensione letture e scritture
static size_t lastOpWSize = 0;//write sul server
static size_t lastOpRSize = 0;//read dal server
size_t getLastOpWSize(){ return lastOpWSize; }
size_t getLastOpRSize(){ return lastOpRSize; }


/**
 *   @brief funzione che ferma l'esecuzione del programma per time millisecondi
 *   @param time tempo di attesa
 *   @return 0 se termina correttamente, -1 altrimenti
 */
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
/**
 *   @brief funzione che confronta b con il tempo attuale
 *   @param b struct timespect che rappresenta un tempo assoluto da confontare
 *   @return -1 se b è prima del tempo attuale, 0 se b è uguale al tempo attuale, 1 altrimenti
 */
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
/**
 *   @brief Funzione che permette di effettuare la read completandola in seguito alla ricezione di un segnale
 *
 *   @param fd     descrittore della connessione
 *   @param buf    puntatore al messaggio da inviare
 *
 *   @return Il numero di bytes letti, -1 se c'e' stato un errore
 */
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
/**
 *   @brief Funzione che permette di effettuare la write completandola in seguito alla ricezione di un segnale
 *
 *   @param fd descrittore della connessione
 *   @param buf puntatore al messaggio da inviare
 *
 *   @return Il numero di bytes scritti, -1 se c'è stato un errore
 */
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
/**
 *   @brief Funzione che si occupa di ricevere e memorizzare n file dal server
 *   @param n numero di file da ricevere
 *   @param dir directory dove memorizzare i file ricevuti
 *   @return 0 se termina correttamente, -1 se genera errore
 */
int receiveNFile(int n, const char* dir){
    int i = 0;
    int textSize = 0;
    char firstMsg[STANDARDMSGSIZE] = "";
    char *text;
    char *basePath;
    FILE *newFile;
    char finalPath[MAXFILENAMELENGHT] = "";

    if(n == 0) n = INT_MAX;

    lastOpRSize = 0;//inizio a contare i byre ricevuti
    while(i<n){

        GESTERRI(readn(socketFD, firstMsg, STANDARDMSGSIZE) == -1, errno = EREMOTEIO; return -1;)//lettura path
        printf("RICEVUTO: %s\n", firstMsg);

        GESTERRI(strcmp(firstMsg, "END") == 0, return i;)//se i file finiscono prima del previsto

        char *token;
        char *save = NULL;
        token = strtok_r(firstMsg, ";", &save);//esito richiesta
        basePath = basename(token);

        token = strtok_r(NULL, ";", &save);//ricevuta dimensione del testo
        textSize = strtol(token , NULL, 10);

        text = malloc(sizeof(char) * textSize);
        GESTERRP(text, errno = ENOMEM; perror("receiveNFile"); return -1;)//fallimento malloc
        GESTERRI(readn(socketFD, text, textSize) == -1, free(text); errno = EREMOTEIO; return -1;)//lettura contenuto di un file
        lastOpRSize += textSize;//incremento i byte totali letti

        sprintf(finalPath, "%s/%s", dir, basePath);//path del file da creare trovato
        newFile = fopen(finalPath, "w");//file aperto o creato
        GESTERRP(newFile, perror("receiveNFile");)

        fprintf(newFile, "%s", text);
        fclose(newFile);

        free(text);
        i++;
    }
    return i;
}

int openConnection(const char* sockname, int msec, const struct timespec abstime) {
    GESTERRP(sockname, errno = EINVAL; return -1;)//socket name == NULL

    struct sockaddr_un sa;
    memset(&sa, 0, sizeof(sa));
    strncpy(sa.sun_path, sockname, MAXFILENAMELENGHT);
    sa.sun_family = AF_UNIX;

    GESTERRI((socketFD = socket(AF_UNIX, SOCK_STREAM, 0)) == -1,//prendiamo il file descriptor
                errno = EINVAL;perror("open connection");return -1;)

    int riuscita = connect(socketFD,(struct sockaddr*) &sa, sizeof(sa));
    while ((riuscita != 0) &&
                (timeOut(abstime) != 1)){
        //attesa di msec secondi
        msSleep(msec);
        riuscita = connect(socketFD,(struct sockaddr*)&sa,sizeof(sa));
    }
    if (riuscita != 0) {
        //se siamo oltre l tempo massimo ci fermiamo
        errno = ETIMEDOUT;
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
        sprintf(buffer,"2;");

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
    sprintf(buffer, "9;%s;", path);

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
int writeFile(const char* pathname, const char* dir) {

    GESTERRI(statoConnessione == 0, errno = ENOTCONN; return -1;)//client disconnesso

    if (dir != NULL)// se la directory non esiste viene creata
        if (mkdir(dir, S_IRWXU) == -1)
            if (errno != EEXIST)
                return -1;

    FILE *fileP = fopen(pathname, "r");
    GESTERRP(fileP, errno = ENOENT; return -1;)//apriamo il file

    char dummy[MAXTEXTLENGHT] = "\0";
    char bufferCopia[50];
    int n = 0;
    int charLetti = fread(bufferCopia, sizeof(char), 50, fileP);
    while(charLetti > 0){

        if(n + charLetti>= MAXTEXTLENGHT){ fclose(fileP); errno = EFBIG; return -1;}//file troppo grande

        memcpy(dummy + n, bufferCopia, charLetti);
        n += charLetti;

        if(charLetti == 50) charLetti = fread(bufferCopia, sizeof(char), 50, fileP);
        else charLetti = -1;
    }
    fclose(fileP);//chiudiamo il file
    dummy[n] = '\0';
    n++;//lunghezza di dummy compreso il terminatore

    // preparaione del comando per il server
    char buffer[STANDARDMSGSIZE];
    memset(buffer, 0, STANDARDMSGSIZE);
    sprintf(buffer, "6;%s;%d;", pathname, n);//primo messaggio

    GESTERRI(writen(socketFD, buffer, STANDARDMSGSIZE) == -1, errno = EREMOTEIO; return -1;)//scrittura 1
    GESTERRI(writen(socketFD, dummy, n) == -1, errno = EREMOTEIO; return -1;)//scrittura 2
    lastOpWSize = n;//imposto la dimensione di scrittura

    char message[SMALLMSGSIZE];
    GESTERRI(readn(socketFD, message, SMALLMSGSIZE) == -1, errno = EREMOTEIO; return -1;)//lettura

    char *save = NULL;
    char *token = strtok_r(message, ";", &save);//esito richiesta

    int errnoVal = 0;
    int retVal;

    retVal =  strtol(token, NULL, 10);

    if (retVal != 0) { //se l'operazione è fallita memorizziamo errno
        token = strtok_r(NULL, ";", &save);
        errnoVal = (int) strtol(token, NULL, 10);
    }

    token = strtok_r(NULL, ";", &save);
    int nFile = strtol(token, NULL, 10);//controlliamo se abbiamo file da memorizzare

    if(nFile != 0 && dir != NULL) GESTERRI(receiveNFile(nFile, dir) == -1, return -1;)//abbiamo file da memorizzare e lo facciamo

    errno = errnoVal;
    return retVal;//restituiamo l'esito dell'operazione e settiamo errno correttamente
}
int appendToFile(const char* pathname, void* buf, size_t size, const char* dir){
    GESTERRI(statoConnessione == 0, errno = ENOTCONN; return -1;)//client disconnesso

    if (dir != NULL)// se la directory non esiste viene creata
        if (mkdir(dir, S_IRWXU) == -1)
            if (errno != EEXIST)
                return -1;

    //preparaione del comando per il server
    char buffer[STANDARDMSGSIZE];
    memset(buffer, 0, STANDARDMSGSIZE);
    sprintf(buffer, "7;%s;%lu;", pathname, size);//primo messaggio

    GESTERRI(writen(socketFD, buffer, STANDARDMSGSIZE) == -1, errno = EREMOTEIO; return -1;)//scrittura 1
    GESTERRI(writen(socketFD, buf, size) == -1, errno = EREMOTEIO; return -1;)//scrittura 2
    lastOpWSize = size;//imposto la dimensione di scrittura

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

    if(nFile != 0 && dir != NULL) GESTERRI(receiveNFile(nFile, dir) == -1, return -1;)//abbiamo file da memorizzare e lo facciamo

    errno = errnoVal;
    return retVal;//restituiamo l'esito dell'operazione e settiamo errno correttamente
}
int readFile(const char* pathname, void** buf, size_t* size){
    GESTERRI(statoConnessione == 0, errno = ENOTCONN; return -1;)//client disconnesso

    char buffer [STANDARDMSGSIZE];
    memset(buffer,0,STANDARDMSGSIZE);
    sprintf(buffer, "4;%s;",pathname);

    GESTERRI(writen(socketFD, buffer, STANDARDMSGSIZE) == -1, errno = EREMOTEIO; return -1;)//scrittura
    char message[SMALLMSGSIZE];
    GESTERRI(readn(socketFD, message, SMALLMSGSIZE) == -1, errno = EREMOTEIO; return -1;)//lettura
    //0;10;
    //msg dimensione 10

    char* token;
    char* save = NULL;
    token = strtok_r(message, ";", &save);

    if (strcmp(token, "-1") == 0){
        token = strtok_r(NULL, ";",&save);
        errno = (int)strtol(token, NULL, 10);
        return -1;
    }
    else{
        token = strtok_r(NULL, ";",&save);

        *size = (unsigned long)strtol(token, NULL, 10);
        printf("%lu\n", *size);
        *buf = malloc(sizeof(char) * (*size));
        GESTERRP(*buf, perror("malloc failed"); return -1;)

        memset(*buf, 0, sizeof(char) * (*size));

        GESTERRI(readn(socketFD, *buf, *size) == -1, errno = EREMOTEIO; return -1;)//lettura
        lastOpRSize = *size;

        return 0;
    }
}
int readNFiles(int N, const char* dir) {
    GESTERRI(statoConnessione == 0, errno = ENOTCONN; return -1;)//client disconnesso

    if (dir != NULL)// se la directory non esiste viene creata
        if (mkdir(dir, S_IRWXU) == -1)
            if (errno != EEXIST)
                return -1;


    char buffer [STANDARDMSGSIZE];
    memset(buffer,0,STANDARDMSGSIZE);
    sprintf(buffer, "5;%d;",N);

    GESTERRI(writen(socketFD, buffer, STANDARDMSGSIZE) == -1, errno = EREMOTEIO; return -1;)//scrittura

    return receiveNFile(N, dir);
}
