#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <assert.h>
#include <limits.h>

//gestione errore semplificata
#define GESTERRP(a, b) if((a) == NULL){b}
#define GESTERRI(a, b) if((a)){b}
#define AGGMAX(a, b) if((a) > (b)) (b) = (a);
#define POLITIC(a) if(politic == 1) pListLRU((a)); else if(politic == 2) ;
#define PRINT(a) if(print > 0){a}

//tutte le scritture e letture nel socket per comunicare con il client portano allo stesso risultato se fallite
//il trattare il client come disconnesso
#define GESTERRPIPE(a,b)                                                                                                            \
    if(a){                                                                                                                          \
        perror((b));                                                                                                                \
        *endFlag = 1;                                                                                                               \
        hashResetLock(storage, clientId);                                                                                           \
        safeLock(&logMutex);                                                                                                        \
        fprintf(logF, "[%lu]<%d/2/1>\n", pthread_self(), clientId);                                                                 \
        safeUnlock(&logMutex);                                                                                                      \
                                                                                                                                    \
        GESTERRI(write(pipeFD, &clientId, sizeof(clientId)) == -1,                                                                  \
                 errno = EPIPE; perror((b)); exit(EXIT_FAILURE);)                                                                   \
        GESTERRI(write(pipeFD, endFlag, sizeof(*endFlag)) == -1,                                                                    \
                 errno = EPIPE; perror((b)); exit(EXIT_FAILURE);)                                                                   \
        return 1;}

#define MAXFILENAMELENGHT 108 //lunghezza massima dei path dei file
#define STANDARDMSGSIZE 1024   // dimensione messaggio standard tra api e server
#define SMALLMSGSIZE 128 // dimensione messaggio small tra api e server
#define BIGMSGSIZE 10240 //dimensione messaggio big tra api e server
#define MAXTEXTLENGHT maxText // grandezza massima del contenuto di un file
#define SOCKET "./ssocket.sk"  // nome di default per il socket
#define LOGNAME "./log.txt"    // nome di default per il file di log

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
/**
 * @brief funzione che presa una stringa e un size_t restituisce un intero associato alla stringa < di size_t
 * @param s stringa
 * @param N limite superiore
 * @return il numero minore di N associato alla stringa,
 */
//utilizzare solo char* sicuri
static long long hashFun(char* s, size_t N){

    ub4 valore = bernstein((ub1*)s, strnlen(s, MAXFILENAMELENGHT));
    long long ret = valore % N;
    return ret;
}
//struct per lista di client
typedef struct sn {
    int clientId;//descrittore client
    struct sn *next;
    struct sn *prec;
} clientNode;
typedef struct sl {
    clientNode *head;
    clientNode *tail;
} clientList;

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
    clientList *openerList;//lista di client che lo hanno aperto
    int lockOwner;//client che detiene il file
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

/* LOG:
 * open_Connection : [Thrd_id]<clientId/1>
 * close_Connection : [Thrd_id]<clientId/2/err>
 *      err = 0 se la chiusura della connessione è richiesta dal client
 *      err = 1 se la chiusura della connessione è inaspettata
 * open_File : [ThreadID]<clientId/3/ret/errno/O_CREATE/O_LOCK/"filePath">
 * read_File : [Thrd_id]<clientId/4/ret/errno/"filePath"/readSize>
 * read_NFiles : [Thrd_id]<clientId/5/0/fileInviati>
 * write_File : [Thrd_id]<clientId/6/ret/errno/"filePath"/listSize>
 * append_to_File : [Thrd_id]<clientId/7/ret/errno/"filePath"/listSize> //non utilizzata
 * lock_File : [Thrd_id]<clientId/8/ret/errno/"filePath">
 * unlock_File : [Thrd_id]<clientId/9/ret/errno/"filePath">
 * close_File : [ThreadID]<clientId/10/ret/errno/"filePath">
 * remove_File : [Thrd_id]<clientId/11/ret/errno/"filePath">
 */
FILE *logF; // puntatore al file di log
pthread_mutex_t logMutex = PTHREAD_MUTEX_INITIALIZER; // mutex per mutua esclusione sul file di log

//VARIABILI GLOBALI
/*
 * 0 -> FIFO
 * 1 -> LRU
*/
static int politic = 0;
static int print = 1; //1->stampe attivate, 0->stampe disattivate

static size_t maxStorageSize;    // dimensione massima dello storage (solo il contenuto dei file)
static size_t maxNFile;      // numero massimo di files nello storage
char socketName[MAXFILENAMELENGHT]; // nome socket
static size_t nThread;    // numero di thread worker del server
static long maxText;    //lunghezza massima testo file

static hash *storage = NULL;    // tabella hash in cui saranno raccolti i files del server
//non necessita di una mutex

static priorityList* priorityQ = NULL;      //coda per la gestione della priorità di rimpiazzamento dei file
pthread_mutex_t pqMutex = PTHREAD_MUTEX_INITIALIZER; // mutex coda priorityList
//questa mutex si locka solo dentro (dopo) una lock dei file, MAI il contrario

static clientList* clientQ = NULL;     // struttura dati di tipo coda FIFO per la comunicazione Master/Worker
pthread_mutex_t cMutex = PTHREAD_MUTEX_INITIALIZER; // mutex per mutua esclusione sugli accessi alla coda
pthread_cond_t condCoda = PTHREAD_COND_INITIALIZER;

//lock e variabile d condizione per simulare le lock sui file e per risparmiare operazioni inutili al server
//le lock sui file rimangono solo fino a quando un client è connesso quindi l'attesa per una lock è finita e sostenibile
pthread_mutex_t lockFileMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t lockFileCondizione = PTHREAD_COND_INITIALIZER;

volatile sig_atomic_t t = 0; // gestione dei segnali, modificata dall'handler e usata nel main
//volatile perchè è necessario

//VARIABILI STAT
pthread_mutex_t sMutex = PTHREAD_MUTEX_INITIALIZER; // mutex per tutte le variabili stat

//variabili su numero di file e cose
static size_t currentStorageSize = 0;   // dimensione attuale dello storage
static size_t maxStorageSizeReach = 0;    //dimensione massima dello storage raggiunta in Byte
static size_t currentNFile = 0;     // numero attuale di files nello storage
static size_t maxNFileReach = 0;      //numero massimo di file raggiunto

//variabili funzioni api
static size_t replaceSucc = 0;   //numero di replace terminate con successo
static size_t replaceAtt = 0;   //numero di replace iniziate
static size_t readSucc = 0;  //read terminate con successo
static size_t totalReadSize = 0;  //dimensione totale delle letture terminate con successo
static size_t writeSucc = 0; //write terminate con successo
static size_t totalWriteSize = 0; //dimensione totale delle scritture terminate con successo
static size_t lockSucc = 0; //lock terminate con successo
static size_t unlockSucc = 0; //numero di operazioni unlock terminate con successo
static size_t openSucc = 0; //open con flag O_LOCK attivo terminate con successo
static size_t closeSucc = 0; //close avvenute con successo
static size_t maxConnReach = 0;   //numero massimo di connessioni contemporanee raggiunto
static size_t currentConn = 0;   //numero di connessioni attuali
static size_t removeSucc = 0;   //numero di removeFile

//LOCK E UNLOCK CON GESTIONE ERRORI
static void safeLock(pthread_mutex_t *mtx) {
    int err;
    GESTERRI(err = pthread_mutex_lock(mtx) != 0, errno = err; perror("lock error"); pthread_exit((void *) errno);)
}
static void safeUnlock(pthread_mutex_t *mtx) {
    int err;
    GESTERRI(err = pthread_mutex_unlock(mtx) != 0, errno = err; perror("unlock error"); pthread_exit((void *) errno);)
}


//FREE DEI NODI DELLE LISTE E DELLE LISTE

/**
 *   @brief Funzione che esegue la free su ogni nodo della lista e sulla lista
 *   @param lst puntatore alla clientList di cui fare la free
 */
//non esegue la free della struct lst
static void freeClientList(clientList *lst) {

    GESTERRP(lst, errno = EINVAL; return;)

    clientNode *dummy = lst->head;

    while (dummy != NULL) {
        lst->head = lst->head->next;
        free(dummy);
        dummy = lst->head;
    }

    lst->head = NULL;
    lst->tail = NULL;
}

/**
 *   @brief Funzione che librea la memoria occupata nello heap di un fifoNode
 *   @param n puntatore al fifoNode
 */
static void freePriorityNode(fPriorityNode *n) {
    GESTERRP(n, return;)
    free(n->path);
    free(n);
    n = NULL;
}
/**
 *   @brief Funzione che esegue la free su ogni nodo della lista e sulla lista
 *   @param lst puntatore alla coda FIFO
 */
static void freePriorityList(priorityList *lst) {
    GESTERRP(lst, errno = EINVAL; return;)

    fPriorityNode *tmp = lst->head;

    while (tmp != NULL) {
        lst->head = lst->head->next;
        freePriorityNode(tmp);
        tmp = lst->head;
    }

    lst->tail = NULL;
    free(lst);
    lst = NULL;
}
/**
 *   @brief Funzione che libera la memoria allocata nello heap da un file
 *   @param f puntatore al file
 */
static void freeFile(file *f) {
    GESTERRP(f, return;)
    freeClientList(f->openerList);
    free(f->openerList);
    free(f->path);
    free(f->text);
    free(f);
    f = NULL;
}
/**
 *   @brief Funzione che esegue la free su ogni nodo della lista e sulla lista
 *   @param lst puntatore alla lista
 */
static void freeFileList(fileList *lst) {
    GESTERRP(lst, errno = EINVAL; return;)

    file *dummy = lst->head;
    while (dummy != NULL) {
        lst->head = lst->head->next;
        freeFile(dummy);
        dummy = lst->head;
    }

    lst->tail = NULL;

    free(lst);
    lst = NULL;
}
/**
 *   @brief funzione che esegue una free su tutti gli elementi della tabella
 *   @param tbl puntatore alla tabella hash
*/
static void freeHashTable(hash *tbl) {
    GESTERRP(tbl, errno = EINVAL; return;)

    size_t i;
    for (i = 0; i < tbl->size; i++) {
        freeFileList(tbl->lists[i]);
    }
    free(tbl->lists);
    free(tbl);
    tbl = NULL;
}

//FUNZIONI PER LA GESTIONE DI LISTE DI CLIENT
/**
 *   @brief Crea un client correttamente inizializzato
 *   @param clientId descrittore della connessione con un client
 *   @return puntatore al clientNode inizializzato, NULL in caso di fallimento
 */
static clientNode* createClientNode(int clientId) {

    GESTERRI(clientId == 0, errno = EINVAL; return NULL;)

    clientNode *tmp = malloc(sizeof(clientNode));
    GESTERRP(tmp, free(tmp); errno = ENOMEM; perror("malloc failed");return NULL;)

    tmp->next = NULL;
    tmp->prec = NULL;
    tmp->clientId = clientId;

    return tmp;
}
/**
 *   @brief Funzione che inizializza una lista di clientNode
 *   @return puntatore alla clientList inizializzata, NULL in caso di fallimento
 */
