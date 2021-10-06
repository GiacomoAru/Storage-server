#ifndef LIB_STRUTTUREDATI_H
#define LIB_STRUTTUREDATI_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "StruttureDati.h"



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
    list ** vettTab;
    int dimTabella;
} hashTable;
typedef struct hn{
    char* key;
    void* el;
} hashNode;


list* createList();
int addHeadL(list* l, void* el);
int addTailL(list* l, void* el);
void* removeFirstL(list* l);
void* removeLastL(list* l);
void* removeElL(list* l, void* el);
int freeAllL(list* l);

void printList(list* l);

hashTable* createHashTable(int dimTab);
int addElH(hashTable* ht, char* key, void* el);
void* removeElH(hashTable* ht, char* key);
int freeAllH(hashTable* ht);
#endif //LIB_STRUTTUREDATI_H
