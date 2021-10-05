#ifndef LIB_STRUTTUREDATI_H
#define LIB_STRUTTUREDATI_H

#include <stdlib.h>
#include <stdio.h>
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
    int elTot;
    int (*cmpf) (void*, void*);
} list;
//il tipo lista su cui applicare i metodi implementati
typedef struct h{
    list * vettTable;

} hashTable;

list* createList();
int addHead(list* l, void* el);
int addTail(list* l, void* el);
void* removeFirst(list* l);
void* removeLast(list* l);
void* removeEl(list* l, void* el);
int freeAll(list* l);
void printList(list* l);

#endif //LIB_STRUTTUREDATI_H
