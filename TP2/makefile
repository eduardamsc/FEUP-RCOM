CC=gcc
CFLAGS=-I.
DEPS = llAPI.h

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

canonmake: noncanonical.c
	gcc -Wall -o noncanonical noncanonical.c -std=c11
	gcc -Wall -o writenoncanonical writenoncanonical.c -std=c11

clean:
	rm noncanonical
	rm writenoncanonical
