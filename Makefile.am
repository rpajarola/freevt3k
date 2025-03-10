AUTOMAKE_OPTIONS = -Wall -Werror gnu
SUBDIRS = src

DISTCLEANFILES = *~
MAINTAINERCLEANFILES = INSTALL Makefile Makefile.in aclocal.m4 compile config.guess config.h config.h.in config.log config.status config.sub configure depcomp install-sh missing

distclean-local:
	rm -rf autom4te.cache

maintainerclean-local:
	autoreconf -i
	automake --add-missing -c
