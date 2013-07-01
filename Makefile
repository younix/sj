CFLAGS=$(pkg-config --cflags libxml-2.0)
CFLAGS=-I/usr/local/include/libxml2 -I/usr/local/include
LFLAGS=$(pkg-config --libs libxml-2.0)
#EXPAT=$(pkg-config --cflags --libs expat)
EXPAT=-I/usr/include -L/usr/lib -lexpat
LFLAGS=-L/usr/local/lib -lxml2

all: sj expat

sj: sj.c
	gcc -std=c99 -pedantic -g ${CFLAGS} ${LFLAGS} -o $@ $<

test: test.c
	gcc -std=c99 -pedantic -g ${CFLAGS} ${LFLAGS} -o $@ $<

expat: expat.c
	gcc -std=c99 -pedantic -g ${EXPAT} -o $@ $<
