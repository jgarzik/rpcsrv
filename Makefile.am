
AM_CPPFLAGS = $(PTHREAD_CFLAGS)

sbin_PROGRAMS = rpcsrvd

rpcsrvd_SOURCES = \
	src/rpcsrv.cc

rpcsrvd_LDFLAGS = $(PTHREAD_CFLAGS) $(OPENSSL_LDFLAGS) -L./univalue/lib
rpcsrvd_LDADD = \
	-lunivalue
	$(PTHREAD_LIBS)		\
	$(OPENSSL_LIBS)

