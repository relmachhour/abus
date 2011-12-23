ACLOCAL_AMFLAGS = -I m4

SUBDIRS = m4 abus tools examples doc

EXTRA_DIST = LICENSE abus.pc.in AUTHORS

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = abus.pc

DISTCLEANFILES = abus.pc

doc:
	make -C doc doc

examples:
	make -C examples examples

.PHONY: doc examples
