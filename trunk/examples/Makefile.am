
INC = -I$(top_srcdir)/abus/libjson -I$(top_srcdir)/abus

AM_CPPFLAGS = $(INC)

EXTRA_PROGRAMS   = \
		example-service \
		example-client \
		example-svc-poll \
		example-svc-array \
		example-clnt-array \
		example-svc-attr \
		example-clnt-attr \
		example-clnt-multiattr \
		example-svc-event \
		example-clientpp \
 		example-servicepp \
		example-servicexx \
		example-json-config

if HAVE_EXAMPLES
bin_PROGRAMS = $(EXTRA_PROGRAMS)
endif

examples: $(EXTRA_PROGRAMS)
DISTCLEANFILES = $(EXTRA_PROGRAMS)

.PHONY: examples

EXTRA_DIST = \
			lighttpd.conf \
			jquery.js \
			jquery.jsonrpc.js \
			index.html \
			rpc.sh

example_service_SOURCES = example-service.c

example_client_SOURCES = example-client.c

example_svc_poll_SOURCES = example-svc-poll.c

example_svc_event_SOURCES = example-svc-event.c

example_clientpp_SOURCES = example-clientpp.cpp

example_servicepp_SOURCES = example-servicepp.cpp

example_servicexx_SOURCES = example-servicexx.cpp

example_svc_array_SOURCES = example-svc-array.c

example_clnt_array_SOURCES = example-clnt-array.c

example_svc_attr_SOURCES = example-svc-attr.c

example_clnt_attr_SOURCES = example-clnt-attr.c

example_clnt_multiattr_SOURCES = example-clnt-multiattr.c

LDADD    = $(top_builddir)/abus/libabus.la

