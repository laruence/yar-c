AUTOMAKE_OPTIONS=foreign  # don't check for NEWS ChangeLog etc 
lib_LTLIBRARIES=libyar.la
libyar_la_SOURCES=yar_server.c yar_client.c yar_response.c yar_request.c yar_pack.c yar_protocol.c yar_log.c
libyar_la_LDFLAGS=-levent -lmsgpackc
include_HEADERS=yar.h yar_common.h yar_server.h yar_client.h yar_response.h yar_request.h yar_pack.h yar_protocol.h yar_log.h
EXTRA_DIST=example/server.c example/client.c example/Makefile.in example/Makefile.am