static clientList* createClientList() {
    clientList *dummy = malloc(sizeof(clientList));
    GESTERRP(dummy, errno = ENOMEM; free(dummy);perror("malloc failed");return NULL;)

    dummy->head = NULL;
    dummy->tail = NULL;

    return dummy;
}
/**
 *   @brief Funzione che verifica la presenza di un client in una clientList
 *   @param lst  puntatore alla clientList
 *   @param clientId  descrittore della connessione con un client
 *   @return true = 1, 0 = false, -1 se genera errore
 */
static int clientListContains(clientList *lst, int clientId) {
    GESTERRP(lst, errno = EINVAL; return -1;)
    GESTERRI(clientId == 0, errno = EINVAL; return -1;)

    clientNode *cn = lst->head;
    while (cn != NULL) {
        if (cn->clientId == clientId) return 1;
        cn = cn->next;
    }

    return 0;
}

/**
 *   @brief Funzione che aggiunge un clientNode in testa ad una clientList
 *   @param lst puntatore alla clientList
 *   @param clientId descrittore della connessione con un client
 *   @return 1 se termina correttamente, -1 se fallisce
 */
static int clientListAddH(clientList *lst, int clientId) {
    GESTERRP(lst, errno = EINVAL; return -1;)
    GESTERRI(clientId == 0, errno = EINVAL; return -1;)

    clientNode *dummy = createClientNode(clientId);
    if (lst->head == NULL) {
        lst->head = dummy;
        lst->tail = dummy;
    } else {
        lst->head->prec = dummy;
        dummy->next = lst->head;
        lst->head = dummy;
    }

    return 1;
}

/**
 *   @brief Funzione chiamata dai thread worker per ottenere il descrittore della connessione con il prossimo client
 *   @param lst  puntatore alla clientList
 *   @return int: il descrittore del file, -2 se fallisce
 */
static int clientListPop(clientList *lst) {
    GESTERRP(lst, errno = EINVAL; return -2;)//-1 è un valore che determina la terminazione del thread

    safeLock(&cMutex);
    while (lst->head == NULL) {
        pthread_cond_wait(&condCoda, &cMutex); // attesa del segnale inviato dal thread main
    }

    int ret = lst->tail->clientId;

    if (lst->head == lst->tail) {
        free(lst->tail);
        lst->tail = NULL;
        lst->head = NULL;
    } else {
        lst->tail = lst->tail->prec;
        free(lst->tail->next);
        lst->tail->next = NULL;
    }

    safeUnlock(&cMutex);
    return ret;
}

/**
 *   @brief Funzione che elimina un client dalla clientList
 *   @param lst  puntatore alla clientList
 *   @param clientId  descrittore del client
 *   @return 1 se termina correttamente, 0 se fallisce la rimozione, -1 se genera un errore
 */
static int clientListRemove(clientList* lst, int clientId) {
    GESTERRP(lst, errno = EINVAL; return -1;)
    GESTERRI(clientId == 0, errno = EINVAL; return -1;)

    GESTERRP(lst->head, return 0;)

    clientNode *curr = lst->head;

    //divido l'eliminazione in testa dal resto, la return termina la funzione
    if (clientId == lst->head->clientId) {
        lst->head = lst->head->next;

        if ( lst->head != NULL ) lst->head->prec = NULL;
        else lst->tail = NULL;

        free(curr);
        return 1;
    }
    curr = lst->head->next;

    while (curr != NULL) {

        if (curr->clientId == clientId) {

            if (lst->tail->clientId == clientId) {
                lst->tail->prec->next = NULL;
                lst->tail = lst->tail->prec;
            } else {
                curr->prec->next = curr->next;
                curr->next->prec = curr->prec;
            }
            free(curr);
            return 1;
        }
        else curr = curr->next;
    }

    return 0;
}

//FUNZIONI PER LA GESTIONE DI FILE
/**
 *   @brief Funzione che inizializza un file
 *   @param path path del file che lo definisce univocamente
 *   @param text contenuto del file testuale
 *   @param lockOwner descrittore del lock owner
 *   @return puntatore al file inizializzato, NULL in caso di errore
 */
static file *createFile(char *path, char *text, int lockOwner) {
    GESTERRP(path, errno = EINVAL; return NULL;)
    GESTERRP(text, errno = EINVAL; return NULL;)

    size_t pathLenght = strnlen(path, (MAXFILENAMELENGHT));//limitazione della lettura del buffer
    size_t textLenght = strnlen(text, (MAXTEXTLENGHT));

    GESTERRI(pathLenght >= MAXFILENAMELENGHT, errno = ENAMETOOLONG; return NULL;)
    GESTERRI(textLenght >= MAXTEXTLENGHT, errno = ENAMETOOLONG; return NULL;)

    file *dummy = malloc(sizeof(file));
    GESTERRP(dummy, errno = ENOMEM; perror("malloc failed"); return NULL;)

    dummy->path = malloc(sizeof(char) * MAXFILENAMELENGHT);
    GESTERRP(dummy->path, free(dummy); errno = ENOMEM;perror("malloc failed"); return NULL;)
    strcpy(dummy->path, path);

    dummy->text = malloc(sizeof(char) * (textLenght + 1));
    GESTERRP(dummy->text, free(dummy->path); free(dummy); errno = ENOMEM;perror("malloc failed"); return NULL;)
    strcpy(dummy->text, text);

    dummy->lockOwner = lockOwner;
    dummy->openerList = createClientList();
    GESTERRP(dummy->openerList, free(dummy->text); free(dummy->path); free(dummy); return NULL;)

    dummy->next = NULL;
    dummy->prec = NULL;
    dummy->fPointer = NULL;

    return dummy;
}
/**
 *   @brief Funzione che restituisce una copia del file in input
 *   @param f puntatore al file
 *   @return puntatore al file copia, NULL se fallisce
 */
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

//    FUNZIONI PER AMMINISTRARE LA POLITICA FIFO DELLA "FIFO* QUEUE"   //
/**
 *   @brief Funzione che crea un nodo per la coda della priorità dei file
 *   @param path path del file associato al nodo
 *   @return puntatore al fPriorityNode, NULL se fallisce
 */
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
/**
 *   @brief Funzione che inizializza correttamente una priorityList
 *   @return puntatore alla lista inizializzata, NULL se fallisce
 */
static priorityList *createPriorityList() {
    priorityList *dummy = malloc(sizeof(priorityList));
    GESTERRP(dummy, errno = ENOMEM; perror("malloc failed"); return NULL;)

    dummy->head = NULL;
    dummy->tail = NULL;

    return dummy;
}


/**
 *   @brief Funzione che aggiunge un file alla lista della priorità
 *   @param lst puntatore alla lista
 *   @param f puntatore alla nodo
 *   @return 1 se termina con successo, -1 se fallisce
 */
static int pListAddHead(priorityList *lst, fPriorityNode *f) {
    safeLock(&pqMutex);

    GESTERRP(lst, errno = EINVAL; safeUnlock(&pqMutex); return -1;)
    GESTERRP(f, errno = EINVAL; safeUnlock(&pqMutex); return -1;)

    if (lst->head == NULL) {
        lst->tail = f;
    } else {
        lst->head->prec = f;
        f->next = lst->head;
    }
    lst->head = f;

    safeUnlock(&pqMutex);
    return 1;
}
/**
 *   @brief Funzione che rimuove un elemento dalla lista della priorità
 *   @param lst puntatore alla lista
 *   @param path path del file da rimuovere
 *   @return 1 se termina con successo, 0 se il file non viene trovato, -1 se genera errore
 */
static int pListRemove(priorityList *lst, char *path) {
    safeLock(&pqMutex);

    GESTERRP(lst, errno = EINVAL; safeUnlock(&pqMutex); return -1;)
    GESTERRP(path, errno = EINVAL; safeUnlock(&pqMutex); return -1;)
    GESTERRP(lst->head, safeUnlock(&pqMutex); return 0;)//non è un errore, file non trovato

    fPriorityNode *curr = lst->head;

    //concettualmente uguale alla precedente remove
    if (strncmp(path, lst->head->path, MAXFILENAMELENGHT) == 0) {
        lst->head = lst->head->next;

        if (lst->head != NULL) {
            lst->head->prec = NULL;
        } else {
            lst->tail = NULL;
        }

        freePriorityNode(curr);
        safeUnlock(&pqMutex);
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

            freePriorityNode(curr);
            safeUnlock(&pqMutex);
            return 1;
        }
        curr = curr->next;
    }

    safeUnlock(&pqMutex);
    return 0;
}

/*///TOGLIERE
static void printPQ (priorityList* lst)

{
    safeLock(&pqMutex);

    if (lst == NULL || lst->head == NULL)
    {
        errno = EINVAL;
        safeUnlock(&pqMutex);
        return;
    }

    printf("\nSTART QUEUE \n");
    fPriorityNode* cursor = lst->head;
    while (cursor != NULL)
    {
        printf("%s||",cursor->path);
        cursor = cursor->next;
    }
    printf("\nEND QUEUE \n");

    safeUnlock(&pqMutex);
}*/

/**
 * @brief funzione che mantiene la politica LRU mettendo in testa un file appena utilizzato
 * @param node elemento della lista da portare in testa nella lista globale
 * @return 1 se termna corettamente, -1 se fallisce
**/
static int pListLRU(fPriorityNode *node) {
    safeLock(&pqMutex);
    GESTERRP(priorityQ, errno = EINVAL; perror("pListLRU: priorityQ = NULL"); safeUnlock(&pqMutex); return -1;)
    GESTERRP(node, errno = EINVAL;  safeUnlock(&pqMutex); return -1;)
    GESTERRP(priorityQ->head, safeUnlock(&pqMutex); return -1;)

    if(priorityQ->head == node){safeUnlock(&pqMutex);return 1;}
    if(priorityQ->tail == node){
        if(priorityQ->head == node){safeUnlock(&pqMutex);return 1;}
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
    safeUnlock(&pqMutex);
    return 1;
}


//FUNZIONI PER LE LISTE DI FILE
/**
 *   @brief Funzione che inizializza una lista di file
 *   @return il puntatore alla lista inizializzata, NULL se fallisce
 */
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

/**
 *   @brief Funzione che stampa una rappresentazione di una lista di files
 *   @param puntatore alla lista
 */
/*//TOGLIERE
static void printFileList(fileList *lst) {

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
}*/

/**
 *   @brief Funzione che restituisce il puntatore ad un file dentro alla lista con il path specificato
 *   @param lst puntatore alla lista
 *   @param path path del file da estrarre
 *   @return puntatore al file, NULL se non trova il file
 */
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

/**
 *   @brief Funzione che determina se un file è presente nella lista passata come parametro
 *   @param lst puntatore alla lista
 *   @param path path del file da cercare
 *   @return 1 se trova il file, 0 se non trova il file, -1 se fallisce
 */
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
/**
 *   @brief Aggiunge un file alla lista se non è già presente
 *   @param lst puntatore alla lista
 *   @param f   puntatore al file
 *   @return 1 se aggiunge il file, 0 se non lo aggiunge, -1 se genera errore
 */
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

/**
 *   @brief Funzione che rimuove un file dalla lista passata in input
 *   @param lst puntatore alla lista
 *   @param path path del file da rimuovere
 *   @return puntatore alla copia del file rimosso, NULL se non lo trova o se genera errore (setta errno)
 */
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
        freeFile(curr);
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
            freeFile(curr);
            lst->size--;
            return out;
        }
        else curr = curr->next;
    }

    return NULL;
}

