## Process this file with automake to produce Makefile.in

INCLUDES = \
        -I../../amu -I../../include/ \
	-I$(top_srcdir)/intl \
	@GTK_CFLAGS@

bin_PROGRAMS = bewan_adsl_status

bewan_adsl_status_SOURCES = \
	main.c \
	support.c support.h \
	interface.c interface.h \
	callbacks.c callbacks.h \
	status.h \
	status.c

bewan_adsl_status_LDADD = @GTK_LIBS@ $(INTLLIBS)

