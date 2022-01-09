#include "api.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include <fcntl.h>
#include <libgen.h>

#define MSG_SIZE 2048
#define MAX_CNT_LEN 4096 // grandezza massima del contenuto di un file: 1 KB

#define MAXFILENAMELENGHT 108

//errori
#define GESTERRP(a, b) if((a) == NULL){b}
#define GESTERRI(a, b) if((a)){b}
#define PRINT(a) if(pFlag > 0){a}

// il nodo della lista dei comandi:
typedef struct n{
    char cmd; // stringa in cui sarà identificato il comando richiesto
    char * arg; // stringa nella quale saranno inseriti i vari argomenti utili al comando
    char * dir; // cartella relativa al comando, non è sempre utile o usata
    struct n* next; // puntatore al cmando successivo della lista
    struct n* prec; // puntatore al comando precedente nella lista
} node;
typedef struct l {
    node * head; // testa della lista
    node * tail; //coda della lista (utile perché la lista è bidirezionale)
    size_t size; //dimensione della lista, serve??
} list;

//flag comandi unici
int pFlag = 0; //stampe nello stdout
int fFlag = 0; //nome del socket
int tFlag = 0; //tempo tra 2 richieste

int totFileScritti = 0;
int totFileDaScrivere = 0;
size_t totR = 0;
size_t totW = 0;

int tms = 0; // tempo di attesa tra 2 operazioni sul server (millisecondi)

//variabili utili
char * fArg = NULL;

//lista comandi
list * lst = NULL;

/**
 *   @brief Funzione che aggiunge un nodo ad una lista
 *   @param lst puntatore alla lista
 *   @param cmd stringa contenente il comando del nodo
 *   @param arg stringa contenente l'argomento del nodo
 */
static void listAddTail(char cmd, char* arg) {

    GESTERRP(arg, perror("cmd == NULL"); exit(EXIT_FAILURE);)

    node * n = malloc(sizeof(node));
    GESTERRP(n, perror("malloc failed"); exit(EXIT_FAILURE);)
    n->next = NULL;
    n->prec = NULL;
    n->dir = NULL;
    n->cmd = cmd;

    n->arg = malloc(sizeof(char) * (strlen(arg) + 1));
    GESTERRP(n->arg, perror("malloc failed"); free(n); exit(EXIT_FAILURE);)
    strcpy(n->arg, arg);

    if (lst->tail == NULL) {
        lst->head = n;
    } else {
        lst->tail->next = n;
        n->prec = lst->tail;
    }
    lst->tail = n;
    lst->size++;
}
/**
 *   @brief Funzione che esegue la free della lista
 *   @param lst lista dei comandi
 */
static void freeList() {
    node* dummy = NULL;
    while(lst->head != NULL){
        dummy = lst->head;
        lst->head = lst->head->next;

        free(dummy->dir);
        free(dummy->arg);
        free(dummy);
    }
    free(lst);
    lst = NULL;
}

/**
 *   @brief Funzione che esegue la scrittura nel server di un numero di file scelto dentro ad una directory scelta
 *   @param arg directory in cui sono contenuti i file da scrivere
 *   @param dir directory in cui scrivere i file espulsi dal server
 **/