//FUNZIONI TABELLA HASH DI FILE
/**
 *   @brief Funzione che inizializza correttamente una tabella hash
 *   @param size grandezza della tabella (array di liste)
 *   @return puntatore alla tabella hash creata, NULL se fallisce
 */
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
/**
 *   @brief funzione che aggiuge un file ad una tabella
 *   @param ht puntatore alla tabella hash
 *   @param f puntatore al filen da aggiungere
 *   @return 1 se termina con successo, -1 se fallisce
*/
//se fallisce --> comportamento imprevedibile
//le funzioni della hash table vanno chiamate dopo aver lockato la lista corrispondente all'elemento
static int hashTableAdd(hash *ht, file *f) {
    GESTERRP(ht, errno = EINVAL; return -1;)
    GESTERRP(f, errno = EINVAL; return -1;)

    size_t pos = hashFun(f->path, ht->size);
    int agg = fileListAddH(ht->lists[pos], f);

    if (agg == 1) {
        //aggiorno stat
        safeLock(&sMutex);
        currentNFile++;
        currentStorageSize = currentStorageSize + strnlen(f->text, MAXTEXTLENGHT);

        AGGMAX(currentStorageSize, maxStorageSizeReach)

        safeUnlock(&sMutex);

        fPriorityNode *filePathPL = createFPriorityNode(f->path);
        f->fPointer = filePathPL;
        int aggPL = pListAddHead(priorityQ, filePathPL);

        if (aggPL == 1) {
            return 1;
        }
        else{
            errno = ENOTRECOVERABLE;
            return -1;
        }//errore non previsto
    }

    if(agg == 0) return 0;
    else{
        return -1;
    }
}

/**
 *   @brief Funzione che rimuove un file dalla tabella hash dato il path in input
 *   @param ht puntatore alla tabella
 *   @param path path del file
 *   @return puntatore alla copia del file rimosso, NULL se fallisce, (setta ERRNO)
*/
//se fallisce file non trovato
static file *hashTableRemovePath(hash *ht, char *path) {
    GESTERRP(ht, errno = EINVAL; return NULL;)
    GESTERRP(path, errno = EINVAL; return NULL;)

    size_t pos = hashFun(path, ht->size);
    file *rim = fileListRemove(ht->lists[pos], path);

    if(rim != NULL){
        safeLock(&sMutex);
        currentNFile--;
        currentStorageSize = currentStorageSize - strnlen(rim->text, MAXTEXTLENGHT);
        safeUnlock(&sMutex);

        int rimPL = pListRemove(priorityQ, rim->path);
        if(rimPL == 1){
            return rim;
        }
        else{//errore imprevisto
            errno = ENOTRECOVERABLE;
            perror("??");
            return NULL;
        }
    }
    else {//non trovato
        return NULL;
    }
}
/**
 *   @brief Funzione che restituisce il puntatore ad un file contenuto nella hash table (non una copia!!)
 *   @param ht puntatore alla tabella
 *   @param path path del file
 *   @return puntatore al file, NULL se fallisce o non lo trova
*/
static file *hashTableGetFile(hash *ht, char *path) {
    GESTERRP(ht, errno = EINVAL; return NULL;)
    GESTERRP(path, errno = EINVAL; return NULL;)//serve? non credo proprio

    size_t pos = hashFun(path, ht->size);

    return fileListGetFile(ht->lists[pos], path);
}

/**
 *   @brief funzione che restituisce la lista che corrisponde al file passato in input (non una copia!!)
 *   @param ht puntatore alla tabella hash
 *   @param path path del file
 *   @return puntatore alla lista, NULL se fallisce
*/
static fileList *hashTableGetFList(hash *ht, char *path) {
    GESTERRP(ht, errno = EINVAL; return NULL;)
    GESTERRP(path, errno = EINVAL; return NULL;)

    size_t pos = hashFun(path, ht->size);

    return ht->lists[pos];
}

/**
 *   @brief funzione che stampa la tabella hash ht
 *   @param ht puntatore alla tabella hash
*/
/*///TOGLIERE
static void printHashTable(hash *ht) {
    GESTERRP(ht, printf("NULL\n");)//non è un errore ma fa comodo

    printf("size: %ld\n", ht->size);

    int i;
    for (i = 0; i < ht->size; i++) printFileList(ht->lists[i]);

    printf("END TABLE\n");
}*/

/**
 *   @brief Funzione che controlla se un file è presente nella tabella passata come parametro
 *   @param ht puntatore alla tabella
 *   @param path path del file
 *   @return 1 se il file è presente, 0 se non è presente, -1 se genera errori
*/
static int hashContains(hash *ht, char *path) {
    GESTERRP(ht, errno = EINVAL; return -1;)
    GESTERRP(path, errno = EINVAL; return -1;)

    size_t pos = hashFun(path, ht->size);

    return fileListContains(ht->lists[pos], path);
}
/**
 *   @brief Funzione che esegue la safeLock su tutte le liste della hash table rendendola non modificabile se non da un thread solo
 *      deve essere sempre seguita da una unlockAllH
 *   @param ht puntatore alla tabella
*/
static void lockAllH(hash *ht){
    int i;
    for( i = 0 ; i<ht->size; i++) safeLock(&(ht->lists[i]->mtx));
}
/**
 *   @brief Funzione che esegue la safeUnlock su tutte le liste della hash table, segue sempre una lockAllH
 *   @param ht puntatore alla tabella
*/
static void unlockAllH(hash *ht){
    int i;
    for( i = 0 ; i<ht->size; i++) safeUnlock(&(ht->lists[i]->mtx));
}
/**
 *   @brief funzione che gestisce il rimpiazzamento di file ( se serve )
 *   @param ht puntatore alla tabella hash
 *   @param path path del file che potrebbe aver causato il superamento del limite massimo di memoria
 *   @param clientId descrittore del client che ha causato l'overflow
 *   @return un puntatore a fileList che contiene la lista dei file rimossi per creare spazio, NULL altrimenti
*/
//controlla solo file aggiunti prima del file che ha generato l'overflow, se non riesce a creare abbatstanza spazio ritorna con EFBIG in errno
static fileList* hashReplaceF(hash *ht, char *path, int clientId) {//ENOMEM e ENOTRECOVERABLE e EFBIG
    GESTERRP(ht, errno = EINVAL; return NULL;)
    GESTERRP(path, errno = EINVAL; return NULL;)
    GESTERRI(clientId == 0, errno = EINVAL; return NULL;)

    //lista restituita
    fileList *retList = createFileList();
    GESTERRP(retList, return NULL;)// errno è settato da create list se serve

    lockAllH(ht);//serve così posso usare liberamente la priorityQ senza aver paura di sbagli

    int successo = 0; //l'algoritmo non apporta modifiche ogni volta che è  stato attivato
    safeLock(&sMutex);
    if(currentStorageSize > maxStorageSize) successo = 1;

    fPriorityNode *target = NULL; //punatatore al nodo della coda priorityList che conterrà il path del file potenzialmente rimpiazzabile
    target = priorityQ->tail;

    //se serve l'algoitmo rimpiazza più di un file
    while (currentStorageSize > maxStorageSize) {
        safeUnlock(&sMutex);
        GESTERRP(target,
                  safeUnlock(&sMutex); unlockAllH(ht); fileListAddH(retList, hashTableRemovePath(ht, path));errno = EFBIG; return retList;)
        file *toRemove = hashTableGetFile(storage, target->path);
        // troviamo il file corrispondente al path scelto
        GESTERRP(toRemove, errno = ENOTRECOVERABLE; freeFileList(retList); safeUnlock(&sMutex);unlockAllH(ht); perror("errore sincronizzazione PQ e hashTable"); return NULL;)

        //il file da eliminare non deve essere lockato da nessuno se non da chi ha fatto aviare la replace, e non deve essere
        //il file che ha generato il bisogno di avviare la replace
        while ((toRemove->lockOwner != 0 && toRemove->lockOwner != clientId) ||
                (!strcmp(target->path, path))) {
            target = target->prec;
            GESTERRP(target,  safeUnlock(&sMutex); unlockAllH(ht); fileListAddH(retList, hashTableRemovePath(ht, path));errno = EFBIG;return retList;)
            //il file non viene inserito, ma altri sono stati tolti

            toRemove = hashTableGetFile(storage, target->path);
            GESTERRP(toRemove, errno = ENOTRECOVERABLE; freeFileList(retList);safeUnlock(&sMutex); unlockAllH(ht); perror("errore sincronizzazione PQ e hashTable"); return NULL;)
        }
        target = target->prec;//bisogna aggiornare il puntatore prima di chiamare la remove (che elimina il path dalla pq)

        file *copy = hashTableRemovePath(storage, toRemove->path);
        //non servono lock questa vola perchè già fatte
        GESTERRP(copy,
                 freeFileList(retList); safeUnlock(&sMutex);unlockAllH(ht); errno = ENOTRECOVERABLE; perror("errore sincronizzazione PQ e hashTable"); return NULL;)

        GESTERRI(fileListAddH(retList, copy) == -1, freeFileList(retList); safeUnlock(&sMutex);unlockAllH(ht); return NULL;)

        safeLock(&sMutex);
        replaceSucc++;   //un file tolto dalla replace
    }
    //aggiornamento statistiche
    if (successo == 1) replaceAtt++;
    AGGMAX(currentStorageSize, maxStorageSizeReach)
    safeUnlock(&sMutex);

    //fine senza errori
    unlockAllH(ht);
    errno = 0;
    return retList;
}

/**
 *   @brief funzione che rimuove tutte le lock in file appartenute ad un client dopo la sua disconnessione
 *   @param ht puntatore alla tabella hash
 *   @param clientId descrittore del client
*/
static void hashResetLock(hash *ht, int clientId) {
    GESTERRP(ht, errno = EINVAL; return;)

    int i;
    file *curr;

    for (i = 0; i < ht->size; i++) {
        safeLock(&(ht->lists[i]->mtx));
        curr = ht->lists[i]->head;

        while (curr != NULL) {
            if (curr->lockOwner == clientId) curr->lockOwner = 0;
            curr = curr->next;
        }
        safeUnlock(&(ht->lists[i]->mtx));
    }
    pthread_cond_broadcast(&lockFileCondizione);//consizione per ritentare la lockFile
}

//    FUNZIONI PER IL SERVER   //
/**
 *   @brief Fuznione che controlla se s è un numero, se si inserisce il valore in n
 *   @param numbers puntatore al char di cui fare il controllo
 *   @param n puntatore dove inserire il valore di s se s è un numero
 *   @return 1 se s è un numero, 0 se non lo è, 2 se il numero genera overflow
 */
static int isNumber(const char *numbers, long *n) {
    GESTERRP(numbers, return 0;)
    GESTERRI(strlen(numbers) == 0, return 0;)

    char *e = NULL;
    errno = 0;

    long val = strtol(numbers, &e, 10);
    if (errno == ERANGE) return 2;    // overflow
    if (e != NULL && *e == (char) 0) {
        *n = val;
        return 1;   // successo
    }
    return 0;   // non e' un numero
}
/**
 *   @brief Funzione che permette di effettuare la read completandola correttamente nonostante una ricezione di un segnale
 *   @param fd descrittore della connessione
 *   @param buf puntatore al messaggio da inviare
 *   @return Il numero di Bytes letti, -1 se genera errore
 */
