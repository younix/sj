CFLAGS=-std=c99 -pedantic -Wall -Wextra -g
LD_EXPAT=-L/usr/lib -lexpat
CF_EXPAT=-I/usr/include

.PHONY: all test clean debug
.SUFFIXES: .o .c

all: sj
clean:
	rm -f sj *.o *.core
debug:
	gdb sj sj.core

sj: sj.o sasl/sasl.o sasl/base64.o
	gcc $(LD_EXPAT) -O3 -o $@ sj.o sasl/sasl.o sasl/base64.o -lm

.c.o:
	gcc $(CFLAGS) $(CF_EXPAT) -O0 -c -o $@ $<
