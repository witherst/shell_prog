CC=gcc
CFLAGS=-g -std=c99

test: smallsh.c
	$(CC) $(CFLAGS) -o test smallsh.c

clean:
	rm -rf *.o smallsh test
