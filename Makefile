CC=gcc
CFLAGS=-O3 -DNDEBUG -Wall -Wextra
DEBUGFLAGS=-g -Wall -Wextra

bf: bf.c
	$(CC) $(CFLAGS) -o $@ bf.c

bf_debug: bf.c
	$(CC) $(DEBUGFLAGS) -o $@ bf.c

clean:
	rm -f bf *intermediate