int readn(long fd, void *buf, size_t size) {
    int readn = 0;
    int r = 0;

    while (readn < size) {
        //lettura non terminata
        if ((r = (int) read((int) fd, buf, size)) == -1) {
            if (errno == EINTR)
                //recezione segnale e continuazione
                continue;
            else {
                perror("Readn");
                return -1;
            }
        }
        if (r == 0)
            //Lettura terminata
            return readn;

        readn += r;
    }

    return readn;
}

/**
 *   @brief Funzione che permette di effettuare la write completandola correttamente nonostante una ricezione di un segnale
 *   @param fd descrittore della connessione
 *   @param buf puntatore al messaggio da inviare
 *   @param size numero di dati da scivere in bytes
 *   @return Il numero di bytes scritti, -1 se genera errore
 */
int writen(long fd, const void *buf, size_t size) {
    int writen = 0;
    int w = 0;

    while (writen < size) {
        //scrittura non terminata
        if ((w = (int) write((int) fd, buf, size)) == -1) {
            if (errno == EINTR)
                continue;//continua la scrittura
            else if (errno == EPIPE)
                break;
            else {
                perror("Writen");
                return -1;
            }
        }
        if (w == 0)
            return writen;
            //scrittura terminata
        writen += w;
    }

    return writen;
}
/**
 *   @brief funzione per l'aggiornamento del file descriptor massimo
 *   @param fdmax descrittore massimo attuale
 *   @return file descriptor massimo -> successo, -1 altrimenti
*/
static int max_up(fd_set set, int fdmax) {
    int i;
    for (i = (fdmax - 1); i >= 0; --i)
        if (FD_ISSET(i, &set)) return i;

    assert(1 == 0); //sempre falsa
    return -1;
}

//funzionamento del server
/* openFILE : FLAGS
 * 0 ->O_CREATE = 0 && O_LOCK = 0
 * 1 ->O_CREATE = 0 && O_LOCK = 1
 * 2 ->O_CREATE = 1 && O_LOCK = 0
 * 3 ->O_CREATE = 1 && O_LOCK = 1
 */
/**
 *   @brief comportamento conforme alla funzione della api
 *   @param path il path del file da aprie
 *   @param flags un intero che contiene le flag della funzione
 *   @param clientId l'id del client che richiede l'operazione
 *   @return 0 se l'operazione va a buon fine, -1 se fallisce
*/
static int openFile(char *path, int flags, int clientId) {
    GESTERRP(path, errno = EINVAL; return -1;)
    GESTERRI(flags < 0 || flags > 3, errno = EINVAL; return -1;)

    fileList *dummyL = hashTableGetFList(storage, path);//lock dello storage
    GESTERRP(dummyL, perror("openFile"); return -1;)
    safeLock(&(dummyL->mtx));

    int presenza = hashContains(storage, path);
    GESTERRI(presenza == -1, safeUnlock(&(dummyL->mtx));perror("openFile");  return -1;)//errno settato dalla contains

    switch (flags) {
        case 0 : {

            GESTERRI(presenza == 0, errno = ENOENT; safeUnlock(&(dummyL->mtx)); return -1;)//il file non è presente

            file *dummyF = hashTableGetFile(storage, path);
            GESTERRP(dummyF, safeUnlock(&(dummyL->mtx));perror("openFile");  return -1;)


            if (dummyF->lockOwner == 0 || dummyF->lockOwner == clientId) {//operazione permessa
                if(clientListContains(dummyF->openerList, clientId) == 0)
                    if (clientListAddH(dummyF->openerList, clientId) == -1) {
                        perror("openFile");
                        safeUnlock(&(dummyL->mtx));
                        return -1;
                    }

                safeLock(&sMutex);
                openSucc++;
                safeUnlock(&sMutex);

                //terminata con succeesso
                safeUnlock(&(dummyL->mtx));

                POLITIC(dummyF->fPointer)//funzioni per la attuazione della politica attiva
                return 0;
            }
            else {safeUnlock(&(dummyL->mtx)); errno = EPERM; return -1;}//operazione non permessa
        }

        case 1 : {
            GESTERRI(presenza == 0, errno = ENOENT; safeUnlock(&(dummyL->mtx)); return -1;)//il file non è presente

            file *dummyF = hashTableGetFile(storage, path);
            GESTERRP(dummyF, safeUnlock(&(dummyL->mtx));perror("openFile");  return -1;)

            if (dummyF->lockOwner == 0 || dummyF->lockOwner == clientId) {//successo

                if(clientListContains(dummyF->openerList, clientId) == 0)
                    if (clientListAddH(dummyF->openerList, clientId) == -1) {
                        perror("openFile");
                        safeUnlock(&(dummyL->mtx));
                        return -1;
                    }

                safeLock(&sMutex);
                openSucc++;
                safeUnlock(&sMutex);

                dummyF->lockOwner = clientId;
                safeUnlock(&(dummyL->mtx));

                POLITIC(dummyF->fPointer)
                return 1;
            }
            else { safeUnlock(&(dummyL->mtx)); errno = EPERM; return -1; }//non permesso
        }

        case 2 : {
            GESTERRI(presenza == 1, errno = EEXIST; safeUnlock(&(dummyL->mtx)); return -1;)//il file è presente

            int canBeAdded = 0;

            safeLock(&sMutex);
            if (currentNFile < maxNFile) canBeAdded = 1;
            safeUnlock(&sMutex);

            GESTERRI(canBeAdded == 0,errno = ENFILE; safeUnlock(&(dummyL->mtx)); return -1;)//non permesso

            file *dummyF = createFile(path, "", 0);
            GESTERRP(dummyF, safeUnlock(&(dummyL->mtx)); return -1;)//path troppo lungo, errore settato prima


            if (clientListAddH(dummyF->openerList, clientId) == -1){
                freeFile(dummyF); safeUnlock(&(dummyL->mtx));
                return -1;
            }//non può essere già contenuto

            GESTERRI(hashTableAdd(storage, dummyF) == -1,
                     freeFile(dummyF); safeUnlock(&dummyL->mtx);perror("openFile"); return -1;)

            safeLock(&sMutex);
            AGGMAX(currentNFile, maxNFileReach)
            openSucc++;
            safeUnlock(&sMutex);

            safeUnlock(&dummyL->mtx);
            POLITIC(dummyF->fPointer)
            return 0;
        }

        case 3 : {
            GESTERRI(presenza == 1, errno = EEXIST; safeUnlock(&(dummyL->mtx)); return -1;)//il file è presente

            int canBeAdded = 0;

            safeLock(&sMutex);
            if (currentNFile < maxNFile) canBeAdded = 1;
            safeUnlock(&sMutex);

            GESTERRI(canBeAdded == 0,errno = ENFILE; safeUnlock(&(dummyL->mtx)); return -1;)//non permesso

            file *dummyF = createFile(path, "", clientId);
            GESTERRP(dummyF, safeUnlock(&(dummyL->mtx)); perror("openFile"); return -1;)

            if (clientListAddH(dummyF->openerList, clientId) == -1){
                freeFile(dummyF); safeUnlock(&(dummyL->mtx));
                return -1;
            }//non può essere già contenuto

            GESTERRI(hashTableAdd(storage, dummyF) == -1,
                     freeFile(dummyF); safeUnlock(&dummyL->mtx);perror("openFile"); return -1;)

            safeLock(&sMutex);
            AGGMAX(currentNFile, maxNFileReach)
            openSucc++;
            safeUnlock(&sMutex);


            safeUnlock(&dummyL->mtx);
            POLITIC(dummyF->fPointer)
            return 0;
        }

        default : {
            safeUnlock(&dummyL->mtx);
            errno = EINVAL;
            perror("openFile");
            return -1;
        }
    }
}
/**
 *   @brief comportamento conforme alla funzione della api
 *   @param path il path del file da leggere
 *   @param buf buffer dove restituire il testo letto
 *   @param size dimensione del buffer
 *   @param clientId l'id del client che richiede l'operazione
 *   @return 0 se l'operazione va a buon fine, 0 se fallisce
*/
//il buffer viene inizializzato dentro
static int readFile(char *path, char **buf, size_t *size, int clientId) {

    GESTERRP(path, errno = EINVAL; return -1;)

    fileList *dummyL = hashTableGetFList(storage, path);
    GESTERRP(dummyL, return -1;)

    safeLock(&dummyL->mtx);

    if (hashContains(storage, path)) {

        file *dummyF = hashTableGetFile(storage, path);
        GESTERRP(dummyF, perror("readFile");safeUnlock(&(dummyL->mtx)); return -1;)//serve?

        if ((dummyF->lockOwner == 0 || dummyF->lockOwner == clientId) &&
            clientListContains(dummyF->openerList, clientId)) {
            //se il client ha aperto il file e esso non è locked
            *size = strlen(dummyF->text) + 1;

            *buf = malloc(sizeof(char) * (*size));
            strncpy(*buf, dummyF->text, (*size));


            safeLock(&sMutex);
            readSucc++;
            totalReadSize = totalReadSize + (*size);
            safeUnlock(&sMutex);

            safeUnlock(&dummyL->mtx);

            POLITIC(dummyF->fPointer)

            return 0;
        } else {
            safeUnlock(&(dummyL->mtx));
            errno = EPERM;
            return -1;
        }
    } else {
        errno = ENOENT;
        safeUnlock(&(dummyL->mtx));
        return -1;
    }
}
/**
 *   @brief comportamento conforme alla funzione della api
 *   @param path il path del file in cui scrivere
 *   @param text il testo da scrivere dentro al file
 *   @param clientId l'id del client che richiede l'operazione
 *   @return il puntatore alla lista dei file espulsi, NULL se fallisce o genera errori
*/
static fileList *writeFile(char *path, char *text, int clientId) {
    GESTERRP(path, errno= EINVAL; return NULL;)
    GESTERRP(text, errno = EINVAL; return NULL;)
    fileList *dummyL = hashTableGetFList(storage, path);
    GESTERRP(dummyL, errno = ENOTRECOVERABLE; perror("writeFile");  return NULL;)//errori che non dovrebbero accadere

    safeLock(&(dummyL->mtx));
    file *dummyF = hashTableGetFile(storage, path);
    GESTERRP(dummyF, safeUnlock(&(dummyL->mtx)); errno = ENOENT; return NULL;)

    if(clientListContains(dummyF->openerList, clientId) == 1){
        if (dummyF->lockOwner != clientId) {
            errno = EPERM;
            safeUnlock(&(dummyL->mtx));//permesso negato
            return NULL;
        }
        else{
            size_t lenCurr = strnlen(text, MAXTEXTLENGHT);
            GESTERRI(lenCurr >= MAXTEXTLENGHT,  safeUnlock(&(dummyL->mtx)); errno = EFBIG; return NULL;)//file?? boh

            size_t lenPrec = strlen(dummyF->text);
            free(dummyF->text);

            dummyF->text = malloc(sizeof(char) * (lenCurr + 1));
            GESTERRP(dummyF->text, errno = ENOMEM; perror("write file"); safeUnlock(&(dummyL->mtx)); return NULL;)
            strcpy(dummyF->text, text);

            safeLock(&sMutex);
            currentStorageSize = currentStorageSize + lenCurr - lenPrec;
            writeSucc++;
            totalWriteSize = totalWriteSize + lenCurr;
            safeUnlock(&sMutex);

            safeUnlock(&(dummyL->mtx));
            POLITIC(dummyF->fPointer)
            fileList *out = hashReplaceF(storage, path, clientId);
            int errnoV = errno;
            pthread_cond_broadcast(&lockFileCondizione);//consizione per ritentare la lockFile

            errno = errnoV;
            return out;
        }
    }
    else{//permesso negato
        errno = EPERM;
        safeUnlock(&(dummyL->mtx));
        return NULL;
    }
}
/**
 *   @brief comportamento conforme alla funzione della api
 *   @param path il path del file in cui scrivere
 *   @param text il testo da scrivere dentro al file
 *   @param clientId l'id del client che richiede l'operazione
 *   @return il puntatore alla lista dei file espulsi, NULL se fallisce o genera errori
*/
static fileList *appendToFile(char *path, char *text, int clientId) {
    GESTERRP(path, errno= EINVAL; return NULL;)
    GESTERRP(text, errno = EINVAL; return NULL;)

    fileList *dummyL = hashTableGetFList(storage, path);
    GESTERRP(dummyL, errno = ENOTRECOVERABLE; perror("appendFile");  return NULL;)//errori che non dovrebbero accadere

    safeLock(&(dummyL->mtx));

    file *dummyF = hashTableGetFile(storage, path);
    GESTERRP(dummyF, safeUnlock(&(dummyL->mtx)); errno = ENOENT; return NULL;)

    if(clientListContains(dummyF->openerList, clientId) == 1){
        if (dummyF->lockOwner == 0 || dummyF->lockOwner != clientId) {
            errno = EPERM;
            safeUnlock(&(dummyL->mtx));//permesso negato
            return NULL;
        }
        else{

            size_t lenCurr = strnlen(text, MAXTEXTLENGHT);
            size_t lenPrec = strlen(dummyF->text);

            GESTERRI(lenCurr + lenPrec >= MAXTEXTLENGHT, errno = EFBIG; safeUnlock(&(dummyL->mtx)); return NULL;)//file?? boh

            dummyF->text = realloc(dummyF->text, sizeof(char) * (lenCurr + lenPrec + 1));
            GESTERRP(dummyF->text, errno = ENOMEM; perror("appendFile"); safeUnlock(&(dummyL->mtx)); return NULL;)

            strcat(dummyF->text, text);

            safeLock(&sMutex);
            currentStorageSize = currentStorageSize + lenCurr;
            writeSucc++;
            totalWriteSize = totalWriteSize + lenCurr;
            safeUnlock(&sMutex);

            safeUnlock(&(dummyL->mtx));
            POLITIC(dummyF->fPointer);
            fileList *out = hashReplaceF(storage, path, clientId);

            pthread_cond_broadcast(&lockFileCondizione);//consizione per ritentare la lockFile

            return out;
        }
    }
    else{//permesso negato
        errno = EPERM;
        safeUnlock(&(dummyL->mtx));
        return NULL;
    }
}
/**
 *   @brief comportamento conforme alla funzione della api
 *   @param path il path del file di cui eseguire la lock
 *   @param clientId l'id del client che richiede l'operazione
 *   @return 0 se termina correttamente, -1 se fallisce
*/
static int lockFile(char *path, int clientId) {
    GESTERRP(path, errno = EINVAL; return -1;)

    fileList *dummyL = hashTableGetFList(storage, path);
    GESTERRP(dummyL, errno = ENOTRECOVERABLE; perror("lockFile");  return -1;)
    safeLock(&(dummyL->mtx));

    if(hashContains(storage, path) == 1){
        file *dummyF = hashTableGetFile(storage, path);

        if (dummyF->lockOwner == 0 || dummyF->lockOwner == clientId) {
            dummyF->lockOwner = clientId;

            safeLock(&sMutex);
            lockSucc++;
            safeUnlock(&sMutex);

            safeUnlock(&(dummyL->mtx));
            POLITIC(dummyF->fPointer)
            return 0;
        }
        else{
        	errno = ENOLCK;
            safeUnlock(&(dummyL->mtx));
            return -2; // valore speciale, quando sarà ricevuto dalla do a job verrà fatta una wait
        }
    }
    else{
        safeUnlock(&(dummyL->mtx));
        errno = ENOENT;
        return -1;
    }
}
/**
 *   @brief comportamento conforme alla funzione della api
 *   @param path il path del file di cui eseguire la unlock
 *   @param clientId l'id del client che richiede l'operazione
 *   @return 0 se termina correttamente, -1 se fallisce
*/
static int unlockFile(char *path, int clientId) {
    GESTERRP(path, errno = EINVAL; return -1;)

    fileList *dummyL = hashTableGetFList(storage, path);
    GESTERRP(dummyL, errno = ENOTRECOVERABLE; perror("unlockFile");  return -1;)
    safeLock(&(dummyL->mtx));

    if(hashContains(storage, path) == 1){
        file *dummyF = hashTableGetFile(storage, path);

        if (dummyF->lockOwner == clientId || dummyF->lockOwner == 0) {
            dummyF->lockOwner = 0;

            safeLock(&sMutex);
            unlockSucc++;
            safeUnlock(&sMutex);

            safeUnlock(&(dummyL->mtx));

            POLITIC(dummyF->fPointer)

            pthread_cond_broadcast(&lockFileCondizione);//consizione per ritentare la lockFile
            return 0;
        }
        else{
            safeUnlock(&(dummyL->mtx));
            errno = EPERM;
            return -1;
        }
    }
    else{
        safeUnlock(&(dummyL->mtx));
        errno = ENOENT;
        return -1;
    }
}
/**
 *   @brief comportamento conforme alla funzione della api
 *   @param path il path del file di cui eseguire la unlock
 *   @param clientId l'id del client che richiede l'operazione
 *   @return 0 se termina correttamente, -1 se fallisce
*/
static int closeFile(char *path, int clientId) {
    GESTERRP(path, errno = EINVAL; return -1;)

    fileList *dummyL = hashTableGetFList(storage, path);
    GESTERRP(dummyL, errno = ENOTRECOVERABLE; perror("closeFile");  return -1;)//errori che non dovrebbero accadere
    safeLock(&(dummyL->mtx));

    if(hashContains(storage, path) == 1){
        file *dummyF = hashTableGetFile(storage, path);

        if( clientListContains(dummyF->openerList, clientId) == 1 &&
                (dummyF->lockOwner == clientId || dummyF->lockOwner == 0)){

            GESTERRI(clientListRemove(dummyF->openerList, clientId) != 1, errno = ENOTRECOVERABLE; perror("closeFile");  return -1;)
            safeLock(&sMutex);
            closeSucc++;
            safeUnlock(&sMutex);

            safeUnlock(&(dummyL->mtx));
            POLITIC(dummyF->fPointer)
            return 0;
        }
        else{
            safeUnlock(&(dummyL->mtx));
            errno = EPERM;
            return -1;
        }
    }
    else{
        safeUnlock(&(dummyL->mtx));
        errno = ENOENT;
        return -1;
    }
}
/**
 *   @brief comportamento conforme alla funzione della api
 *   @param path il path del file di cui eseguire la unlock
 *   @param clientId l'id del client che richiede l'operazione
 *   @return 0 se termina correttamente, -1 se fallisce
*/
static int removeFile(char *path, int clientId) {
    GESTERRP(path, errno = EINVAL; return -1;)

    fileList *dummyL = hashTableGetFList(storage, path);
    GESTERRP(dummyL, errno = ENOTRECOVERABLE; perror("removeFile");  return -1;)//errori che non dovrebbero accadere
    safeLock(&(dummyL->mtx));

    if(hashContains(storage, path) == 1){
        file *dummyF = hashTableGetFile(storage, path);

        if(dummyF->lockOwner == clientId){//rimozione solo se locked

            freeFile(hashTableRemovePath(storage, path));
            safeLock(&sMutex);
            removeSucc++;
            safeUnlock(&sMutex);

            safeUnlock(&(dummyL->mtx));

            pthread_cond_broadcast(&lockFileCondizione);//consizione per ritentare la lockFile
            return 0;
        }
        else{
            safeUnlock(&(dummyL->mtx));
            errno = EPERM;
            return -1;
        }
    }
    else{
        safeUnlock(&(dummyL->mtx));
        errno = ENOENT;
        return -1;
    }
}
/**
 *   @brief Funzione che invia messaggi al client necessari per la trasmissione di N file
 *   @param fl lista di file da inviare al client
 *   @param clientId descrittore del cient
 *   @param pipeFD descrittore della pipe
 *   @param endFlag puntatore alla flag indicante l'avvenuta chiusura di una connessione
 */
