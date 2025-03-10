#AM_CFLAGS = -Wall -Werror --pedantic -O2 @X_CFLAGS@
AM_CFLAGS = -O2 @X_CFLAGS@

xhpterm_LDADD = @X_LIBS@ -lX11
xhpterm_CFLAGS = -DXHPTERM $(AM_CFLAGS)

bin_PROGRAMS = freevt3k xhpterm

freevt3k_SOURCES = dumpbuf.c dumpbuf.h freevt3k.c freevt3k.h hpvt100.c hpvt100.h timers.c timers.h vtconn.c vtconn.h vt.h

xhpterm_SOURCES = conmgr.c conmgr.h dumpbuf.c dumpbuf.h freevt3k.c freevt3k.h getcolor.c hpterm.c hpterm.h hpvt100.c hpvt100.h rlogin.c rlogin.h timers.c timers.h tty.c tty.h vt3kglue.c vt3kglue.h vtconn.c vtconn.h vt.h x11glue.c x11glue.h

MAINTAINERCLEANFILES = Makefile.in

maintainerclean-local:
	rm -rf .deps
