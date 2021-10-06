#include <stdlib.h>
#include <stdio.h>
#include <string.h>

//MACRO
//gestione errori per puntatori e interi
#define GESTERRP(a,b) if( (a) == NULL ) {b}
#define GESTERRI(a,b) if( (a) != 0 ) {b}
//macro per la funzione hash
#define mix(a,b,c) { \
  a -= b; a -= c; a ^= (c>>13); \
  b -= c; b -= a; b ^= (a<<8); \
  c -= a; c -= b; c ^= (b>>13); \
  a -= b; a -= c; a ^= (c>>12);  \
  b -= c; b -= a; b ^= (a<<16); \
  c -= a; c -= b; c ^= (b>>5); \
  a -= b; a -= c; a ^= (c>>3);  \
  b -= c; b -= a; b ^= (a<<10); \
  c -= a; c -= b; c ^= (b>>15); \
}

//STRUCT UTILI
//il nodo della lista generico
typedef struct ln{
    void* el;
    struct ln* next;
    struct ln* prec;
} listNode;
//il tipo lista su cui applicare i metodi implementati
typedef struct l{
    listNode * head;
    listNode * tail;
    int (*cmpf) (void*, void*);
} list;
//tipo hashTable, comprende un vettore di elementi lista che contengono a loro volta un HashNode,
//che contiene la chiave e l'elemento (*void)
typedef struct h{
    list** vettTab;
    int dimTabella;
} hashTable;
typedef struct hn{
    char* key;
    void* el;
} hashNode;
//typedef funzione di hashing
typedef  unsigned long  int  ub4;   /* unsigned 4-byte quantities */
typedef  unsigned       char ub1;   /* unsigned 1-byte quantities */

//FUNZIONE HASH
//http://burtleburtle.net/bob/hash/doobs.html (source)
ub4 hash( k, length, initval)
        register ub1 *k;        /* the key */
        register ub4  length;   /* the length of the key */
        register ub4  initval;  /* the previous hash, or an arbitrary value */
{
    register ub4 a,b,c,len;

    /* Set up the internal state */
    len = length;
    a = b = 0x9e3779b9;  /* the golden ratio; an arbitrary value */
    c = initval;         /* the previous hash value */

    /*---------------------------------------- handle most of the key */
    while (len >= 12)
    {
        a += (k[0] +((ub4)k[1]<<8) +((ub4)k[2]<<16) +((ub4)k[3]<<24));
        b += (k[4] +((ub4)k[5]<<8) +((ub4)k[6]<<16) +((ub4)k[7]<<24));
        c += (k[8] +((ub4)k[9]<<8) +((ub4)k[10]<<16)+((ub4)k[11]<<24));
        mix(a,b,c)
        k += 12; len -= 12;
    }

    /*------------------------------------- handle the last 11 bytes */
    c += length;
    switch(len)              /* all the case statements fall through */
    {
        case 11: c+=((ub4)k[10]<<24);
        case 10: c+=((ub4)k[9]<<16);
        case 9 : c+=((ub4)k[8]<<8);
            /* the first byte of c is reserved for the length */
        case 8 : b+=((ub4)k[7]<<24);
        case 7 : b+=((ub4)k[6]<<16);
        case 6 : b+=((ub4)k[5]<<8);
        case 5 : b+=k[4];
        case 4 : a+=((ub4)k[3]<<24);
        case 3 : a+=((ub4)k[2]<<16);
        case 2 : a+=((ub4)k[1]<<8);
        case 1 : a+=k[0];
        default : ;
            /* case 0: nothing left to add */
    }
    mix(a,b,c)
    /*-------------------------------------------- report the result */
    return c;
}
ub4 bernstein(ub1 *key, ub4 len, ub4 level)
{
    ub4 hash = level;
    ub4 i;
    for (i=0; i<len; ++i) hash = 33*hash + key[i];
    return hash;
}
//funzione hash, prende una stringa e restituisce un numero compres tra 0 e N scelto
static int hashF(char* s, int N){
    ub4 valore = bernstein((ub1*)s, strlen(s),0);
    int ret = valore % N;
    return ret;
}
//FUNZIONI UTILI
//funzione per comparare oggetti dentro alla lista
//a l'elemento della struttura dati, b l'elemento preso dalla funzione di eliminazione per il confronto
static int compare(void* a, void* b) {return a == b;}
static int compareH(void* a, void* b){
    return ( !strcmp( ((hashNode*)(a))->key , (char*)b));
}
//debugging solo interi nella  lista
void printList(list* l){

    GESTERRP(l, perror("we ma che fai bimbo"); return;)
    listNode* dummy = l->head;
    while(dummy != NULL){
        printf("%d   ", *((int*)(dummy->el)) );
        dummy = dummy->next;
    }
}
//debugging ma no
/*void printHashT(hashTable* ht){
    int i;
    printf("dimTab: %d, elTot: %d", ht->dimTabella, ht->elTot);
    for(i=0; i<ht->dimTabella; i++){
        printList(ht->vettTab[i]);
    }
    printf("\n");
}*/