static int sendNFile(fileList *fl, int N,  int clientId, int pipeFD, int *endFlag){

    file* curr = fl->head;
    char out[STANDARDMSGSIZE];
    int textLen;
    int i = 0;

    while(i < N){
        memset(out, 0, STANDARDMSGSIZE);

        if(curr == NULL){//errore
            sprintf(out, "END");

            if(writen(clientId, out, STANDARDMSGSIZE) == -1){
                perror("executeTask:sendNFile");
                *endFlag = 1;
                hashResetLock(storage, clientId);

                //USOPIPE
                GESTERRI(write(pipeFD, &clientId, sizeof(clientId)) == -1,
                         errno = EPIPE; perror("executeTask:sendNFile"); exit(EXIT_FAILURE);)
                GESTERRI(write(pipeFD, endFlag, sizeof(*endFlag)) == -1,
                         errno = EPIPE; perror("executeTask:sendNFile"); exit(EXIT_FAILURE);)
                         return -1;
            }
            return i;
        }

        textLen = strlen(curr->text) + 1;
        sprintf(out, "%s|%d|", curr->path, textLen);
        if(writen(clientId, out, STANDARDMSGSIZE) == -1){//scrittura
            perror("executeTask:sendNFile");
            *endFlag = 1;
            hashResetLock(storage, clientId);

            //USOPIPE
            GESTERRI(write(pipeFD, &clientId, sizeof(clientId)) == -1,
                     errno = EPIPE; perror("executeTask:sendNFile"); exit(EXIT_FAILURE);)
            GESTERRI(write(pipeFD, endFlag, sizeof(*endFlag)) == -1,
                     errno = EPIPE; perror("executeTask:sendNFile"); exit(EXIT_FAILURE);)
            return -1;
        }

        int pos = 0;
        while(textLen - pos > BIGMSGSIZE){
            if(writen(clientId, (curr->text) + pos, BIGMSGSIZE) == -1){
                perror("executeTask:sendNFile");
                *endFlag = 1;
                hashResetLock(storage, clientId);

                //USOPIPE
                GESTERRI(write(pipeFD, &clientId, sizeof(clientId)) == -1,
                         errno = EPIPE; perror("executeTask:sendNFile"); exit(EXIT_FAILURE);)
                GESTERRI(write(pipeFD, endFlag, sizeof(*endFlag)) == -1,
                         errno = EPIPE; perror("executeTask:sendNFile"); exit(EXIT_FAILURE);)
                return -1;
            }
            pos += BIGMSGSIZE;
        }
        if(writen(clientId, (curr->text) + pos, textLen - pos) == -1){//scrittura
            perror("executeTask:sendNFile");
            *endFlag = 1;
            hashResetLock(storage, clientId);

            //USOPIPE
            GESTERRI(write(pipeFD, &clientId, sizeof(clientId)) == -1,
                     errno = EPIPE; perror("executeTask:sendNFile"); exit(EXIT_FAILURE);)
            GESTERRI(write(pipeFD, endFlag, sizeof(*endFlag)) == -1,
                     errno = EPIPE; perror("executeTask:sendNFile"); exit(EXIT_FAILURE);)
            return -1;
        }

        curr = curr->next;
        i++;
    }
    return i;
}

