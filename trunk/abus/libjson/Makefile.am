
abusincludedir=$(includedir)/abus

noinst_LTLIBRARIES = libjson.la

libjson_la_SOURCES = json.c

abusinclude_HEADERS = json.h

EXTRA_PROGRAMS = jsonlint

jsonlint_SOURCES = jsonlint.c
jsonlint_LDADD = libjson.la

EXTRA_DIST = README.md documentation

# TODO: tests/