static void recursiveWrite(char* arg, char* dir){
    struct stat fileInfo;
    int dummyRet;

    GESTERRI(stat(arg, &fileInfo) == -1, fprintf(stderr,"w:stat(\"%s\"):%s\n", arg, strerror(errno)); return;)

    if(S_ISDIR(fileInfo.st_mode)){

        DIR* directory = opendir(arg);
        GESTERRP(directory, perror("opendir"); return;)

        struct dirent * nextFile;
        nextFile = readdir(directory);

        while (nextFile != NULL && totFileScritti < totFileDaScrivere){
            if(strcmp(nextFile->d_name, ".") != 0 && strcmp(nextFile->d_name, "..") != 0) {
                char path[MAXFILENAMELENGHT];
                snprintf(path, MAXFILENAMELENGHT, "%s/%s", arg, nextFile->d_name);

                totFileScritti++;
                recursiveWrite(path, dir);
            }
            nextFile = readdir(directory);
        }
        free(directory);

    }else if(S_ISREG(fileInfo.st_mode)) {

        dummyRet = openFile(arg, 3);
        GESTERRI(dummyRet == -1, fprintf(stderr, "w:openFile(\"%s\"):%s\n", arg, strerror(errno)); totFileScritti--; return;)
        dummyRet = writeFile(arg, dir);
        GESTERRI(dummyRet == -1, removeFile(arg); fprintf(stderr, "w:writeFile(\"%s\"):%s\n", arg, strerror(errno)); totFileScritti--; return;)

        totR += getLastOpRSize();
        totW += getLastOpWSize();

        dummyRet = closeFile(arg);
        GESTERRI(dummyRet == -1, fprintf(stderr, "w:closeFile(\"%s\"):%s\n", arg, strerror(errno)); return;)
    }
}
/**
 *   @brief Funzione che termina il client a seguito di un errore incorreggibile
 */
static void freeAndFail() {//errori gravi
    perror("errore");
    freeList();
    free(fArg);
    exit(EXIT_FAILURE);
}
/**
 *   @brief Funzione che termina il client correttamente
 */
static void freeAndExit(){//comportamento standard programma
    freeList();
    free(fArg);
    return;
}
/**
 *   @brief Funzione che riconosce e de esegue un operazione nel server
 *   @param cmd il tipo di operazione da eseguire
 *   @param arg gli argomenti dell'operazione da eseguire
 *   @param dir la directory in cui scrivere i file espulsi dal server
 */
