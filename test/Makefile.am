
INC = -I$(top_srcdir)/abus/hashtab -I$(top_srcdir)/abus/libjson -I$(top_srcdir)/abus

EXTRA_PROGRAMS   = \
		abus-test

DISTCLEANFILES = $(EXTRA_PROGRAMS)

# ----------------------------------------------------------------
#                        Unit Tests
# ----------------------------------------------------------------
# Listing of the program to launch
# Those programs should not be installed
# ----------------------------------------------------------------
if HAVE_TEST
TESTS = $(EXTRA_PROGRAMS)
check_PROGRAMS = $(TESTS)
endif

# ------------------------------------------------------------------
#                   Automatic unitary tests using gtest
# ------------------------------------------------------------------
#                             Assertion
# ------------------------------------------------------------------
abus_test_SOURCES  = \
					 abus-tor.cpp \
					 abus-svc.cpp \
					 abus-attr.cpp \
					 abus-event.cpp \
					 abus-introspect.cpp \
					 AbusTest.cpp \
					 AbusTest.hpp \
					 json-config.cpp

abus_test_CPPFLAGS = $(INC) $(GTEST_CFLAGS) \
					-O1		\
					-g		\
					-Wall	\
					-Wextra

abus_test_LDADD    = $(top_builddir)/abus/libabus.la \
						$(GTEST_LIBS) -lgtest_main
