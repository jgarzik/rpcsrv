
SUBDIRS = json

INCLUDES = $(PTHREAD_CFLAGS) $(BOOST_CPPFLAGS)

sbin_PROGRAMS = rpcsrvd

rpcsrvd_SOURCES = \
	connection.h		\
	header.h		\
	mime_types.h		\
	reply.h			\
	request.h		\
	request_handler.h	\
	request_parser.h	\
	rpcs.h			\
	server.h		\
				\
	connection.cc		\
	main.cc			\
	mime_types.cc		\
	reply.cc		\
	request_handler.cc	\
	request_parser.cc	\
	rpcs.cc			\
	server.cc

rpcsrvd_LDFLAGS = $(PTHREAD_CFLAGS) $(OPENSSL_LDFLAGS)
rpcsrvd_LDADD = \
	$(PTHREAD_LIBS)		\
	-Ljson -ljson_spirit	\
	$(OPENSSL_LIBS)		\
	$(BOOST_LDFLAGS) $(BOOST_SYSTEM_LIB) $(BOOST_THREAD_LIB) \
	$(BOOST_DATE_TIME_LIB) $(BOOST_PROGRAM_OPTIONS_LIB)

