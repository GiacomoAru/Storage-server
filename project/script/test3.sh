#!/bin/bash


mainpid=$$ #pid di questa shell

(sleep 32; kill -s SIGINT $(pidof ./server))&
sleep 2

# il test si assicura che vi siano sempre almeno 10 client connessi al server

#1
( while true
    do
    ./client -f ./ssocket.sk -t 0 -W ./test3/dir1/textFile1.txt -u ./test3/dir1/textFile1.txt -l ./test3/dir1/textFile1.txt -r ./test3/dir1/textFile1.txt -c ./test3/dir1/textFile1.txt,./test3/dir1/textFile1.txt
    done
)&
(sleep 30; kill $!;)&

#2
( while true
    do
    ./client -f ./ssocket.sk -t 0 -W ./test3/dir1/textFile2.txt -u ./test3/dir1/textFile2.txt -l ./test3/dir1/textFile2.txt -r ./test3/dir1/textFile2.txt -c ./test3/dir1/textFile2.txt,./test3/dir1/textFile1.txt
    done
)&
spid=$! #pid del processo più recente
(sleep 30; kill $spid;)&

#3
( while true
    do
    ./client -f ./ssocket.sk -t 0 -W ./test3/dir1/textFile3.txt -u ./test3/dir1/textFile3.txt -l ./test3/dir1/textFile3.txt -r ./test3/dir1/textFile3.txt -c ./test3/dir1/textFile3.txt,./test3/dir1/textFile2.txt
    done
)&
spid=$! #pid del processo più recente
(sleep 30; kill $spid;)&

#4
( while true
    do
    ./client -f ./ssocket.sk -t 0 -W ./test3/dir1/textFile4.txt -u ./test3/dir1/textFile4.txt -l ./test3/dir1/textFile4.txt -r ./test3/dir1/textFile4.txt -c ./test3/dir1/textFile4.txt,./test3/dir1/textFile3.txt
    done
)&
spid=$! #pid del processo più recente
(sleep 30; kill $spid;)&

#5
( while true
    do
    ./client -f ./ssocket.sk -t 0 -W ./test3/dir1/textFile5.txt -u ./test3/dir1/textFile5.txt -l ./test3/dir1/textFile5.txt -r ./test3/dir1/textFile5.txt -c ./test3/dir1/textFile5.txt,./test3/dir1/textFile4.txt
    done
)&
spid=$! #pid del processo più recente
(sleep 30; kill $spid;)&

#6
( while true
    do
    ./client -f ./ssocket.sk -t 0 -W ./test3/dir1/textFile6.txt -u ./test3/dir1/textFile6.txt -l ./test3/dir1/textFile6.txt -r ./test3/dir1/textFile6.txt -c ./test3/dir1/textFile6.txt,./test3/dir1/textFile5.txt
    done
)&
spid=$! #pid del processo più recente
(sleep 30; kill $spid;)&

#7
( while true
    do
    ./client -f ./ssocket.sk -t 0 -W ./test3/dir1/textFile7.txt -u ./test3/dir1/textFile7.txt -l ./test3/dir1/textFile7.txt -r ./test3/dir1/textFile7.txt -c ./test3/dir1/textFile7.txt,./test3/dir1/textFile6.txt
    done
)&
spid=$! #pid del processo più recente
(sleep 30; kill $spid;)&

#8
( while true
    do
    ./client -f ./ssocket.sk -t 0 -W ./test3/dir1/textFile8.txt -u ./test3/dir1/textFile8.txt -l ./test3/dir1/textFile8.txt -r ./test3/dir1/textFile8.txt -c ./test3/dir1/textFile8.txt,./test3/dir1/textFile7.txt
    done
)&
spid=$! #pid del processo più recente
(sleep 30; kill $spid;)&

#9
( while true
    do
    ./client -f ./ssocket.sk -t 0 -W ./test3/dir1/textFile9.txt -u ./test3/dir1/textFile9.txt -l ./test3/dir1/textFile9.txt -r ./test3/dir1/textFile9.txt -c ./test3/dir1/textFile9.txt,./test3/dir1/textFile8.txt
    done
)&
spid=$! #pid del processo più recente
(sleep 30; kill $spid;)&

#10
( while true
    do
    ./client -f ./ssocket.sk -t 0 -W ./test3/dir1/textFile10.txt -u ./test3/dir1/textFile10.txt -l ./test3/dir1/textFile10.txt -r ./test3/dir1/textFile10.txt -c ./test3/dir1/textFile10.txt,./test3/dir1/textFile9.txt
    done
)&
spid=$! #pid del processo più recente
(sleep 30; kill $spid;)&

#11
( while true
    do
      ./client -f ./ssocket.sk -t 0 -l ./fileInesistente,./fileInesistente2 &
      ./client -f ./ssocket.sk -t 0 -u ./fileInesistente,./fileInesistente2 &
      ./client -f ./ssocket.sk -t 0 -c ./fileInesistente,./fileInesistente2 &
      ./client -f ./ssocket.sk -t 0 -r ./test3/dir1/textFile1,./test3/dir1/textFile2 &
      ./client -f ./ssocket.sk -t 0 -W ./test3/dir1/textFile1,./test3/dir1/textFile2 &
      ./client -f ./ssocket.sk -t 0 -r ./test3/dir1/textFile1,./test3/dir1/textFile2,./test3/dir1/textFile3,./test3/dir1/textFile4,./test3/dir1/textFile5
    done
)&
spid=$! #pid del processo più recente
(sleep 30; kill $spid;)&
