# Standalone Makefile for the yar-c test suite.
#
# Build the library first in the parent directory:
#   ./configure && make
# then:
#   make -C tests
#
# Optional variables:
#   CC, CFLAGS        compiler configuration
#   EXTRA_CFLAGS      additional compile flags (eg. sanitizer flags)
#   EXTRA_LDFLAGS     additional link flags (eg. -L<dir> for libevent/msgpack)
#   EXTRA_LIBS        additional libraries

TOP = ..

CC ?= cc
CFLAGS ?= -O0 -g
EXTRA_CFLAGS ?=
EXTRA_LDFLAGS ?=
EXTRA_LIBS ?=

ALL_CFLAGS = $(CFLAGS) $(EXTRA_CFLAGS) -Wall -I$(TOP)
ALL_LDFLAGS = $(EXTRA_LDFLAGS)
LDLIBS = $(TOP)/.libs/libyar.a -levent -lmsgpackc $(EXTRA_LIBS)

all: yar_test_server yar_test_client

yar_test_server: test_server.c $(TOP)/.libs/libyar.a
	$(CC) $(ALL_CFLAGS) -o $@ test_server.c $(ALL_LDFLAGS) $(LDLIBS)

yar_test_client: test_client.c yar_test.h $(TOP)/.libs/libyar.a
	$(CC) $(ALL_CFLAGS) -o $@ test_client.c $(ALL_LDFLAGS) $(LDLIBS)

$(TOP)/.libs/libyar.a:
	@echo "error: $@ not found; run ./configure && make in $(TOP) first" >&2
	@exit 1

clean:
	rm -f yar_test_server yar_test_client
	rm -rf logs

.PHONY: all clean
