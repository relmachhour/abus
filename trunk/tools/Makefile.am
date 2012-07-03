
INC = -I$(top_srcdir)/abus/hashtab -I$(top_srcdir)/abus/libjson -I$(top_srcdir)/abus

AM_CPPFLAGS = $(INC)

bin_PROGRAMS   = abus-send abus-service @ABUS_CGI@

dist_man1_MANS = abus-send.1

EXTRA_PROGRAMS   = abus-cgi

abus_send_SOURCES  = abus-send.c
abus_send_LDADD    = $(top_builddir)/abus/libabus.la

abus_service_SOURCES  = abus-service.c
abus_service_LDADD    = $(top_builddir)/abus/libabus.la

# Require pkg libfcgi-dev
abus_cgi_SOURCES  = abus-cgi.c
abus_cgi_LDADD    = $(top_builddir)/abus/libabus.la -lfcgi