static void exec(char cmd, char* arg, char* dir){
    //printf("##\tEsecuzione: %c\t%s\t%s##\n", cmd, arg, dir);

    char * finalPath = NULL;
    char* save = NULL;
    char * token = NULL;
    struct stat fileInfo;
    int dummyRet;

    int totFile = 0;//prova


    switch (cmd){
        //lettura comandi
        case 'w':
            token = strtok_r(arg, ",", &save);

            char* dummyStr = strtok_r(NULL, ",", &save);
            totFileDaScrivere = INT_MAX;
            if(dummyStr != NULL && strcmp(dummyStr, "") != 0) totFileDaScrivere = strtol(dummyStr, NULL, 10);

            totFileScritti = 0;
            totR = 0;
            totW = 0;

            GESTERRP(finalPath = realpath(arg, finalPath), fprintf(stderr, "\"%s\" non esiste\n", arg); return;)
            GESTERRI(stat(arg, &fileInfo) == -1, fprintf(stderr,"w:stat(\"%s\"):%s", arg, strerror(errno)); return;)
            GESTERRI(!S_ISDIR(fileInfo.st_mode), fprintf(stderr, "\"%s\" non è un file accettato\n", finalPath); token = strtok_r(NULL, ",", &save); return;)

            recursiveWrite(finalPath, dir);

            PRINT(printf("OP: write di");
                    if(totFileDaScrivere == INT_MAX) printf(" MAX ");
                    else printf(" %d ", totFileDaScrivere);
                    printf("files contenuti nella cartella \"%s\":\n\tfile realmente scritti: %d\n\tdimensione totale scritture: %lu\n"
                         , arg, totFileScritti, totW);
                if(dir != NULL)printf("\tdimensione totale file espulsi: %lu\n\tmemorizzati nella cartella: \"%s\"\n", totR, dir);
                )
            break;

        case 'W'://openfile, writeFile, closeFile (rimane la lock)
            token = strtok_r(arg, ",", &save);
            while(token != NULL && strcmp(token, "") != 0){
                //il nome del file è troppo grande
                GESTERRI(strlen(token) >= MAXFILENAMELENGHT, errno = ENAMETOOLONG; fprintf(stderr,"W:openFile(\"%s\"):%s\n", finalPath,strerror(errno)); token = strtok_r(NULL, ",", &save); continue;)

                GESTERRP(finalPath = realpath(token, finalPath), fprintf(stderr, "\"%s\" non esiste\n", token); token = strtok_r(NULL, ",", &save); continue;)
                GESTERRI(stat(finalPath, &fileInfo) == -1, perror("stat"); token = strtok_r(NULL, ",", &save);continue;)

                GESTERRI(!S_ISREG(fileInfo.st_mode), fprintf(stderr, "\"%s\" non è un file accettato\n", finalPath); token = strtok_r(NULL, ",", &save); continue;)

                dummyRet = openFile(finalPath, 3);
                GESTERRI(dummyRet == -1, fprintf(stderr,"W:openFile(\"%s\"):%s\n", finalPath,strerror(errno)); token = strtok_r(NULL, ",", &save); continue;)
                dummyRet = writeFile(finalPath, dir);
                GESTERRI(dummyRet == -1, removeFile(finalPath); fprintf(stderr,"W:writeFile(\"%s\"):%s\n", finalPath,strerror(errno)); token = strtok_r(NULL, ",", &save); continue;)
                dummyRet = closeFile(finalPath);
                GESTERRI(dummyRet == -1, fprintf(stderr,"W:closeFile(\"%s\"):%s\n", finalPath,strerror(errno)); token = strtok_r(NULL, ",", &save); continue;)

                PRINT(printf("OP: write file \"%s\" sul server:\n\tdimensione scrittura: %lu\n", finalPath, getLastOpWSize());
                              if(dir != NULL) printf("\tdimensione file espulsi dal server: %lu\n\tmemorizzati nella cartella: \"%s\"\n",  getLastOpRSize(), dir);)

                token = strtok_r(NULL, ",", &save);
            }
            break;
        case 'r': // readFile
            token = strtok_r(arg, ",", &save);
            while(token != NULL && strcmp(token, "") != 0){

                GESTERRP(finalPath = realpath(token, finalPath), fprintf(stderr, "\"%s\" non esiste\n", token); token = strtok_r(NULL, ",", &save);  continue;)
                void * buf = NULL;
                size_t size;

                dummyRet = openFile(finalPath, 0);
                GESTERRI(dummyRet == -1, fprintf(stderr,"r:openFile(\"%s\"):%s\n", finalPath,strerror(errno)); token = strtok_r(NULL, ",", &save); continue;)
                dummyRet = readFile(finalPath, &buf, &size);
                GESTERRI(dummyRet == -1, fprintf(stderr,"r:readFile(\"%s\"):%s\n", finalPath,strerror(errno));token = strtok_r(NULL, ",", &save); continue;)
                dummyRet = closeFile(finalPath);
                GESTERRI(dummyRet == -1, fprintf(stderr,"r:closeFile(\"%s\"):%s\n", finalPath,strerror(errno)); token = strtok_r(NULL, ",", &save); continue;)

                if(dir != NULL){
                    FILE * newFile = NULL;
                    char dummyChar[MAXFILENAMELENGHT] = "";
                    sprintf(dummyChar,"%s/%s", dir, basename(token));

                    newFile = fopen(dummyChar, "w"); //aperto o creato
                    GESTERRP(newFile, perror("fopen");token = strtok_r(NULL, ",", &save); continue;)
                    fprintf(newFile, "%s", (char *)buf);
                    fclose(newFile);
                }
                free(buf);

                PRINT(printf("OP: read file \"%s\" sul server:\n\tdimensione lettura: %lu\n", finalPath, getLastOpRSize());
                              if(dir != NULL) printf("\tmemorizzato nella cartella \"%s\"\n\tsotto il nome di: \"%s\"\n", dir, basename(token));)
                token = strtok_r(NULL, ",", &save);
            }
            break;
        case 'R':// readNFiles

            if(arg != NULL) totFile = strtol(arg, NULL, 10);

            dummyRet = readNFiles(totFile, dir);
            GESTERRI(dummyRet == -1, perror("R:readNFiles"); return;)

            PRINT(printf("OP: read %d file sul server:\n\tnumero file realmente letti: %d\n\tdimensione totale lettura: %lu\n", totFile, dummyRet, getLastOpRSize());
                          if(dir != NULL) printf("\tfile memorizzati nella cartella \"%s\"\n", dir);)
            break;
        case 'l':
            token = strtok_r(arg, ",", &save);
            while(token != NULL && strcmp(token, "") != 0){

                GESTERRP(finalPath = realpath(token, finalPath), fprintf(stderr, "\"%s\" non esiste\n", token); token = strtok_r(NULL, ",", &save); continue;)

                dummyRet = lockFile(finalPath);
                GESTERRI(dummyRet == -1, fprintf(stderr,"l:lockFile(\"%s\"):%s\n", finalPath,strerror(errno));token = strtok_r(NULL, ",", &save); continue;)

                PRINT(printf("OP: lock file \"%s\" sul server\n", finalPath);)
                token = strtok_r(NULL, ",", &save);
            }
            break;
        case 'u':
            token = strtok_r(arg, ",", &save);
            while(token != NULL && strcmp(token, "") != 0){

                GESTERRP(finalPath = realpath(token, finalPath), fprintf(stderr, "\"%s\" non esiste\n", token); token = strtok_r(NULL, ",", &save); continue;)

                dummyRet = unlockFile(finalPath);
                GESTERRI(dummyRet == -1, fprintf(stderr,"u:unlockFile(\"%s\"):%s\n", finalPath,strerror(errno));token = strtok_r(NULL, ",", &save); continue;)

                PRINT(printf("OP: unlock file \"%s\" sul server\n", finalPath);)
                token = strtok_r(NULL, ",", &save);
            }
            break;
        case 'c':
            token = strtok_r(arg, ",", &save);
            while(token != NULL && strcmp(token, "") != 0){

                GESTERRP(finalPath = realpath(token, finalPath), fprintf(stderr, "\"%s\" non esiste\n", token); token = strtok_r(NULL, ",", &save); continue;)

                dummyRet = removeFile(finalPath);
                GESTERRI(dummyRet == -1, fprintf(stderr,"c:removeFile(\"%s\"):%s\n", finalPath,strerror(errno));token = strtok_r(NULL, ",", &save); continue;)

                PRINT(printf("OP: remove file \"%s\" sul server\n", finalPath);)
                token = strtok_r(NULL, ",", &save);
            }
            break;
        default: return;
    }
    free(finalPath);
}
/**
 *   @brief Funzione main che legge gli input e esegue
 */