//STRUTTURA DI QUEST: FUN_NAME;ARG1;ARG2;...;
/**
 *   @brief Funzione che interpreta ed esegue le operazioni richieste dai client
 *   @param clientId descrittore del cient
 *   @param pipeFD descrittore della pipe
 *   @param task richiesta da interpretare e portare a termine
 *   @param endFlag puntatore alla flag indicante l'avvenuta chiusura di una connessione
 *   @return 0 se la funzione termina senza notificare al main che il client si è disconnesso, 1 altrimenti
 */
static int executeTask(char *task, int clientId, int pipeFD, int *endFlag) {
    GESTERRP(task, errno = EINVAL; return -1;)
    GESTERRI(clientId < 1, errno = EINVAL; return -1;)
    GESTERRI(pipeFD < 1, errno = EINVAL; return -1;)

    char out[SMALLMSGSIZE];
    memset(out, 0, SMALLMSGSIZE);
    char *token = NULL;
    char *save = NULL;

    int ret;//valore di ritorno da mandare al client
    int errnoValue;// errno per il log

    token = strtok_r(task, "|", &save);//tokenizzazione stringa che contiene la task, primo token = tipo di op

    //openfile
    if (strcmp(token, "3") == 0) {
        //3|flags|pathname|

        //flags
        token = strtok_r(NULL, "|", &save);
        int flags = (int) strtol(token, NULL, 10);

        token = strtok_r(NULL, "|", &save);//il terminatore c'è per forza, se troppo lungo genera errore in seguito
        //path

        ret = openFile(token, flags, clientId);
        errnoValue = errno;

        if (ret == -1) sprintf(out, "-1|%d|", errno);
        else sprintf(out, "0|");

        GESTERRPIPE(writen(clientId, out, SMALLMSGSIZE) == -1, "executeTask:openFile")

        int o_create; int o_lock;//flag
        switch(flags){
            case 0: o_create = 0;
                    o_lock = 0;
                    break;
            case 1: o_create = 0;
                    o_lock = 1;
                break;
            case 2: o_create = 1;
                    o_lock = 0;
                break;
            case 3: o_create = 1;
                    o_lock = 1;
                break;
            default : errno = ENOTRECOVERABLE; perror("executeTask:openFile"); exit(EXIT_FAILURE); break;//??
        }

        //log
        //[ThreadID]<clientId/3/ret/errno/O_CREATE/O_LOCK/"filePath">
        safeLock(&logMutex);
        fprintf(logF, "[%lu]<%d/3/%d/%d/%d|%d/\"%s\">\n", pthread_self(), clientId, ret, errnoValue, o_create, o_lock, token);
        safeUnlock(&logMutex);
    }
    //closeFile
    else if (strcmp(token, "10") == 0) {
        //10|pathname|

        token = strtok_r(NULL, "|", &save);

        ret = closeFile(token, clientId);
        errnoValue = errno;

        if (ret == -1) sprintf(out, "-1|%d|", errno);
        else sprintf(out, "0|");

        GESTERRPIPE(writen(clientId, out, SMALLMSGSIZE) == -1, "executeTask:closeFile")

        //log
        //[ThreadID]<clientId/10/ret/errno/"filePath">
        safeLock(&logMutex);
        fprintf(logF, "[%lu]<%d/10/%d/%d/\"%s\">\n", pthread_self(), clientId, ret, errnoValue, token);
        safeUnlock(&logMutex);
    }
    //lockFile
    else if (strcmp(token, "8") == 0) {
        //8|pathname|

        token = strtok_r(NULL, "|", &save);
        ret = -2;

        safeLock(&lockFileMutex);
        ret = lockFile(token, clientId);
        errnoValue = errno;
        while (ret == -2){
            //il thread attende un segnale che viene mandato quando un client esegue una unlock su un file, o elimina un file
            pthread_cond_wait(&lockFileCondizione, &lockFileMutex);
            ret = lockFile(token, clientId);
            errnoValue = errno;
        }
        safeUnlock(&lockFileMutex);

        if (ret == -1) sprintf(out, "-1|%d|", errno);
        else sprintf(out, "0|");

        GESTERRPIPE(writen(clientId, out, SMALLMSGSIZE) == -1, "executeTask:lockFile")

        //log
        //[Thrd_id]<clientId/8/ret/errno/"filePath">
        safeLock(&logMutex);
        fprintf(logF, "[%lu]<%d/8/%d/%d/\"%s\">\n", pthread_self(), clientId,ret,errnoValue, token);
        safeUnlock(&logMutex);
    }
    //unlockFile
    else if (strcmp(token, "9") == 0) {
        //9|pathname|

        // tokenizzazione degli argomenti
        token = strtok_r(NULL, "|", &save);

        // esecuzione della richiesta
        ret = unlockFile(token, clientId);
        errnoValue = errno;

        if (ret == -1) sprintf(out, "-1|%d|", errno);
        else sprintf(out, "0|");

        GESTERRPIPE(writen(clientId, out, SMALLMSGSIZE) == -1, "executeTask:unlockFile")

        //log
        //[Thrd_id]<clientId/9/ret/errno/"filePath">
        safeLock(&logMutex);
        fprintf(logF, "[%lu]<%d/9/%d/%d/%s>\n", pthread_self(), clientId, ret, errnoValue, token);
        safeUnlock(&logMutex);
    }
    //removeFile
    else if (strcmp(token, "11") == 0) {
        //11|pathname|

        token = strtok_r(NULL, "|", &save);

        // esecuzione della richiesta
        ret = removeFile(token, clientId);
        errnoValue = errno;

        if (ret == -1) sprintf(out, "-1|%d|", errno);
        else sprintf(out, "0|");

        GESTERRPIPE(writen(clientId, out, SMALLMSGSIZE) == -1, "executeTask:removeFile")

        //log
        //[Thrd_id]<clientId/11/ret/errno/"filePath">
        safeLock(&logMutex);
        fprintf(logF, "[%lu]<%d/11/%d/%d/\"%s\">\n", pthread_self(), clientId, ret, errnoValue, token);
        safeUnlock(&logMutex);
    }
    //writeFile
    else if (strcmp(token, "6") == 0) {
        //6|pathname|sizeText|
        //prossimo messagio: text|

        //pathname
        char *path;
        path = strtok_r(NULL, "|", &save);

        //dim testo (con terminatore)
        size_t sizeText;
        token = strtok_r(NULL, "|", &save);
        sizeText = (size_t) strtol(token, NULL, 10);

        char* text = malloc(sizeof(char) * sizeText);//buffer per il testo

        int pos = 0;
        while(sizeText - pos > BIGMSGSIZE){
            GESTERRPIPE(readn(clientId, (text + pos), BIGMSGSIZE) == -1, "executeTask:writeFile" )//ricezione
            pos += BIGMSGSIZE;
        }
        GESTERRPIPE(readn(clientId, (text + pos), (sizeText - pos)) == -1, "executeTask:writeFile" )//ricezione

        text[sizeText - 1] = '\0';//serve??

        // esecuzione della richiesta
        fileList *dummyFileList = writeFile(path, text, clientId);
        free(text);

        errnoValue = errno;
        size_t listSize = 0;
        if(dummyFileList != NULL) listSize = dummyFileList->size;
        if (errnoValue != 0){
            ret = -1;
            sprintf(out, "-1|%d|%lu|", errno, listSize);
        }
        else {
            ret = 0;
            sprintf(out, "0|%lu|", listSize);
        }

        GESTERRPIPE(writen(clientId, out, SMALLMSGSIZE) == -1 , "executeTask:writeFile" )//ricezione 2.2

        if(listSize != 0)
            GESTERRI(sendNFile(dummyFileList, listSize, clientId, pipeFD, endFlag) == -1, freeFileList(dummyFileList); return 1;)//errore invio messaggi

        //log
        //[Thrd_id]<clientId/6/ret/errno/"filePath"/listSize>
        safeLock(&logMutex);
        fprintf(logF, "[%lu]<%d/6/%d/%d/\"%s\"/%lu>\n", pthread_self(), clientId, ret,errnoValue, token, listSize);
        safeUnlock(&logMutex);
        freeFileList(dummyFileList);
    }
    //appendToFile
    else if (strcmp(token, "7") == 0) {
        //7|pathname|sizeText|
        //prossimo messagio: text|

        //pathname
        char *path;
        path = strtok_r(NULL, "|", &save);

        //dim testo (con terminatore)
        size_t sizeText;
        token = strtok_r(NULL, "|", &save);
        sizeText = (size_t) strtol(token, NULL, 10);

        char text[sizeText];


        int pos = 0;
        while(sizeText - pos > BIGMSGSIZE){
            GESTERRPIPE(readn(clientId, (text + pos), BIGMSGSIZE) == -1, "executeTask:appendToFile" )//ricezione 2.2
            pos += BIGMSGSIZE;
        }
        GESTERRPIPE(readn(clientId, (text + pos), (sizeText - pos)) == -1, "executeTask:appendToFile" )//ricezione 2.2


        text[sizeText - 1] = '\0';

        // esecuzione della richiesta
        fileList *dummyFileList = appendToFile(path, text, clientId);
        errnoValue = errno;
        size_t listSize = 0;
        if(dummyFileList != NULL) listSize = dummyFileList->size;
        if (errnoValue != 0){
            ret = -1;
            sprintf(out, "-1|%d|%lu|", errno, listSize);
        }
        else {
            ret = 0;
            sprintf(out, "0|%lu|", listSize);
        }

        GESTERRPIPE(writen(clientId, out, SMALLMSGSIZE) == -1, "executeTask:appendToFile")

        if(listSize != 0)
            GESTERRI(sendNFile(dummyFileList, listSize, clientId, pipeFD, endFlag) == -1, freeFileList(dummyFileList);return 1;)//errore invio messaggi

        //log
        //[Thrd_id]<clientId/7/ret/errno/"filePath"/listSize>
        safeLock(&logMutex);
        fprintf(logF, "[%lu]<%d/7/%d/%d/\"%s\"/%lu>\n", pthread_self(), clientId, ret,errnoValue, token, listSize);
        safeUnlock(&logMutex);
        freeFileList(dummyFileList);
    }
    //readFile
    else if (strcmp(token, "4") == 0) {
        //4|pathname|

        token = strtok_r(NULL, "|", &save);

        char *buffer;
        size_t size;

        ret = readFile(token, &buffer, &size, clientId);//esecuzione read
        errnoValue = errno;

        if (ret == -1){
            sprintf(out, "-1|%d|", errno);
            buffer = NULL;
            size = 0;
        }
        else sprintf(out, "0|%lu|", size); //comando terminato correttamente, 0;dimensioneRead;

        GESTERRPIPE(writen(clientId, out, SMALLMSGSIZE) == -1, "executeTask:readFile")//successo o fallimento

        if(ret != -1) {//contenuto file
            int pos = 0;
            while (size - pos > BIGMSGSIZE) {
                GESTERRPIPE(writen(clientId, buffer + pos, BIGMSGSIZE) == -1, "executeTask:readFile")
                pos += BIGMSGSIZE;
            }
            GESTERRPIPE(writen(clientId, buffer + pos, size - pos) == -1, "executeTask:readFile")
        }

        //log
        //[Thrd_id]<clientId/4/ret/errno/"filePath"/readSize>
        safeLock(&logMutex);
        fprintf(logF, "[%lu]<%d/4/%d/%d/\"%s\"/%lu>\n", pthread_self(), clientId, ret, errnoValue, token, size);
        safeUnlock(&logMutex);

        free(buffer);
    }
    //readNFiles
    else if (strcmp(token, "5") == 0) {
        //5|N|

        token = strtok_r(NULL, "|", &save);
        int N = (int) strtol(token, NULL, 10);
        int fileInviati = 0;
        int storageFinito = 0;
        int lista = 0;

        int dummy;
        if(N == 0 || N > currentNFile) N = INT_MAX; //valore massimo

        while(fileInviati < N && !storageFinito){
            //invio file nella pista lista dello storage
            safeLock(&(storage->lists[lista]->mtx));

            dummy = N - fileInviati;//numero di file giusti
            if(dummy > (int) storage->lists[lista]->size) dummy = (int) storage->lists[lista]->size;

            GESTERRI(sendNFile(storage->lists[lista],dummy , clientId, pipeFD, endFlag) == -1, return 1;)

            fileInviati += dummy;
            safeUnlock(&(storage->lists[lista]->mtx));

            lista++;
            if(lista >= storage->size) storageFinito = 1;
        }
        if(storageFinito && fileInviati<N){//se N era 0 o troppo grande avvisiamo che non abbiamo altro da inviare
            char out2[STANDARDMSGSIZE] = "END";

            GESTERRPIPE(writen(clientId, out2, STANDARDMSGSIZE) == -1, "executeTask:readNFiles")
        }


        //log
        //[Thrd_id]<clientId/5/0/fileInviati>
        safeLock(&logMutex);
        fprintf(logF, "[%lu]<%d/5/0/%d>\n", pthread_self(), clientId, fileInviati);
        safeUnlock(&logMutex);
    }
    //closeConnection
    else if (strcmp(token, "2") == 0) {//disconnect
        hashResetLock(storage, clientId);
        *endFlag = 1;//disconnesso, cambiare client

        GESTERRI(write(pipeFD, &clientId, sizeof(clientId)) == -1,//scrittura nella pipe per comunicare con il server stesso
                 errno = EPIPE; perror("executeTask:closeConnection"); exit(EXIT_FAILURE);)
        GESTERRI(write(pipeFD, endFlag, sizeof(*endFlag)) == -1,
                 errno = EPIPE; perror("executeTask:closeConnection"); exit(EXIT_FAILURE);)

        //log
        //[Thrd_id]<clientId/2>
        safeLock(&logMutex);
        fprintf(logF, "[%lu]<%d/2/0>\n", pthread_self(), clientId);
        safeUnlock(&logMutex);
        return 1;
    }
    //default
    else {
        //funzione non implementata
        sprintf(out, "-1|%d|", ENOSYS);

        GESTERRPIPE(writen(clientId, out, SMALLMSGSIZE) == -1,"executeTask:default")
    }
    return 0;
}
/**
 *   @brief Funzione eseguita dai worker che aspettano e gestiscono richieste dei client
 *   @param arg richiesta del client da eseguire
 */
