#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define MAXFILENAMELENGHT 108 //lunghezza massima dei path dei file
#define MSG_SIZE 2048   // dimensione di alcuni messaggi scambiati tra server e API
#define MAXTEXTLENGHT 4096 // grandezza massima del contenuto di un file: 1 KB

//gestione errore semplificata
#define GESTERRP(a, b) if((a) == NULL){b}
#define GESTERRI(a, b) if((a)){b}
#define AGGMAX(a, b) if((a) > (b)) (b) = (a);

//typedef funzione hash
typedef  unsigned long  int  ub4;   /* unsigned 4-byte quantities */
typedef  unsigned       char ub1;   /* unsigned 1-byte quantities */
static ub4 bernstein(ub1 *key, ub4 len)
{
    ub4 hash = 5381;
    ub4 i;
    //for (i=0; i<len; ++i) hash = 33*hash + key[i];
    for (i=0; i<len; ++i) hash = ((hash << 5) + hash) + key[i];
    return hash;
}
//funzione hash, prende una stringa e restituisce un numero compreso tra 0 e N scelto
//utilizzare solo char* sicuri
static long long hashFun(char* s, size_t N){

    ub4 valore = bernstein((ub1*)s, strnlen(s, MAXFILENAMELENGHT));
    long long ret = valore % N;
    return ret;
}

//struct per la coda per la gestione della priorità di eliminazione
typedef struct sff {
    char *path; //path del file
    struct sff *next;
    struct sff *prec;
} fPriorityNode;
typedef struct sf {
    fPriorityNode *head;
    fPriorityNode *tail;
} priorityList;

//struct per la gestione dei file
typedef struct sfi {
    char *path;//path del file
    char *text;//contenuto testuale del file
    size_t lockOwner;//client che detiene il file
    size_t open;//open flag 0->false
    struct sfi *next;
    struct sfi *prec;
    fPriorityNode *fPointer;//nodo associato alla lista di priorità per la espulsione
} file;
typedef struct sfl {
    file *head;
    file *tail;
    size_t size;
    pthread_mutex_t mtx;
} fileList;

//tabella hash
typedef struct sh {
    fileList **lists;//array
    size_t size;//dimensione
} hash;

//VARIABILI GLOBALI
/*
 * 0 -> FIFO
 * 1 -> LRU
*/
static size_t politic = 0;


static size_t maxStorageSize;    // dimensione massima dello storage (solo il contenuto dei file)
static size_t maxNFile;      // numero massimo di files nello storage

static size_t nThread;    // numero di thread worker del server

static hash *storage = NULL;    // tabella hash in cui saranno raccolti i files del server
//non necessita di una mutex

static priorityList* priorityQ = NULL;      //coda per la gestione della priorità di rimpiazzamento dei file
pthread_mutex_t pqMutex = PTHREAD_MUTEX_INITIALIZER; // mutex coda priorityList
//questa cosa di usa e si locka solo dentro una lock dei file, MAI il contrario

//VARIABILI STAT
pthread_mutex_t sMutex = PTHREAD_MUTEX_INITIALIZER; // mutex per tutte le variabili stat

//variabili su numero di file e cose
static size_t currentStorageSize = 0;   // dimensione attuale dello storage
static size_t maxStorageSizeReach = 0;    //dimensione massima dello storage raggiunta in Byte
static size_t currentNFile = 0;     // numero attuale di files nello storage
static size_t maxNFileReach = 0;      //numero massimo di file raggiunto

static void fifoNodeFree(fPriorityNode *n) {
    GESTERRP(n, return;)
    free(n->path);
    free(n);
    n = NULL;
}

static void fileFree(file *f) {
    GESTERRP(f, return;)
    free(f->path);
    free(f->text);
    free(f);
    f = NULL;
}

static void freeFifo(priorityList *lst) {
    GESTERRP(lst, errno = EINVAL; return;)

    fPriorityNode *tmp = lst->head;

    while (tmp != NULL) {
        lst->head = lst->head->next;
        fifoNodeFree(tmp);
        tmp = lst->head;
    }

    lst->tail = NULL;
    free(lst);
    lst = NULL;
}

