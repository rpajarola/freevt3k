#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([freevt3k],[1.0],[rp@servium.ch])
AM_INIT_AUTOMAKE([-Wall -Werror gnu subdir-objects])
AC_PROG_CC

AC_USE_SYSTEM_EXTENSIONS
AC_CHECK_HEADERS([ctype.h sys/socket.h netinet/in.h sys/time.h termios.h])

AC_PATH_XTRA

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
 Makefile
 src/Makefile
])
AC_OUTPUT
