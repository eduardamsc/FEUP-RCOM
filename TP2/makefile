CC=gcc
CFLAGS=-I.
DEPS = llAPI.h

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

canonmake: noncanonical.c
	gcc -o noncanonical noncanonical.c -I.
	gcc -o writenoncanonical writenoncanonical.c -I.
