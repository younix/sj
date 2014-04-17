CFLAGS=-std=c99 -pedantic -Wall -Wextra -O3 -g
NOWARNING=-Wno-unused
CFLAGS_MXML=`pkg-config --cflags mxml`
LIBS_MXML=`pkg-config --libs mxml`

.PHONY: all test clean debug update
.SUFFIXES: .o .c

BINS=sj messaged

all: $(BINS)
sj: sj.o sasl/sasl.o sasl/base64.o bxml/bxml.o
	cc -o $@ sj.o sasl/sasl.o sasl/base64.o bxml/bxml.o $(LIBS_MXML) -lm

messaged: messaged.o bxml/bxml.o
	cc -o $@ messaged.o bxml/bxml.o $(LIBS_MXML)

sj.o: sj.c bxml/bxml.h sasl/sasl.h
	cc $(CFLAGS) $(CFLAGS_MXML) $(NOWARNING) -c -o $@ sj.c

messaged.o: messaged.c bxml/bxml.h
	cc $(CFLAGS) $(CFLAGS_MXML) -c -o $@ messaged.c

.c.o:
	cc $(CFLAGS) -c -o $@ $<

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
