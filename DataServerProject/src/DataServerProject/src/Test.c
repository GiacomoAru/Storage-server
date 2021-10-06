#include "StruttureDati.h"

int compareInt(void* a, void* b){return *((int*)(a))  == *((int*)(b));}

int testStruttureDati(){
    int i;


    int * dummy;
    list* l = NULL;

    l = createList(compareInt);

    printList(l);

    printList(l);

    for(i = 0; i<100; i++){
        dummy = malloc(sizeof(int));
        *dummy = i*4;
        if(i%2) {
            addHeadL(l, dummy);
        }
        else{
            addTailL(l, dummy);
        }

    }

    printList(l);
    free(removeFirstL(l));
    free(removeLastL(l));
    free(removeFirstL(l));
    free(removeLastL(l));
    free(removeFirstL(l));
    free(removeLastL(l));
    printList(l);

    int d = 368;
    free(removeElL(l, &d));
    printList(l);

    for(i = 0; i<100; i+=8){
        free(removeElL(l, &i));
        printf("%d\n", i);
    }
    printList(l);
    for(i = 0; i<100; i++){
        free(removeFirstL(l));
    }
    printList(l);

    freeAllL(l);


    printf("\n####################TEST HASHTABLE####################\n");

    hashTable* ht = createHashTable(100);

    for(i = 0; i<100; i++){
        int* dummy = malloc(sizeof(int));
        *dummy = i*2 + 3;
        char s[20];
        sprintf(s,"Elemento n.%d key", *dummy);

        addElH(ht, s, dummy);
    }
    for(i = 20; i<40; i++){
        printf("rimosso:%d\n", i);
        int dummy = i*2 + 3;
        char s[20];
        sprintf(s,"Elemento n.%d key", dummy);

        free(removeElH(ht,s));
    }

    for(i = 0; i<1000; i++){
        int* dummy = malloc(sizeof(int));
        *dummy = i*5 + 3;
        char s[60];
        sprintf(s,"Elemento n.%d key ma più lunga%d%d", *dummy, *dummy, *dummy);

        addElH(ht, s, dummy);
    }
    for(i = 100; i<1000; i += 3){
        printf("rimosso:%d\n", i);
        int dummy = i*5 + 3;
        char s[60];
        sprintf(s,"Elemento n.%d key ma più lunga%d%d", dummy, dummy, dummy);

        free(removeElH(ht,s));
    }

    freeAllH(ht);
    return 0;
}
int main(){
    testStruttureDati();
    return 1;
}

