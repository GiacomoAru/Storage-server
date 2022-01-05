#!/bin/bash

#entra nella cartella script, se non esiste termina
cd script
echo ''
tail -n 14 ../log.txt

#scrive nello stdout
echo
echo  Operazioni svolte da ogni thread:

#grep seleziona le righe contenetni "["
#cut taglia le riche e selezione il primo elemento
#sort -g riordina in modo crescente i numeri presenti nelle riche
#uniq elimina i duplicati
grep / ../log.txt | cut -d "[" -f 2| cut -d "]" -f 1 | sort -g | uniq | while read thread
do
    echo -n "  "$thread": "
    grep -c $thread ../log.txt
done
