CC=gcc
CFLAGS=-g -lpthread -std=c99

test: test
	$(CC) $(CFLAGS) -o test smallsh.c

clean:
	rm -rf *.o smallsh test
