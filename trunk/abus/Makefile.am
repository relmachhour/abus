lib_LTLIBRARIES = libabus.la

INC = -I$(top_srcdir)/abus/hashtab -I$(top_srcdir)/abus/libjson -I$(top_srcdir)/abus

abusincludedir=$(includedir)/abus

AM_CPPFLAGS = $(INC)
AM_CFLAGS = -Wall
AM_CXXFLAGS = $(AM_CFLAGS)

libabus_la_SOURCES = jsonrpc.c abus.c sock_un.c sock_un.h
libabus_la_LDFLAGS = $(WINLDFLAGS) $(OSXLDFLAGS) -no-undefined -version-info 0:0:0
libabus_la_CFLAGS = $(AM_CFLAGS)
libabus_la_LIBADD = libjson/libjson.la hashtab/libhashtab.la -lrt

abusinclude_HEADERS = abus.h jsonrpc.h abus.hpp

SUBDIRS = hashtab libjson

cppcheck:
	cppcheck -q -I $(top_builddir) $(INC) $(top_srcdir)/abus/

.PHONY: cppcheck
