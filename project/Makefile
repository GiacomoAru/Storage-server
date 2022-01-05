CC 			= gcc
CFLAGS		= -g -Wall
TARGETS		= server client

.PHONY: test1 clean1 cleanall
#genera tutti gli eseguibili
all : $(TARGETS)


# $< rappresenta il primo prerequisito (solitamente un file sorgente)
# $@ rappresenta il target che stiamo generando
server : src/server.c
	$(CC) $(CFLAGS) $< -o $@ -lpthread
client : src/client.c api
	$(CC) $(CFLAGS) $< -o $@ -L ./compil/ compil/api.a
api : compil/api.o
	ar rcs compil/api.a $<
compil/api.o : src/api.c
	$(CC) -g -c $< -o $@


#ripulisce tutto
#*~ ripulisce i files residui di emacs
cleanall : clean1 clean2
	-rm -f $(TARGETS) compil/* log.txt *.sk *~

stats :
	chmod +x ./script/stats.sh
	./script/stats.sh

#primo test
test1 : $(TARGETS)
	valgrind --leak-check=full ./server -cnfg ./test1/config.txt &
	chmod +x ./script/test1.sh
	./script/test1.sh &
clean1 :
	-rm -f test1/espulsi/* test1/letti/* test1/out.txt test1/err.txt log.txt

#secondo test
test2 : $(TARGETS)
	./server -cnfg ./test2/config.txt &
	chmod +x ./script/test2.sh
	./script/test2.sh &
clean2 :
	-rm -f test2/espulsi/* test2/out.txt test2/err.txt log.txt

#terzo test
test3 : $(TARGETS)
	./server -cnfg ./test3/config.txt &
	chmod +x ./script/test3.sh
	./script/test3.sh &