static void *workerRoutine(void *arg) {
    //indirizzo pipe passato
    int pipeFD = *((int *) arg);
    int clientId;

    while (1) {
        int end = 0; //valore indicante la terminazione del client
        int ret;

        //un client viene espulso dalla coda secondo la politica priorityList
        clientId = clientListPop(clientQ);

        //interruzione
        if (clientId == -2) return (void *) -1;
        if (clientId == -1) return (void *) 0;

        //esegue una richiesta del client e poi diventa nuovamente disponibile per altre richieste
        char task[STANDARDMSGSIZE];
        memset(task, 0, STANDARDMSGSIZE);

        ret = readn(clientId, task, STANDARDMSGSIZE);

        if (ret <= 0) {//client disconnesso, worker si deve liberare
            end = 1;//disconnesso, cambiare client
            hashResetLock(storage, clientId);

            safeLock(&logMutex);
            fprintf(logF, "[%lu]<%d/2/1>\n", pthread_self(), clientId);
            safeUnlock(&logMutex);

            //USOPIPE
            GESTERRI(write(pipeFD, &clientId, sizeof(clientId)) == -1,//scrittura nella pipe per comunicare con il server stesso
                     errno = EPIPE; perror("workerRoutine"); exit(EXIT_FAILURE);)
            GESTERRI(write(pipeFD, &end, sizeof(end)) == -1,
                     errno = EPIPE; perror("workerRoutine"); exit(EXIT_FAILURE);)
        }
        else {//eseguiamo il comando del client

            PRINT(printf("CLIENTID: %d\nRICHIESTA <%s>\n", clientId, task);)
            ret = executeTask(task, clientId, pipeFD, &end);

            if(ret == 0){

                //notifichiamo il main che abbiamo finito la gestione della richiesta, solo se non è già stato fatto
                GESTERRI(write(pipeFD, &clientId, sizeof(clientId)) == -1,//scrittura nella pipe per comunicare con il server stesso
                         errno = EPIPE; perror("workerRoutine"); exit(EXIT_FAILURE);)
                GESTERRI(write(pipeFD, &end, sizeof(end)) == -1,
                         errno = EPIPE; perror("workerRoutine"); exit(EXIT_FAILURE);)

            }else if(ret == -1){
                perror("workerRoutine");
                return (void *) -1;
            }
        }
    }
}

