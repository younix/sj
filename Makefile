CC ?= cc
CFLAGS := -std=c99 -pedantic -Wall -Wextra -O3 -g
CFLAGS_MXML := `pkg-config --cflags mxml`
LIBS_MXML := `pkg-config --libs mxml`

.PHONY: all test clean debug update
.SUFFIXES: .o .c

BINS=sj messaged iqd roster

all: $(BINS)
sj: sj.o sasl/sasl.o sasl/base64.o bxml/bxml.o
	$(CC) -o $@ $(LIBS_MXML) -lm \
	    sj.o sasl/sasl.o sasl/base64.o bxml/bxml.o

messaged: messaged.o bxml/bxml.o
	$(CC) -o $@ $(LIBS_MXML) messaged.o bxml/bxml.o

iqd: iqd.o bxml/bxml.o
	$(CC) -o $@ $(LIBS_MXML) iqd.o bxml/bxml.o

roster: roster.o
	$(CC) -o $@ $(LIBS_MXML) roster.o

sj.o: sj.c bxml/bxml.h sasl/sasl.h
	$(CC) $(CFLAGS) $(CFLAGS_MXML) -c -o $@ sj.c

messaged.o: messaged.c bxml/bxml.h
	$(CC) $(CFLAGS) $(CFLAGS_MXML) -c -o $@ messaged.c

iqd.o: iqd.c bxml/bxml.h
	$(CC) $(CFLAGS) $(CFLAGS_MXML) -c -o $@ iqd.c

roster.o: roster.c
	$(CC) $(CFLAGS) $(CFLAGS_MXML) -c -o $@ roster.c

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(BINS) *.o *.core expat
	cd bxml; $(MAKE) clean
	cd sasl; $(MAKE) clean

debug:
	#gdb sj sj.core
	gdb messaged messaged.core

update:
	cd bxml; git pull origin master
	cd sasl; git pull origin master

include bxml/Makefile.inc
include sasl/Makefile.inc