static void freeFileList(fileList *lst) {
    GESTERRP(lst, errno = EINVAL; return;)

    file *dummy = lst->head;
    while (dummy != NULL) {
        lst->head = lst->head->next;
        fileFree(dummy);
        dummy = lst->head;
    }

    lst->tail = NULL;

    free(lst);
    lst = NULL;
}
static file *createFile(char *path, char *text, size_t lockOwner) {
    GESTERRP(path, errno = EINVAL; return NULL;)
    GESTERRP(text, errno = EINVAL; return NULL;)

    size_t pathLenght = strnlen(path, (MAXFILENAMELENGHT));//limitazione della lettura del buffer
    size_t textLenght = strnlen(text, (MAXTEXTLENGHT));

    GESTERRI(pathLenght >= MAXFILENAMELENGHT, errno = ENAMETOOLONG; return NULL;)
    GESTERRI(textLenght >= MAXTEXTLENGHT, errno = ENAMETOOLONG; return NULL;)

    file *dummy = malloc(sizeof(file));
    GESTERRP(dummy, errno = ENOMEM; perror("malloc failed");return NULL;)


    dummy->path = malloc(sizeof(char) * MAXFILENAMELENGHT);
    GESTERRP(dummy->path, free(dummy); errno = ENOMEM;perror("malloc failed"); return NULL;)
    strcpy(dummy->path, path);

    dummy->text = malloc(sizeof(char) * MAXTEXTLENGHT);
    GESTERRP(dummy->text, free(dummy->path); free(dummy); errno = ENOMEM;perror("malloc failed"); return NULL;)
    strcpy(dummy->text, text);

    dummy->lockOwner = lockOwner;
    dummy->open = 0;//chiuso
    GESTERRP(dummy->openerList, free(dummy->text); free(dummy->path); free(dummy); return NULL;)

    dummy->next = NULL;
    dummy->prec = NULL;
    dummy->fPointer = NULL;

    return dummy;
}

static file *copyFile(file *f) {
    GESTERRP(f, errno = EINVAL; return NULL;)

    file *copy = malloc(sizeof(file));
    GESTERRP(copy, errno = ENOMEM; perror("malloc failed");return NULL;)

    copy->path = malloc(sizeof(char) * MAXFILENAMELENGHT);
    GESTERRP(copy->path, free(copy); errno = ENOMEM; perror("malloc failed");return NULL;)
    strcpy(copy->path, f->path);

    copy->text = malloc(sizeof(char) * MAXTEXTLENGHT);
    GESTERRP(copy->path, free(copy->path); free(copy); errno = ENOMEM;perror("malloc failed"); return NULL;)
    strcpy(copy->text, f->text);

    copy->next = NULL;
    copy->prec = NULL;
    copy->openerList = NULL;
    copy->lockOwner = f->lockOwner;

    return copy;
}
static fPriorityNode *createFPriorityNode(char *path) {
    GESTERRI(strlen(path) >= MAXFILENAMELENGHT, errno = EINVAL; return NULL;)

    fPriorityNode *dummy = malloc(sizeof(fPriorityNode));
    GESTERRP(dummy, errno = ENOMEM; perror("malloc failed"); return NULL;)

    dummy->path = malloc(sizeof(char) * MAXFILENAMELENGHT);
    GESTERRP(dummy->path, free(dummy); errno = ENOMEM; perror("malloc failed"); return NULL;)

    strcpy(dummy->path, path);
    dummy->next = NULL;
    dummy->prec = NULL;

    return dummy;
}
static priorityList *createPriorityList() {
    priorityList *dummy = malloc(sizeof(priorityList));
    GESTERRP(dummy, errno = ENOMEM; perror("malloc failed"); return NULL;)

    dummy->head = NULL;
    dummy->tail = NULL;

    return dummy;
}
static int pListAddHead(priorityList *lst, fPriorityNode *f) {
    lock(&pqMutex);

    GESTERRP(lst, errno = EINVAL; unlock(&pqMutex); return -1;)
    GESTERRP(f, errno = EINVAL; unlock(&pqMutex); return -1;)

    if (lst->head == NULL) {
        lst->tail = f;
    } else {
        lst->head->prec = f;
        f->next = lst->head;
    }
    lst->head = f;

    unlock(&pqMutex);
    return 1;
}
static int pListRemove(priorityList *lst, char *path) {
    lock(&pqMutex);

    GESTERRP(lst, errno = EINVAL; unlock(&pqMutex); return -1;)
    GESTERRP(path, errno = EINVAL; unlock(&pqMutex); return -1;)
    GESTERRP(lst->head, unlock(&pqMutex); return 0;)//non è un errore, file non trovato

    fPriorityNode *curr = lst->head;

    //concettualmente uguale alla precedente remove
    if (strncmp(path, lst->head->path, MAXFILENAMELENGHT) == 0) {
        lst->head = lst->head->next;

        if (lst->head != NULL) {
            lst->head->prec = NULL;
        } else {
            lst->tail = NULL;
        }

        fifoNodeFree(curr);
        unlock(&pqMutex);
        return 1;
    }

    curr = lst->head->next;
    while (curr != NULL) {
        if (strncmp(curr->path, path, MAXFILENAMELENGHT) == 0) {

            if (strncmp(lst->tail->path, path, MAXFILENAMELENGHT) == 0) {
                lst->tail->prec->next = NULL;
                lst->tail = lst->tail->prec;
            } else {
                curr->prec->next = curr->next;
                curr->next->prec = curr->prec;
            }

            fifoNodeFree(curr);
            unlock(&pqMutex);
            return 1;
        }
        curr = curr->next;
    }

    unlock(&pqMutex);
    return 0;
}

