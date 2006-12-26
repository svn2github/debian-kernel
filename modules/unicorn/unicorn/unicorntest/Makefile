CC=gcc
LD=ld
INSTALL=install
CFLAGS := -Wall -Wstrict-prototypes -O2 -DLINUX
INCLUDES := -I../amu/ -I../include/
prefix=/usr/local
BINDIR=$(prefix)/bin
ifndef VERS
VERS=0
endif

all: unicorntest

unicorntest: unicorntest.c
	$(CC) -DVERS=$(VERS) $(CFLAGS) $(INCLUDES) unicorntest.c -o unicorntest

clean:
	rm -f unicorntest *.o

install: all  
	$(INSTALL) -c unicorntest $(DESTDIR)$(BINDIR)