//main e funzioni che rappresentano la logica del server
/**
 *   @brief Funzione che gestisce i segnali: SIGINT e SIGQUIT = terminazione veloce; SIGHUP = terminazione soft
 *   @param sign segnale ricevuto
*/
static void gestTerminazione(int sign) {
    if (sign == SIGINT || sign == SIGQUIT) t = 1; //terminazione veloce, aspeta solo i thread a lavoro e chiude il server
    else if (sign == SIGHUP)
        t = 2; //terminazione soft, esaudisce tutte le operazioni dei clienti in coda
}
/**
 *   @brief Funzione che gestisce la recezione e la gestione dei segnali ricevuti, esempio l'interruzion
*/
static void gestSig(){

    int dummyRet;
    sigset_t set;
    struct sigaction s;
    memset(&s, 0, sizeof(s)); // setta tutta la struct a 0
    s.sa_handler = gestTerminazione;

    dummyRet = sigfillset(&set);
    GESTERRI(dummyRet == -1, perror("getSig:sigfillset"); exit(EXIT_FAILURE); )

    dummyRet = pthread_sigmask(SIG_SETMASK, &set, NULL);
    GESTERRI(dummyRet == -1, perror("getSig:pthread_sigmask"); exit(EXIT_FAILURE); )

    dummyRet = sigaction(SIGINT, &s, NULL);  //interruzione da tastiera ctrl+c
    GESTERRI(dummyRet == -1, perror("getSig:SIGINT"); exit(EXIT_FAILURE); )

    dummyRet = sigaction(SIGQUIT, &s, NULL);  //interruzione da tastiera ctrl+\ ??
    GESTERRI(dummyRet == -1, perror("getSig:SIGQUIT"); exit(EXIT_FAILURE); )

    dummyRet = sigaction(SIGHUP, &s, NULL);  //interruzione non tastiera
    GESTERRI(dummyRet == -1, perror("getSig:SIGHUP"); exit(EXIT_FAILURE); )

    s.sa_handler = SIG_IGN;
    dummyRet = sigaction(SIGPIPE, &s, NULL);//scrittura su un socket chiuso, ignoro
    GESTERRI(dummyRet == -1, perror("getSig:SIGPIPE"); exit(EXIT_FAILURE); )

    dummyRet = sigemptyset(&set);//setto la maschera del thread a 00
    GESTERRI(dummyRet == -1, perror("getSig:sigemptyset"); exit(EXIT_FAILURE); )

    dummyRet = pthread_sigmask(SIG_SETMASK, &set, NULL);
    GESTERRI(dummyRet == -1, perror("getSig:pthread_sigmask"); exit(EXIT_FAILURE); )
}
/**
 *   @brief Funzione che esegue il parsing del file di config e setta i parametri del server
*/
void parseConfig(char* pathConfig){
    //int defConfig = 0;

    //valori server standard
    maxNFile = 1000;
    maxStorageSize = 1024*1024*65; //65MB
    nThread = 8;
    strcpy(socketName, SOCKET);

    if(pathConfig != NULL) {

        char confLine[100];
        FILE *conf;
        conf = fopen(pathConfig, "r"); // apertura del file di config
        int linea = 1;

        if (conf == NULL) {
            perror("parseConfig:fopen");
        }
        else {

            char parametro[75];
            char valore[75];
            long n;//valore numerale

            while (fgets(confLine, 100, conf) != NULL) {
                if (confLine[0] != '\n') {

                    //scansione parametro da modificare e valore dello stesso
                    int err;
                    err = sscanf(confLine, "%[^=]=%s", parametro, valore);

                    if (err != 2) printf("Formato linea %d non corretto\n", linea);
                    else {
                        if (strcmp(parametro, "nThread") == 0) {//numero dei thread

                            int val = isNumber(valore, &n);

                            if (val == 2 || val == 0 || n <= 0) printf("Formato linea %d non corretto\n", linea);
                            else nThread = (size_t) n;
                        }
                        else if (strcmp(parametro, "socketName") == 0) {//nome del socket
                            strcpy(socketName, valore);
                        }
                        else if (strcmp(parametro, "maxNFile") == 0) {//numero massimo di file

                            int out = isNumber(valore, &n);

                            if (out == 2 || out == 0 || n <= 0) printf("Formato linea %d non corretto\n", linea);
                            else maxNFile = (size_t) n;
                        }
                        else if (strcmp(parametro, "maxStorageSize") == 0) {//dimensione massima storage

                            int out = isNumber(valore, &n);

                            if (out == 2 || out == 0 || n <= 0) printf("Formato linea %d non corretto\n", linea);
                            else maxStorageSize = (size_t) n;
                        }
                        else if (strcmp(parametro, "politic") == 0) {//politica dello storage

                            if (strcmp(valore, "FIFO") == 0) politic = 0;
                            else if (strcmp(valore, "LRU") == 0) politic = 1;
                                else printf("Formato linea %d non corretto\n", linea);
                        }
                        else if (strcmp(parametro, "print") == 0) {//politica dello storage

                            if (strcmp(valore, "YES") == 0) print = 1;
                            else if (strcmp(valore, "NO") == 0) print = 0;
                            else printf("Formato linea %d non corretto\n", linea);
                        }
                        else printf("Formato linea %d non corretto\n", linea);
                    }
                }
                linea++;
            }
            fclose(conf);
        }
    }

    maxText = (maxStorageSize/2) - ((maxStorageSize/2) % BIGMSGSIZE);// dimensione massima testo file, multiplo di BIGMSGSIZE

    PRINT(printf("PARAMETRI DEL SERVER\n");
    printf("  Nome socket: \"%s\"\n  Numero thread: %lu\n  Numero max File: %lu\n  Dimensione max server: %lu\n  Dimensione massima file: %lu\n  Politica: %d\n  Stampe: %d\n",
           socketName, nThread, maxNFile, maxStorageSize, MAXTEXTLENGHT,  politic, print);)
    return;
}
/**
 *   @brief Funzione main che inizializza le strutture dati, i worker e gesisce le richieste dei client e la terminazione del server
*/
int main(int argc, char *argv[]) {

    // argv[0] server.c | argv[1] config.txt_path | ... | argv[argc] NULL
    PRINT(printf("Inizio avvio server\n");)
    int dummyRet;
    int i; //utile per i cicli for
    int t_soft = 0; //flag per memorizzare la ricezione del segnale di arresto

    char *pathConfig = NULL;
    if (argc == 3) {
        if (strcmp(argv[1], "-cnfg") == 0) {
            pathConfig = argv[2];
        }
    }

    parseConfig(pathConfig);//configurazione valori server
    gestSig();//gestione segnali e handler, segnali memorizzati nella variabile t


    //strutture dati
    clientQ = createClientList();//coda richieste clienti
    GESTERRP(clientQ, perror("main:createClientList"); exit(EXIT_FAILURE);)

    storage = createHash(maxNFile/5 + 1);//hash table
    GESTERRP(storage, perror("main:createHash"); exit(EXIT_FAILURE);)

    priorityQ = createPriorityList();//lista priorità di rimpiazzamento
    GESTERRP(priorityQ, perror("main:createPriorityList"); exit(EXIT_FAILURE);)

    //creazione pipe per comunicazione con i thread
    int pip[2];
    dummyRet = pipe(pip);
    GESTERRI(dummyRet == -1, perror("main:pipe"); exit(EXIT_FAILURE);)

    //thread pool
    pthread_t *tPool = malloc(sizeof(pthread_t) * nThread);
    GESTERRP(tPool, perror("main:malloc"); exit(EXIT_FAILURE);)

    for (i = 0; i < nThread; i++) {

        //creazione dei thread che ricevono l'indirizzo di scrittura della pipe
        dummyRet = pthread_create(&tPool[i], NULL, workerRoutine, (void *) (&pip[1]));
        GESTERRI(dummyRet == -1, perror("main:pthread_create"); exit(EXIT_FAILURE);)
    }

    //prepariamo il file di log
    GESTERRP(logF = fopen(LOGNAME, "w"), perror("main:fopen"); exit(EXIT_FAILURE);) //il file di log sempre aperto

    ////////////////////////////////////FAREEEEE
    //SOCKET
    int socketFD; //FD socket
    int clientFD;
    int fd_num = 0; //massimo fd della select
    int dummyFD;
    //insiemi file descriptor totale e per la lettura
    fd_set set;
    fd_set rd_set;

    //settiamo correttamente il socket per la accept
    struct sockaddr_un sa;
    strncpy(sa.sun_path, socketName, MAXFILENAMELENGHT);
    sa.sun_family = AF_UNIX;

    GESTERRI((socketFD = socket(AF_UNIX, SOCK_STREAM, 0)) == -1, perror("main:socket"); exit(EXIT_FAILURE);)
    GESTERRI((bind(socketFD, (struct sockaddr *) &sa, sizeof(sa))) == -1, perror("main:bind"); exit(EXIT_FAILURE);)
    GESTERRI(listen(socketFD, SOMAXCONN) == -1, perror("main:listen"); exit(EXIT_FAILURE);)

    //settiamo a 0 tutta la maschera
    FD_ZERO(&set);

    //teniamo in fd_num il descrittore massimo
    AGGMAX(socketFD, fd_num)
    //registrazione del socket
    FD_SET(socketFD, &set);

    //registrazione della pipe
    AGGMAX(pip[0], fd_num)
    FD_SET(pip[0], &set);

    //inizio ricezione richieste dei client
    PRINT(printf("Server avviato, attesa richieste...\n");)
    while (1) {

        //conrollo terminazione
        if (t_soft == 0 && t == 1){
            PRINT(printf("Terminazione violenta\n");)
            break;//chiusura violenta, non terminiamo le richieste
        }
        else if (t_soft == 0 && t == 2) { //chiusura soft
            t_soft = 1;//abbiamo iniziato la procedura di terminazione leggera
            t = 0;

            if (currentConn < 1){
                PRINT(printf("Terminazione leggera\n");)
                break;
            }else {
                PRINT(printf("Terminazione leggera\n");)

                FD_CLR(socketFD, &set);//rimozione del fd del socket dal set, non accetteremo altre connessioni
                if (socketFD == fd_num) fd_num = max_up(set, fd_num);//aggiorno l'indice massimo

                close(socketFD);//chiusura del socket
                continue; //ci rimettiamo in attesa della select
            }
        }
        else if (t_soft == 1 && t == 1){//2 segnali "soft" -> terminazione violenta
            PRINT(printf("Terminazione violenta\n");)
            break;//chiusura violenta, non terminiamo le richieste

        }


        rd_set = set;//ripristino il set di partenza

        if (select(fd_num + 1, &rd_set, NULL, NULL, NULL) == -1) {//gestione errore
            if (t != 0) continue; //fallimento select a seguito di un segnale
            else {//fallimento select senza segnale ricevuto
                perror("main:select"); break;
            }
        }

        //controlliamo tutti i file descriptors
        for (dummyFD = 0; dummyFD <= fd_num; dummyFD++) {
            if (FD_ISSET(dummyFD, &rd_set)) {//cerchiamo nel set di FD quelli pronti
                //il socket è pronto per accettare una nuova richiesta di connessine
                if (dummyFD == socketFD){
                    //accept che NON si bloccherà, gestione errore di fallimento per cause sconosciute
                    GESTERRI((clientFD = accept(socketFD, NULL, 0)) == -1, perror("main:accept"); exit(EXIT_FAILURE);)

                    //comunichiamo la grandezza massima dei file con la api
                    char dim[SMALLMSGSIZE] = "";
                    sprintf(dim, "%lu|", MAXTEXTLENGHT);
                    GESTERRI(writen(clientFD, dim, SMALLMSGSIZE) == -1, perror("main:openConnection"); continue;)

                    FD_SET(clientFD, &set);//il FD del client va aggiunto all'insieme della select
                    AGGMAX(clientFD, fd_num) //tengo aggiornato l'indice massimo

                    safeLock(&sMutex);
                    currentConn++;//aggiornamento variabili per le statistiche
                    AGGMAX(currentConn, maxConnReach)
                    safeUnlock(&sMutex);

                    PRINT(printf("CLIENTID: %d\nRICHIESTA <%d/1>\n", clientFD, clientFD);)
                    //log
                    //[Thrd_id]<1/clientFD>
                    safeLock(&logMutex);
                    fprintf(logF, "[%lu]<%d/1>\n", pthread_self(), clientFD);
                    safeUnlock(&logMutex);
                }
                //un worker ha terminato di gestire una richiesta e lo notifica al main
                else if (dummyFD == pip[0]) {
                    int fd_c1;
                    int flag;

                    GESTERRI(read(pip[0], &fd_c1, sizeof(fd_c1)) <= 0, perror("main:read:pipe"); exit(EXIT_FAILURE);) //lettura fd client
                    GESTERRI(read(pip[0], &flag, sizeof(flag)) <= 0, perror("main:read:pipe"); exit(EXIT_FAILURE);) //lettura stato client (1 -> terminato)

                    if(flag == 1){//il client è terminato, il suo fd deve essere rimosso dal set
                        close(fd_c1);//chiusura del client

                        safeLock(&sMutex);
                        currentConn--;//client disconnesso
                        safeUnlock(&sMutex);

                    }
                    else{
                        FD_SET(fd_c1, &set);
                        AGGMAX(fd_c1, fd_num)
                    }
                }
                //un client ha inviato una richiesta al server
                else {

                    safeLock(&cMutex);
                    clientListAddH(clientQ, dummyFD);
                    pthread_cond_signal(&condCoda);
                    safeUnlock(&cMutex);

                    FD_CLR(dummyFD, &set);
                    if (dummyFD == fd_num) fd_num = max_up(set, fd_num);//aggiorno l'indice massimo
                }
            }
        }

        if(t_soft == 1 && currentConn < 1){
            break;
        }
    }


    PRINT(printf("\nChiusura del Server...\n");)

    //server in fase di terminazione, puliamo la lista di rihieste e inseriamo richieste "speciali" per terminare i thread worker
    safeLock(&cMutex);
    freeClientList(clientQ);
    for (i = 0; i < nThread; i++) {
        clientListAddH(clientQ, -1);
        pthread_cond_signal(&condCoda);
    }
    safeUnlock(&cMutex);

    for (i = 0; i < nThread; i++) {
        if (pthread_join(tPool[i], NULL) != 0) {
            perror("main:pthread_join");
            exit(EXIT_FAILURE);
        }
    }
    free(tPool);
    remove(socketName);

    //operazioni per stat di read e write
    size_t mediaReadSize = 0;
    size_t mediaWriteSize = 0;

    if(readSucc != 0) mediaReadSize = totalReadSize/readSucc;
    if(writeSucc != 0) mediaWriteSize = totalWriteSize / writeSucc;

    //chiusura file di log
    fprintf(logF, "\n==================================================================================================================================="
                  "\nSUNTO DELLE STATISTICHE:\n");
    fprintf(logF, "Numero di write: %lu\nSize media delle scritture: %lu\nNumero di read: %lu\nSize media delle letture: %lu\n"
                  "Numero di open file: %lu\nNumero di lock: %lu\nNumero di unlock: %lu\nNumero di close: %lu\nNumero di remove: %lu\n"
                  "Dimensione massima dello storage: %lu (MB)\nNumero di file massimo: %lu\nNumero di replace: %lu\n"
                  "Massimo numero di connessioni contemporanee: %lu\n",
             writeSucc, mediaWriteSize, readSucc, mediaReadSize, openSucc, lockSucc, unlockSucc, closeSucc, removeSucc,
            maxStorageSizeReach/(1024*1024) + 1, maxNFileReach, replaceSucc, maxConnReach);

    //print andamento del server
    PRINT(
    printf("SERVER INFO:\n");
    printf("Numero massimo di files raggiunto:\t%lu\n", maxNFileReach);
    printf("Dimensione massima raggiunta:\t\t%lu (MB)\t\t%lu (B)\n", maxStorageSizeReach/(1024*1024) + 1, maxStorageSizeReach);
    printf("Numero read e dimensione media:\t\t%lu\t\t%lu (B)\n", readSucc, mediaReadSize);
    printf("Dimensione media delle write:\t\t%lu\t\t%lu (B)\n", writeSucc, mediaWriteSize);
    printf("Numero di files espulsi:\t\t%lu\n\n", replaceSucc);)

    //free finali
    freeHashTable(storage);
    freePriorityList(priorityQ);
    freeClientList(clientQ);
    free(clientQ);
    fclose(logF);

    return 0;
}