static int pListLRU(fPriorityNode *node) {
    lock(&pqMutex);

    GESTERRP(priorityQ, errno = EINVAL; perror("pListLRU: priorityQ = NULL"); unlock(&pqMutex); return -1;)
    GESTERRP(node, errno = EINVAL;  unlock(&pqMutex); return -1;)
    GESTERRP(priorityQ->head, unlock(&pqMutex); return -1;)

    if(priorityQ->head == node) return 1;
    if(priorityQ->tail == node){
        if(priorityQ->head == node) return 1;
        else{
            priorityQ->tail = priorityQ->tail->prec;
            priorityQ->tail->next = NULL;
        }
    }
    else{
        node->prec->next = node->next;
        node->next->prec = node->prec;
    }

    node->prec = NULL;
    node->next = priorityQ->head;
    priorityQ->head->prec = node;
    priorityQ->head = node;

    /*if (priorityQ->tail == node) {
        priorityQ->tail->prec->next = NULL;
        priorityQ->tail = priorityQ->tail->prec;
    } else {
        node->next->prec = node->prec;
        node->prec->next = node->next;
    }

    priorityQ->head->prec = node;
    node->next = priorityQ->head;
    priorityQ->head = node;*/

    unlock(&pqMutex);
    return 1;
}


static fileList *createFileList() {

    fileList *dummy = malloc(sizeof(fileList));
    GESTERRP(dummy, errno = ENOMEM; perror("malloc failed"); return NULL;)

    //inizializzo tutti i valori
    dummy->head = NULL;
    dummy->tail = NULL;
    dummy->size = 0;
    dummy->mtx = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;

    return dummy;
}
 void printFileList(fileList *lst) {

    if (lst == NULL) printf("NULL\n");
    else {
        printf("%lu----->", lst->size);

        file *curr = lst->head;

        while (curr != NULL) {
            printf("%s//", curr->path);
            curr = curr->next;
        }
    }
    printf("END\n");
}

static file *fileListGetFile(fileList *lst, char *path) {
    GESTERRP(lst, errno = EINVAL; return NULL;)
    GESTERRP(path, errno = EINVAL; return NULL;)

    file *curr = lst->head;

    while (curr != NULL) {

        if (strncmp(curr->path, path, MAXFILENAMELENGHT) == 0) return curr;
        curr = curr->next;
    }

    return NULL;
}

