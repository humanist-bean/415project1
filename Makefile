CFLAGS=-W -Wall -I/usr/local/include
LDFLAGS=-L/usr/local/lib
PROGS=uspsv1

all: $(PROGS)

uspsv1: uspsv1.o p1fxns.o
	gcc $(LDFLAGS) -o $@ $^


clean:
	rm -f *.o $(PROGS)

uspsv1.o: uspsv1.c p1fxns.h
