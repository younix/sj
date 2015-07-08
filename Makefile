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

BINS=sj messaged presenced iqd roster presence

all: $(BINS)

# core deamon
sj: sj.o sasl/sasl.o sasl/base64.o bxml/bxml.o
	@echo "Build: $@"
	@$(CC) -o $@ $(LDFLAGS) $(LIBS_MXML) $(LIBS_BSD) -lm \
	    sj.o sasl/sasl.o sasl/base64.o bxml/bxml.o

messaged: messaged.o bxml/bxml.o
	@echo "Build: $@"
	@$(CC) -o $@ $(LDFLAGS) $(LIBS_MXML) $(LIBS_BSD) messaged.o bxml/bxml.o

presenced: presenced.o bxml/bxml.o
	@echo "Build: $@"
	@$(CC) -o $@ $(LDFLAGS) $(LIBS_MXML) $(LIBS_BSD) presenced.o bxml/bxml.o

iqd: iqd.o bxml/bxml.o
	@echo "Build: $@"
	@$(CC) -o $@ $(LDFLAGS) $(LIBS_MXML) iqd.o bxml/bxml.o

# commandline tools
roster: roster.o
	@echo "Build: $@"
	@$(CC) -o $@ $(LDFLAGS) $(LIBS_MXML) roster.o

presence: presence.o
	@echo "Build: $@"
	@$(CC) -o $@ $(LDFLAGS) presence.o

sj.o: sj.c bxml/bxml.h sasl/sasl.h
	@echo "Build: $@"
	@$(CC) $(CFLAGS) $(CFLAGS_MXML) -c -o $@ sj.c

messaged.o: messaged.c bxml/bxml.h
	@echo "Build: $@"
	@$(CC) $(CFLAGS) $(CFLAGS_MXML) $(CFLAGS_BSD) -c -o $@ messaged.c

presenced.o: presenced.c bxml/bxml.h
	@echo "Build: $@"
	@$(CC) $(CFLAGS) $(CFLAGS_MXML) $(CFLAGS_BSD) -c -o $@ presenced.c

iqd.o: iqd.c bxml/bxml.h
	@echo "Build: $@"
	@$(CC) $(CFLAGS) $(CFLAGS_MXML) -c -o $@ iqd.c

roster.o: roster.c
	@echo "Build: $@"
	@$(CC) $(CFLAGS) $(CFLAGS_MXML) -c -o $@ roster.c

.c.o:
	@echo "Build: $@"
	@$(CC) $(CFLAGS) -c -o $@ $<

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
	cd tests && test.sh

include bxml/Makefile.inc
include sasl/Makefile.inc