static int fileListContains(fileList *lst, char *path) {
    GESTERRP(lst, errno = EINVAL; return -1;)
    GESTERRP(path, errno = EINVAL; return -1;)

    file *curr = lst->head;
    while (curr != NULL) {
        if (strncmp(curr->path, path, MAXFILENAMELENGHT) == 0) return 1;
        curr = curr->next;
    }

    return 0;
}
static int fileListAddH(fileList *lst, file *f) {

    GESTERRP( lst, errno = EINVAL; return -1;)
    GESTERRP( f, errno = EINVAL; return -1;)

    int presence = fileListContains(lst, f->path);

    if (presence == -1) return -1;
    if (presence == 1) return 0;

    if (lst->head == NULL) {
        lst->head = f;
        lst->tail = f;
    } else {
        lst->head->prec = f;
        f->next = lst->head;
        lst->head = f;
    }

    lst->size++;
    return 1;
}
static file *fileListRemove(fileList *lst, char *path) {
    GESTERRP(lst, errno = EINVAL; return NULL;)
    GESTERRP(path, errno = EINVAL; return NULL;)

    GESTERRP(lst->head, return NULL;)//non è propriamente un errore

    file *curr = lst->head;

    //terza funzione coc comportamento simile
    if (strncmp(lst->head->path, path, MAXFILENAMELENGHT) == 0) {
        lst->head = lst->head->next;

        if (lst->head != NULL) lst->head->prec = NULL;
        else lst->tail = NULL;

        curr->next = NULL;

        file *out = copyFile(curr);
        fileFree(curr);
        lst->size--;
        return out;
    }

    curr = lst->head->next;

    while (curr != NULL) {
        if (strncmp(curr->path, path, MAXFILENAMELENGHT) == 0) {
            if (strncmp(lst->tail->path, path, MAXFILENAMELENGHT) == 0) {
                lst->tail->prec->next = NULL;
                lst->tail = lst->tail->prec;
            } else {
                curr->next->prec = curr->prec;
                curr->prec->next = curr->next;
            }
            file *out = copyFile(curr);
            fileFree(curr);
            lst->size--;
            return out;
        }
        else curr = curr->next;
    }

    return NULL;
}
static hash *createHash(size_t size) {
    GESTERRI(size == 0, errno = EINVAL; return NULL;)

    hash *dummy = malloc(sizeof(hash));
    GESTERRP(dummy, errno = ENOMEM; perror("malloc failed"); return NULL;)

    dummy->lists = malloc(sizeof(fileList *) * size);
    GESTERRP(dummy->lists, errno = ENOMEM; perror("malloc failed"); free(dummy); return NULL;)

    dummy->size = size;

    int i;
    for (i = 0; i < size; i++) {
        dummy->lists[i] = createFileList();//creo tutte le liste necessarie nell'array

        //errore nella creazione della hash table
        //de alloco tutta la memoria prima di ritornare al chiamante
        if (dummy->lists[i] == NULL) {

            while (i >= 0) {
                freeFileList(dummy->lists[i]);
                i--;
            }

            free(dummy->lists);
            free(dummy);
            errno = ENOMEM;
            return NULL;//errore
        }
    }

    return dummy;
}
static int hashTableAdd(hash *ht, file *f) {
    GESTERRP(ht, errno = EINVAL; return -1;)
    GESTERRP(f, errno = EINVAL; return -1;)

    size_t pos = hashFun(f->path, ht->size);
    int agg = fileListAddH(ht->lists[pos], f);

    if (agg == 1) {
        //aggiorno stat
        lock(&sMutex);
        currentNFile++;
        currentStorageSize = currentStorageSize + strnlen(f->text, MAXTEXTLENGHT);

        AGGMAX(currentStorageSize, maxStorageSizeReach)

        unlock(&sMutex);

        fPriorityNode *filePathPL = createFPriorityNode(f->path);
        f->fPointer = filePathPL;
        int aggPL = pListAddHead(priorityQ, filePathPL);

        if (aggPL == 1) {
            return 1;
        }
        else{
            errno = -1;
            return -1;
        }//errore non previsto
    }

    if(agg == 0) return 0;
    else{
        return -1;
    }
}
static file *hashTableRemovePath(hash *ht, char *path) {
    GESTERRP(ht, errno = EINVAL; return NULL;)
    GESTERRP(path, errno = EINVAL; return NULL;)

    size_t pos = hashFun(path, ht->size);

    file *rim = fileListRemove(ht->lists[pos], path);
    if(rim != NULL){
        lock(&sMutex);
        currentNFile--;
        currentStorageSize = currentStorageSize - strnlen(rim->text, MAXTEXTLENGHT);
        unlock(&sMutex);

        int rimPL = pListRemove(priorityQ, rim->path);
        if(rimPL == 1){
            return rim;
        }
        else{//errore imprevisto
            errno = -1;
            return NULL;
        }
    }
    else {//non trovato
        return NULL;
    }
}

static file *hashTableGetFile(hash *ht, char *path) {
    GESTERRP(ht, errno = EINVAL; return NULL;)
    GESTERRP(path, errno = EINVAL; return NULL;)//serve? non credo proprio

    size_t pos = hashFun(path, ht->size);

    return fileListGetFile(ht->lists[pos], path);
}

