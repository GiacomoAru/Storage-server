CC 			= gcc
CFLAGS		= -g -Wall
TARGETS		= server

all : $(TARGETS)

server : src/server.c
	$(CC) $(CFLAGS) $< -o $@ -lpthread

compil/api.o : src/api.c
	$(CC) -g -c $< -o $@
compil/api.a : compil/api.o
	ar rcs $@ $<