//FUNZIONI PER LISTE
//crea una lista, allocandola nello heap, e restituendola al chiamante
list* createList(int (*cmpf) (void*, void*)){
    list* dummy;

    GESTERRP(dummy = malloc(sizeof(list)) , perror("malloc failed"); return NULL;)
    dummy->head = NULL;
    dummy->tail = NULL;
    if(cmpf == NULL) dummy->cmpf = compare;
    else dummy->cmpf = cmpf;
    return dummy;
}
//agggiungi un elemento alla testa della lista, ritorna -1 se fallisce
int addHeadL(list* l, void* el){
    GESTERRP(l, perror("list not initialized");return -1;)//gestione dell'errore da rivedere
    GESTERRP(el, perror("el is NULL"); return -1;)
    listNode* dummy;
    GESTERRP(dummy = malloc(sizeof(listNode)),perror("malloc failed"); return -1;)


    if(l->head == NULL && l->tail == NULL){

        dummy->next = NULL;
        dummy->prec = NULL;
        dummy->el = el;

        l->head = dummy;
        l->tail = dummy;
    }
    else{
        dummy->next = l->head;
        l->head->prec = dummy;
        dummy->prec = NULL;
        dummy->el = el;

        l->head = dummy;
    }
    return 0;
}
//agggiungi un elemento alla coda della lista, ritorna -1 se fallisce
int addTailL(list* l, void* el){
    GESTERRP(l, perror("list not initialized");return -1;)//gestione dell'errore da rivedere
    GESTERRP(el, perror("el is NULL"); return -1;)
    listNode* dummy;
    GESTERRP(dummy = malloc(sizeof(listNode)),perror("malloc failed"); return -1;)


    if(l->head == NULL && l->tail == NULL){

        dummy->next = NULL;
        dummy->prec = NULL;
        dummy->el = el;

        l->head = dummy;
        l->tail = dummy;
    }
    else{
        dummy->prec = l->tail;
        l->tail->next = dummy;

        dummy->next = NULL;
        dummy->el = el;
        l->tail = dummy;
    }
    return 0;
}
//vedere removeElL
void* removeFirstL(list* l){

    GESTERRP(l, perror("list not initialized");return NULL;)//non si arresta
    GESTERRP(l->head, return NULL;)
    void* dummy = l->head->el;

    if(l->head->next == NULL){
        l->tail = NULL;
        free(l->head);
        l->head = NULL;
    }
    else{
        l->head = l->head->next;
        free(l->head->prec);
        l->head->prec = NULL;
    }

    return dummy;
}
void* removeLastL(list* l){

    GESTERRP(l, perror("list not initialized");return NULL;)//non si arresta
    GESTERRP(l->tail, return NULL;)
    void* dummy = l->tail->el;

    if(l->tail->prec == NULL){
        l->head = NULL;
        free(l->tail);
        l->tail = NULL;
    }
    else{
        l->tail = l->tail->prec;
        free(l->tail->next);
        l->tail->next = NULL;
    }

    return dummy;
}
//rimuove dalla testa, dalla coda e generale (che sfrutta gli altri 2 metodi)
//restituisce o l'elemento rimosso (void*) oppure null se non trova l'elemento
void* removeElL(list* l, void* el){

    GESTERRP(el, perror("el is NULL"); return NULL;)
    GESTERRP(l, perror("list not initialized");return NULL;)//non si arresta
    GESTERRP(l->head, return NULL;)

    if(l->cmpf(l->head->el,el)) return removeFirstL(l);
    else if(l->cmpf(l->tail->el,el)) return removeLastL(l);
    else{
        listNode* dummy = l->head->next;
        void* ret = NULL;
        while(dummy != NULL && !(l->cmpf(dummy->el, el))) dummy = dummy->next;

        if(dummy != NULL){
            dummy->prec->next = dummy->next;
            dummy->next->prec = dummy->prec;
            ret = dummy->el;

            free(dummy);
        }
        return ret;
    }
}

