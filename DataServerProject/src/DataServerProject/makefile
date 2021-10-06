CC = gcc
CF = -Wall -g

test : src/Test.c compil/StruttureDati.a
	$(CC) $(CF) $< -o $@ -L ./compil/ ./compil/StruttureDati.a

compil/StruttureDati.o : src/StruttureDati.c
	$(CC) $(CF) -c $< -o $@

compil/StruttureDati.a : compil/StruttureDati.o
	ar rcs $@ $<

	
