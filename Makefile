CC ?= cc

DEFINES	:= $(DEFINES) -D_POSIX_C_SOURCE=201405L -D_XOPEN_SOURCE=700
DEFINES	:= $(DEFINES) -D_BSD_SOURCE
DEFINES	:= $(DEFINES) -D_GNU_SOURCE
DEFINES	:= $(DEFINES) -D_DARWIN_C_SOURCE	# For MacOSX

CFLAGS	:= -std=c99 -pedantic -Wall -Wextra -O3 -g $(DEFINES)
CFLAGS_MXML := `pkg-config --cflags mxml`
LIBS_MXML := `pkg-config --libs mxml`

.PHONY: all test clean debug update install
.SUFFIXES: .o .c

BINS=sj messaged iqd roster presence

all: $(BINS)
sj: sj.o sasl/sasl.o sasl/base64.o bxml/bxml.o
	$(CC) -o $@ $(LIBS_MXML) -lm \
	    sj.o sasl/sasl.o sasl/base64.o bxml/bxml.o

messaged: messaged.o bxml/bxml.o
	$(CC) -o $@ $(LIBS_MXML) $(LIBS_BSD) messaged.o bxml/bxml.o

iqd: iqd.o bxml/bxml.o
	$(CC) -o $@ $(LIBS_MXML) iqd.o bxml/bxml.o

roster: roster.o
	$(CC) -o $@ $(LIBS_MXML) roster.o

presence: presence.o
	$(CC) -o $@ presence.o

sj.o: sj.c bxml/bxml.h sasl/sasl.h
	$(CC) $(CFLAGS) $(CFLAGS_MXML) -c -o $@ sj.c

messaged.o: messaged.c bxml/bxml.h
	$(CC) $(CFLAGS) $(CFLAGS_MXML) $(CFLAGS_BSD) -c -o $@ messaged.c

iqd.o: iqd.c bxml/bxml.h
	$(CC) $(CFLAGS) $(CFLAGS_MXML) -c -o $@ iqd.c

roster.o: roster.c
	$(CC) $(CFLAGS) $(CFLAGS_MXML) -c -o $@ roster.c

presence.o: presence.c
	$(CC) $(CFLAGS) -c -o $@ presence.c

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

install:
	mkdir -p ${HOME}/bin
	cp $(BINS) ${HOME}/bin

include bxml/Makefile.inc
include sasl/Makefile.inc
