#!/bin/bash
sleep 3 #attesa post avvio server

./client -f ./ssocket.sk -p -t 50 -W ./test2/0.5MB.txt,./test2/textFile10.txt >./test2/out.txt 2>./test2/err.txt
echo -------------------------------------------------------------------------------------------------------------- >>./test2/out.txt
echo -------------------------------------------------------------------------------------------------------------- >>./test2/err.txt
#FIFO -> 0.5MB espulso, LRU -> textFile10.txt e 0.5MB espulsi
./client -f ./ssocket.sk -p -t 50 -l ./test2/textFile10.txt -w ./test2/dir1 -D ./test2/espulsi >>./test2/out.txt 2>>./test2/err.txt
echo -------------------------------------------------------------------------------------------------------------- >>./test2/out.txt
echo -------------------------------------------------------------------------------------------------------------- >>./test2/err.txt

#invio di sighup al server
kill -s SIGHUP $(pidof ./server)