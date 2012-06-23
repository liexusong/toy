# $Id: Makefile,v 1.41 2011/11/13 08:54:20 mit-sato Exp $

PREFIX		= /usr/local
CC		= gcc

# for product build. (use BoehmGC)
CFLAGS		= -Wall -O3 -c -g -DHAS_GCACHE
#CFLAGS		= -Wall -c -g -DHAS_GCACHE
INCLUDE		= -I/usr/local/include -I.
LIB		= -L/usr/local/lib -lgc -lpthread -lonig -lpcl

# for memory debuging build.
#CFLAGS		= -c -g -DPROF -DHAS_GCACHE
#INCLUDE		= -I/usr/local/include -I.
#LIB		= -L/usr/local/lib -lonig -lpcl

# for profiling build.
#CFLAGS		= -c -g -pg -DPROF -DHAS_GCACHE
#INCLUDE		= -I/usr/local/include -I.
#LIB		= -pg -L/usr/local/lib -lonig -lpcl

HDRS		= bulk.h cell.h array.h error.h hash.h interp.h parser.h \
		  toy.h types.h config.h global.h cstack.h
SRCS		= bulk.c cell.c	array.c hash.c list.c parser.c types.c \
		  eval.c interp.c commands.c methods.c global.c cstack.c
OBJS		= bulk.o cell.o	array.o hash.o list.o parser.o types.o \
		  eval.o interp.o commands.o methods.o global.o cstack.o

all:		toysh

install:
	if [ ! -d $(PREFIX)/lib/toy ]; then mkdir $(PREFIX)/lib/toy; fi
	if [ ! -d $(PREFIX)/lib/toy/lib ]; then mkdir $(PREFIX)/lib/toy/lib; fi
	install -m 755 toysh $(PREFIX)/bin
	install -m 644 setup.toy $(PREFIX)/lib/toy
	install -m 644 lib/*.toy $(PREFIX)/lib/toy/lib

toysh:		$(OBJS) toysh.o
	$(CC) $(OBJS) toysh.o $(LIB) -o toysh

toysh.o:	$(SRCS) $(HDRS) toysh.c
	$(CC) $(CFLAGS) $(INCLUDE) toysh.c -o toysh.o

cell.o:		cell.c $(HDRS)
	$(CC) $(CFLAGS) $(INCLUDE) cell.c -o cell.o

array.o:	array.c $(HDRS)
	$(CC) $(CFLAGS) $(INCLUDE) array.c -o array.o

list.o:		list.c
	$(CC) $(CFLAGS) $(INCLUDE) list.c -o list.o

hash.o:		hash.c $(HDRS)
	$(CC) $(CFLAGS) $(INCLUDE) hash.c -o hash.o

bulk.o:		bulk.c $(HDRS)
	$(CC) $(CFLAGS) $(INCLUDE) bulk.c -o bulk.o

types.o:	types.c $(HDRS)
	$(CC) $(CFLAGS) $(INCLUDE) types.c -o types.o

parser.o:	parser.c $(HDRS)
	$(CC) $(CFLAGS) $(INCLUDE) parser.c -o parser.o

interp.o:	interp.c $(HDRS)
	$(CC) $(CFLAGS) $(INCLUDE) interp.c -o interp.o

eval.o:		eval.c $(HDRS)
	$(CC) $(CFLAGS) $(INCLUDE) eval.c -o eval.o

commands.o:	commands.c $(HDRS)
	$(CC) $(CFLAGS) $(INCLUDE) commands.c -o commands.o

methods.o:	methods.c $(HDRS)
	$(CC) $(CFLAGS) $(INCLUDE) methods.c -o methods.o

global.o:	global.c $(HDRS)
	$(CC) $(CFLAGS) $(INCLUDE) global.c -o global.o

cstack.o:	cstack.c $(HDRS)
	$(CC) $(CFLAGS) $(INCLUDE) cstack.c -o cstack.o

config.h:	config.h.in
	sed 	-e s%@PREFIX@%$(PREFIX)%g \
		-e s%@VERSION@%`head -1 RELEASE | awk '{print $$3}'`%g \
		< config.h.in > config.h

clean:
	rm -f *.o toysh *~ lib/*~ *core* *.gmon config.h
