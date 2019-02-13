
AM_CPPFLAGS = $(PTHREAD_CFLAGS)

EXTRA_DIST = \
	README.md \
	example-config.json

sbin_PROGRAMS = rpcsrvd

rpcsrvd_SOURCES = \
	src/rpcsrv.cc \
	src/util.h src/util.cc

rpcsrvd_LDFLAGS = $(PTHREAD_CFLAGS) $(OPENSSL_LDFLAGS) -L./univalue/lib
rpcsrvd_LDADD = \
	-lunivalue \
	$(PTHREAD_LIBS)		\
	$(ARGP_LIBS)		\
	$(EVENT_LIBS)		\
	$(OPENSSL_LIBS)

