## Process this file with autoconf to create configure. -*- autoconf -*-

AC_PREREQ(2.59)

AC_INIT([abus],[0.1],[abus-discuss@googlegroups.com],[abus],[http://code.google.com/p/abus/])
AC_CONFIG_SRCDIR(abus/abus.c)
AC_CONFIG_MACRO_DIR([m4])

## ------------------------ ##
## Automake Initialisation. ##
## ------------------------ ##
AM_INIT_AUTOMAKE([1.11 foreign tar-pax -Wall -Werror -Wno-portability])
AM_SILENT_RULES([yes])
AM_MAINTAINER_MODE

AC_CANONICAL_HOST
#AC_CANONICAL_TARGET
AC_SUBST(PACKAGE_VERSION)

dnl Checks for programs.
AC_C_CONST
AC_ISC_POSIX
AC_HEADER_STDC
AC_PROG_CC
AC_PROG_CC_STDC
AC_PROG_CXX
AC_PROG_CPP
AC_PROG_LN_S
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AC_PROG_MAKE_SET
AM_PROG_CC_C_O

AC_CONFIG_HEADERS([abus_config.h])

AC_DEFINE([_GNU_SOURCE],[1],[Use GNU C library extensions (e.g. stdndup).])

ACX_PTHREAD([], [AC_MSG_ERROR([Unable to find pthread support])])
LIBS="$PTHREAD_LIBS $LIBS"
CFLAGS="$CFLAGS $PTHREAD_CFLAGS"

AC_ARG_ENABLE(fcgi,
	AS_HELP_STRING([--enable-fcgi], [Enable FastCGI gateway [default=no]]),
	[case "${enableval}" in
		yes) have_fcgi=true ;;
		no)  have_fcgi=false ;;
		*)   AC_MSG_ERROR(bad value ${enableval} for --enable-fcgi) ;;
	 esac],
	[have_fcgi=false])
AC_MSG_CHECKING([Checking the FastCGI support])
if test "$have_fcgi" = "true" ; then
	AC_MSG_RESULT([yes])
	save_LIBS="$LIBS"
	AC_CHECK_LIB(fcgi, FCGI_Accept, [], [AC_MSG_ERROR([Unable to find fcgi library])], [])
	LIBS="$save_LIBS"
	ABUS_CGI="abus-cgi"
else
	AC_MSG_RESULT([no])
	ABUS_CGI=
fi
AC_SUBST(ABUS_CGI)

AC_OUTPUT([
m4/Makefile
abus/Makefile
abus/libjson/Makefile
abus/hashtab/Makefile
examples/Makefile
doc/Makefile
doc/Doxyfile
tools/Makefile
Makefile
abus.pc
])

