ACLOCAL_AMFLAGS = -I m4

AM_MAKEFLAGS= --no-print-directory

SUBDIRS = m4 abus tools examples test doc

EXTRA_DIST = LICENSE abus.pc.in AUTHORS

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = abus.pc

DISTCLEANFILES = abus.pc

doc:
	make -C doc doc

examples:
	make -C examples examples

INC = -I$(top_srcdir)/abus/hashtab -I$(top_srcdir)/abus/libjson -I$(top_srcdir)/abus

cppcheck:
	cppcheck --enable=all -q -I $(top_builddir) $(INC) $(top_srcdir)/{abus,tools,examples}

.PHONY: doc examples cppcheck
