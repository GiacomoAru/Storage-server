#include "StruttureDati.h"

int compareInt(void* a, void* b){return *((int*)(a))  == *((int*)(b));}

int main(){
    int * dummy;
    list* l = NULL;
    
    
    
    l = createList(compareInt);

    
    printList(l);
    
    
    int i;
	
	printList(l);
	
    for(i = 0; i<100; i++){
    	dummy = malloc(sizeof(int));
        *dummy = i*4;
        if(i%2) {
            addHead(l, dummy);
        }
        else{
            addTail(l, dummy);
        }

    }
    
    printList(l);
    free(removeFirst(l));
    free(removeLast(l));
    free(removeFirst(l));
    free(removeLast(l));
    free(removeFirst(l));
    free(removeLast(l));
    printList(l);

    int d = 368;
    free(removeEl(l, &d));
    printList(l);

    for(i = 0; i<100; i+=8){
        free(removeEl(l, &i));
        printf("%d\n", i);
    }
    printList(l);

    freeAll(l);
    return 0;
}

