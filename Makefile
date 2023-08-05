CC ?= cc

DEFINES	:= $(DEFINES) -D_POSIX_C_SOURCE=201405L -D_XOPEN_SOURCE=700
DEFINES	:= $(DEFINES) -D_BSD_SOURCE

DEBUG	:= -g
LDFLAGS	:=
CFLAGS	:= -std=c99 -pedantic -Wall -Wextra -O3 $(DEBUG) $(DEFINES)
CFLAGS_MXML := `pkg-config --cflags mxml`
LIBS_MXML := `pkg-config --libs mxml`

.PHONY: all tests clean debug update install
.SUFFIXES: .o .c

BINS=sj messaged presenced iqd roster presence xmpp_time

all: $(BINS)

# core deamon
sj: sj.o sasl/sasl.o sasl/base64.o bxml/bxml.o
	$(CC) -o $@ $(LDFLAGS) sj.o sasl/sasl.o sasl/base64.o bxml/bxml.o\
	     $(LIBS_MXML) $(LIBS_BSD) -lm

messaged: messaged.o bxml/bxml.o
	$(CC) -o $@ $(LDFLAGS) messaged.o bxml/bxml.o $(LIBS_MXML) $(LIBS_BSD)

presenced: presenced.o bxml/bxml.o
	$(CC) -o $@ $(LDFLAGS) presenced.o bxml/bxml.o $(LIBS_MXML) $(LIBS_BSD)

iqd: iqd.o bxml/bxml.o
	$(CC) -o $@ $(LDFLAGS) iqd.o bxml/bxml.o $(LIBS_MXML)

# commandline tools
roster: roster.o
	$(CC) -o $@ $(LDFLAGS) roster.o $(LIBS_MXML)

presence: presence.o
	$(CC) -o $@ $(LDFLAGS) presence.o

# extensions
xmpp_time: xmpp_time.o
	$(CC) -o $@ $(LDFLAGS) xmpp_time.o $(LIBS_MXML)

xmpp_time.o: xmpp_time.c
	$(CC) $(CFLAGS) $(CFLAGS_MXML) -c -o $@ xmpp_time.c

sj.o: sj.c bxml/bxml.h sasl/sasl.h
	$(CC) $(CFLAGS) $(CFLAGS_MXML) -c -o $@ sj.c

messaged.o: messaged.c bxml/bxml.h
	$(CC) $(CFLAGS) $(CFLAGS_MXML) $(CFLAGS_BSD) -c -o $@ messaged.c

presenced.o: presenced.c bxml/bxml.h
	$(CC) $(CFLAGS) $(CFLAGS_MXML) $(CFLAGS_BSD) -c -o $@ presenced.c

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

install:
	mkdir -p ${HOME}/bin
	cp $(BINS) ${HOME}/bin

tests:
	cd tests && ./test.sh

include bxml/Makefile.inc
include sasl/Makefile.inc
