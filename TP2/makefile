CC=gcc
CFLAGS=-I.
DEPS = llAPI.h

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

prod: noncanonical.c writenoncanonical.c
	gcc -Wall -o noncanonical noncanonical.c -std=c11
	gcc -Wall -o writenoncanonical writenoncanonical.c -std=c11

debug: noncanonical.c writenoncanonical.c
		gcc -g -Wall -o noncanonical noncanonical.c -std=c11 -D DEBUG
		gcc -g -Wall -o writenoncanonical writenoncanonical.c -std=c11 -D DEBUG

test: llReadTest.c llWriteTest.c
		gcc -g -Wall llReadTest.c -o llReadTest -std=c11
		gcc -g -Wall llWriteTest.c -o llWriteTest -std=c11

clean:
	rm noncanonical
	rm writenoncanonical