int main (int argc, char * argv[]){

    //variabiline utili
    char opt;
    int dummyRet;
    int wFlag = 0;
    int rFlag = 0;

    //lista comandi
    lst = malloc(sizeof(list));
    GESTERRP(lst, perror("malloc failed"); exit(EXIT_FAILURE);)
    lst->head = NULL;
    lst->tail = NULL;
    lst->size = 0;

    //creo la lista dei comandi da eseguire
    while ((opt = (char)getopt(argc,argv,"hpf:w:W:u:l:D:r:R:d:t:c:")) != -1){
        switch (opt){
            //lettura comandi
            case 'h':
                printf("Opzioni accettate:\n-h\n-f filename (necessario)\n-w dirname[,n=0]\n-W file1[,file2]\n-D dirname (solo se presenti -w o -W)\n-r file1[,file2]\n-R [n=0]\n"
                       "-d dirname (solo se presenti -r o -R)\n-t time\n-l file1[,file2]\n-u file1[,file2]\n-c file1[,file2]\n-p\n");
                freeAndExit();
                return 0;//CHIUSURA PROGRAMMA

            case 'p': pFlag++; wFlag = 0; rFlag = 0;break;
            case 'f':
                GESTERRI(fFlag != 0, printf("ERRORE COMANDO: l'opzione -f non deve essere ripetuta\n"); freeAndExit(); return 0;)//CHIUSURA PROGRAMMA
                fFlag = 1;
                fArg = malloc(sizeof(char) * (strlen(optarg) + 1));
                strcpy(fArg, optarg);
                wFlag = 0; rFlag = 0;
                break;
            case 't':
                GESTERRI(tFlag != 0, printf("ERRORE COMANDO: l'opzione -t non deve essere ripetuta\n"); freeAndExit(); return 0;)//CHIUSURA PROGRAMMA
                tFlag = 1;
                tms = strtol(optarg , NULL, 10);
                wFlag = 0; rFlag = 0;
                break;
            case 'd':
                GESTERRI(rFlag != 1, printf("ERRORE COMANDO: l'opzione -d deve succedere ad una read (-r || -R)\n"); freeAndExit(); return 0;)
                lst->tail->dir = malloc(sizeof(char) * (strlen(optarg) + 1));
                strcpy(lst->tail->dir, optarg);
                wFlag = 0; rFlag = 0;
                break;

            case 'D':
                GESTERRI(wFlag != 1, printf("ERRORE COMANDO: l'opzione -d deve succedere ad una write (-w || -W)\n"); freeAndExit(); return 0;)

                lst->tail->dir = malloc(sizeof(char) * (strlen(optarg) + 1));
                strcpy(lst->tail->dir, optarg);
                wFlag = 0; rFlag = 0;
                break;
            case 'w':
                listAddTail('w', optarg);
                wFlag = 1;
                rFlag = 0;
                break;
            case 'W':
                listAddTail('W', optarg);
                wFlag = 1;
                rFlag = 0;
                break;
            case 'r':
                listAddTail('r', optarg);
                wFlag = 0;
                rFlag = 1;
                break;
            case 'R':
                listAddTail('R', optarg);
                wFlag = 0;
                rFlag = 1;
                break;
            case 'l': listAddTail('l', optarg); wFlag = 0; rFlag = 0; break;
            case 'u': listAddTail('u', optarg); wFlag = 0; rFlag = 0; break;
            case 'c': listAddTail('c', optarg); wFlag = 0; rFlag = 0; break;
            case '?': // comando non previsto, stampa un avviso automaticamente
                if(optopt == 'R'){
                    listAddTail('R', "0");
                    wFlag = 0;
                    rFlag = 1;
                    break;
                }
                else {
                    printf("Consiglio: eseguire il client con il comando -h per la lista di comandi accettati\n");
                    freeAndExit();
                }
                return 0;
            default: freeAndFail(); return -1;
        }
    }

    //inizio invio comandi
    if(!fFlag){
        printf("ERRORE: comando -f necessario\n");
        freeAndExit();
        return 0;
    }
    else{
        PRINT(printf("Attivate stampe standard output\n\n");if(pFlag > 1) printf("Il comando p è stato inserito %d volte! ne bastava solo uno -_-\n", pFlag);)
        PRINT(printf("Nome socket: \"%s\"\n", fArg);)
        PRINT(printf("Tempo di attesa tra 2 richieste successive: %d(ms)\n\n", tms);)
        setSleep(tms);//settiamo il tempo di attesa minimo tra l'esecuzione di 2 funzioni della api
        PRINT(printf("Inizio invio richieste al server...\n\n");)

        struct timespec ts;
        clock_gettime(CLOCK_REALTIME,&ts);
        ts.tv_sec = ts.tv_sec + 20; //20 secondi massimi per i tentativi di connessione
        dummyRet = openConnection(fArg,750,ts);//750 ms tra un tentativo e l'altro

        GESTERRI(dummyRet == -1, perror("connessione al server"); freeAndExit(); return 0;)
        PRINT(printf("OP: connessione riuscita all'indirizzo: \"%s\"\n", fArg);)
    }

    //scorriamo la lista e eseguiamo i comandi
    node* curr = lst->head;
    while(curr != NULL){
        exec(curr->cmd, curr->arg, curr->dir);
        curr = curr->next;
    }

    //terminate tutte le richieste chiudiamo la connessione con il server
    dummyRet = closeConnection(fArg);
    GESTERRI(dummyRet == -1, perror("terminazione connessione\n"); freeAndExit(); return 0;)
    PRINT(printf("OP: connessione chiusa all'indirizzo: \"%s\"\n", fArg);)


    PRINT(printf("\nesecuzione terminata\nrichieste inviate al server: %lu\n\n", getTotOp());)

    freeAndExit();
    return 0;
}