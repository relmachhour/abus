
noinst_LTLIBRARIES = libjson.la

libjson_la_SOURCES = json.c

nobase_include_HEADERS = json.h

EXTRA_PROGRAMS = jsonlint

jsonlint_SOURCES = jsonlint.c
jsonlint_LDADD = libjson.la

EXTRA_DIST = README.md documentation

# TODO: tests/

