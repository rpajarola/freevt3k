#AM_CFLAGS = -Wall -Werror --pedantic -O2

AM_CFLAGS = -O2 \
	@X_CFLAGS@ \
	-DSHOW_DC1_COUNT=1 \
	-DDEBUG_BREAK=0 \
	-DIGNORE_KEYBOARD_LOCK=1 \
	-DIGNORE_USER_SYSTEM_LOCK=1 \
	-DDEBUG_BLOCK_MODE=1 \
	-DMEMLOCK_2000

xhpterm_LDADD = @X_LIBS@ -lX11
xhpterm_CFLAGS = -DXHPTERM $(AM_CFLAGS)

bin_PROGRAMS = freevt3k xhpterm

freevt3k_SOURCES = dumpbuf.c dumpbuf.h freevt3k.c freevt3k.h hpvt100.c hpvt100.h timers.c timers.h typedef.h vmstypes.h vtconn.c vtconn.h vt.h

xhpterm_SOURCES = conmgr.c conmgr.h dumpbuf.c dumpbuf.h freevt3k.c freevt3k.h getcolor.c hpterm.c hpterm.h hpvt100.c hpvt100.h rlogin.c rlogin.h timers.c timers.h tty.c tty.h typedef.h vmstypes.h vt3kglue.c vt3kglue.h vtconn.c vtconn.h vt.h x11glue.c x11glue.h

MAINTAINERCLEANFILES = Makefile.in

maintainerclean-local:
	rm -rf .deps
