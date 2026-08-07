AUTOMAKE_OPTIONS=foreign
lib_LTLIBRARIES=libyar.la
libyar_la_SOURCES=yar_server.c yar_client.c yar_response.c yar_request.c yar_pack.c yar_protocol.c yar_log.c
libyar_la_LDFLAGS=-levent -lmsgpackc
include_HEADERS=yar.h yar_common.h yar_server.h yar_client.h yar_response.h yar_request.h yar_pack.h yar_protocol.h yar_log.h

# build the test binaries and run the whole suite (C suite + PHP interop);
# TEST_ARGS is forwarded to run_all.sh, pass a php binary to enable the
# PHP interop suite: make test TEST_ARGS="--php /path/to/php"
test: all
	$(MAKE) -C tests EXTRA_CFLAGS="$(CFLAGS)" EXTRA_LDFLAGS="$(LDFLAGS)"
	sh tests/run_all.sh $(TEST_ARGS)

check-local: test

.PHONY: test
