CC=gcc
CFLAGS=-g -std=c99

smallsh: smallsh.c
	$(CC) $(CFLAGS) -o smallsh smallsh.c

clean:
	rm -rf *.o smallsh test
