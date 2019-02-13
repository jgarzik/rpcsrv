
AM_CPPFLAGS = $(PTHREAD_CFLAGS) -I$(srcdir)/univalue/include

SUBDIRS = univalue

EXTRA_DIST = \
	README.md \
	example-config.json

sbin_PROGRAMS = rpcsrvd

rpcsrvd_SOURCES = \
	src/rpcsrv.cc \
	src/util.h src/util.cc

rpcsrvd_LDFLAGS = $(PTHREAD_CFLAGS) $(OPENSSL_LDFLAGS)
rpcsrvd_LDADD = \
	univalue/.libs/libunivalue.a \
	$(PTHREAD_LIBS)		\
	$(ARGP_LIBS)		\
	$(EVENT_LIBS)		\
	$(OPENSSL_LIBS)

