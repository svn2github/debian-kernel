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

all: unicorn_status unicorn_status.cgi oam_ping

unicorn_status:	main.c unicorn_status.c unicorn_device.c
	$(CC) -DVERS=$(VERS) $(CFLAGS) $(INCLUDES) main.c unicorn_status.c unicorn_device.c  -o unicorn_status

unicorn_status.cgi:	cgi.c unicorn_status.c unicorn_device.c
	$(CC) -DVERS=$(VERS) $(CFLAGS) $(INCLUDES) cgi.c unicorn_status.c unicorn_device.c -o unicorn_status.cgi

oam_ping:	oam_ping.c unicorn_device.c
	$(CC) -DVERS=$(VERS) $(CFLAGS) $(INCLUDES) oam_ping.c unicorn_device.c -o oam_ping

clean:
	rm -f unicorn_status unicorn_status.cgi oam_ping *.o

install: all  
	$(INSTALL) -s -c unicorn_status $(DESTDIR)$(BINDIR)
	$(INSTALL) -s -c oam_ping $(DESTDIR)$(BINDIR)