//fa la free di tutti i puntatori contenuti nella lista e della lista stessa
int freeAllL(list* l){
    GESTERRP(l, return 0;)
    listNode* dummy;
    while(l->head != NULL){
        dummy = l->head;
        l->head = l->head->next;
        free(dummy->el);
        free(dummy);
    }
    free(l);
    l = NULL;
    return 0;
}

//FUNZIONI HASH TABLE
hashTable* createHashTable(int dimTab){
    hashTable* ret;
    GESTERRP(ret = malloc(sizeof(hashTable)), perror("malloc failed"); return NULL;)

    ret->dimTabella = dimTab * 2;
    GESTERRP(ret->vettTab = malloc(sizeof(list*) * ret->dimTabella),perror("malloc failed"); free(ret);
                        return NULL;)
    int i;
    for(i = 0; i < ret->dimTabella; i++){
        GESTERRP(ret->vettTab[i] = createList(compareH),
                 perror("malloc failed, fatal error"); exit(-1);)
    }

    return ret;
}
//aggiunge un elemento alla hash table, come primo della lista corrispondente
int addElH(hashTable* ht, char* key, void* el){
    GESTERRP(el, perror("el is NULL"); return -1;)
    GESTERRP(ht, perror("hashTable not initialized");return -1;)
    GESTERRP(key, perror("key is NULL");return -1;)

    hashNode* dummy = malloc(sizeof(hashNode));
    GESTERRP(dummy, return -1;)

    dummy->el = el;
    dummy->key = malloc(sizeof(char)*(strlen(key) + 1));
    GESTERRP(dummy->key, perror("malloc failed, fatal error"); exit(1);)

    strcpy(dummy->key, key);

    GESTERRI(addHeadL(  ht->vettTab[hashF(key, ht->dimTabella)] , dummy), return -1;)

    return 0;
}
//rimuove un elemento la cui chiave corrisponde a quella data
void* removeElH(hashTable* ht, char* key){
    GESTERRP(key, perror("key is NULL");return NULL;)
    GESTERRP(ht, perror("hashTable not initialized");return NULL;)

    hashNode* dummy = removeElL(ht->vettTab[hashF(key, ht->dimTabella)], key);
    GESTERRP(dummy,perror("Element not found"); return NULL;)

    void* ret = dummy->el;
    free(dummy->key);
    free(dummy);

    return ret;
}
//elimina completamente la lista senza lasciare leak di memoria
int freeAllH(hashTable* ht){
    int i;
    for(i = 0; i < ht->dimTabella ; i++){
        hashNode* dummy = removeLastL(ht->vettTab[i]);
        while(dummy != NULL){
            free(dummy->el);
            free(dummy->key);
            free(dummy);
            dummy = removeLastL(ht->vettTab[i]);
        }
        freeAllL(ht->vettTab[i]);
    }
    free(ht->vettTab);
    free(ht);
    return 0;
}














