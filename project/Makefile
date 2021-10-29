CC 			= gcc
CFLAGS		= -g -Wall
TARGETS		= server client

#genera tutti gli eseguibili
all : $(TARGETS)


server : src/server.c
	$(CC) $(CFLAGS) $< -o $@ -lpthread

client : src/client.c compil/api.a
	$(CC) $(CFLAGS) $< -o $@ -L ./compil/ compil/api.a

compil/api.o : src/api.c
	$(CC) -g -c $< -o $@

compil/api.a : compil/api.o
	ar rcs $@ $<

