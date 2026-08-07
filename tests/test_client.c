/**
 * Yar C test suite - C client test cases
 *
 * Usage:
 *   yar_test_client --uri <tcp://host:port | /path/to/sock>   run the test suite
 *   yar_test_client --uri <...> --probe                       wait until the server accepts connections
 *   yar_test_client --uri <...> --concurrent                  run only the concurrency test
 *   yar_test_client --uri <...> --packager <msgpack|json>     wire protocol packager (default msgpack)
 *
 * Copyright (C) 2026 Xinchen Hui <laruence at gmail dot com>
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <arpa/inet.h>

#include "yar.h"
#include "yar_test.h"

#define TEST_ERR_EXCEPTION 0x40 /* keep in sync with YAR_ERR_EXCEPTION in php-yar */

static char *test_uri = NULL;
static int test_is_tcp = 0;
static int test_packager = YAR_PACKAGER_MSGPACK;

/* helpers {{{ */
static yar_client * new_client_timeout(int timeout) {
	yar_client *client = yar_client_init(test_uri);
	if (client) {
		int packager = test_packager;
		yar_client_set_opt(client, YAR_CONNECT_TIMEOUT, &timeout);
		if (packager != YAR_PACKAGER_MSGPACK) {
			yar_client_set_opt(client, YAR_OPT_PACKAGER, &packager);
		}
	}
	return client;
}

static yar_client * new_client(void) {
	return new_client_timeout(8);
}

static void free_response(yar_response *response) {
	if (response) {
		yar_response_free(response);
		free(response);
	}
}

/* msgpack deserializes non-negative integers as ULONG and negative ones as
 * LONG; accept both and extract into a long */
static int data_as_long(const yar_data *data, long *out) {
	unsigned int size = 0;
	int type;

	if (!data) {
		return 0;
	}
	type = yar_unpack_data_type(data, &size);
	if (type == YAR_DATA_LONG) {
		return yar_unpack_data_long(data, out);
	}
	if (type == YAR_DATA_ULONG) {
		ulong uval = 0;
		if (yar_unpack_data_ulong(data, &uval) != YAR_DATA_ULONG) {
			return 0;
		}
		*out = (long)uval;
		return YAR_DATA_LONG;
	}
	return 0;
}

static const yar_data * array_at(const yar_data *array, unsigned int idx) {
	unsigned int size = 0, i = 0;
	const yar_data *current = NULL;
	yar_unpack_iterator *it;

	if (!array || yar_unpack_data_type(array, &size) != YAR_DATA_ARRAY || size <= idx) {
		return NULL;
	}

	it = yar_unpack_iterator_init(array);
	do {
		current = yar_unpack_iterator_current(it);
		if (i == idx) {
			yar_unpack_iterator_free(it);
			return current;
		}
		i++;
	} while (yar_unpack_iterator_next(it));
	yar_unpack_iterator_free(it);

	return NULL;
}

/* look up a string key in a map, returns the value data */
static const yar_data * map_get(const yar_data *map, const char *key, unsigned int klen) {
	unsigned int size = 0, i = 0;
	const yar_data *current = NULL;
	yar_unpack_iterator *it;

	if (!map || yar_unpack_data_type(map, &size) != YAR_DATA_MAP) {
		return NULL;
	}

	it = yar_unpack_iterator_init(map);
	do {
		current = yar_unpack_iterator_current(it);
		if (i % 2 == 0) {
			unsigned int len = 0;
			const char *str = NULL;
			if (yar_unpack_data_type(current, &len) == YAR_DATA_STRING
					&& yar_unpack_data_string(current, &str) == YAR_DATA_STRING
					&& len == klen && strncmp(str, key, klen) == 0) {
				if (yar_unpack_iterator_next(it)) {
					current = yar_unpack_iterator_current(it);
					yar_unpack_iterator_free(it);
					return current;
				}
			}
		}
		i++;
	} while (yar_unpack_iterator_next(it));
	yar_unpack_iterator_free(it);

	return NULL;
}

/* raw blocking connection to the test server, for protocol abuse tests */
static int raw_connect(void) {
	int fd;

	if (test_is_tcp) {
		char host[256];
		char *delim, *copy = strdup(test_uri + sizeof("tcp://") - 1);
		struct hostent *hptr;
		struct sockaddr_in sa;
		int port;

		if (!(delim = strchr(copy, ':'))) {
			free(copy);
			return -1;
		}
		if ((size_t)(delim - copy) >= sizeof(host)) {
			free(copy);
			return -1;
		}
		memcpy(host, copy, delim - copy);
		host[delim - copy] = '\0';
		port = atoi(delim + 1);
		free(copy);

		if (!(hptr = gethostbyname(host))) {
			return -1;
		}

		if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
			return -1;
		}
		memset(&sa, 0, sizeof(sa));
		sa.sin_family = AF_INET;
		sa.sin_port = htons(port);
		{
			/* see yar_copy_unaligned(): the hostent members are misaligned on
			 * some platforms, copy them out byte-wise instead of loading them */
			char **addr_list;
			char *addr;
			yar_copy_unaligned(&addr_list, &hptr->h_addr_list, sizeof(addr_list));
			yar_copy_unaligned(&addr, addr_list, sizeof(addr));
			yar_copy_unaligned(&sa.sin_addr, addr, sizeof(struct in_addr));
		}
		if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
			close(fd);
			return -1;
		}
	} else {
		struct sockaddr_un sa;

		if (strlen(test_uri) >= sizeof(sa.sun_path)) {
			return -1;
		}
		if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
			return -1;
		}
		memset(&sa, 0, sizeof(sa));
		sa.sun_family = AF_UNIX;
		memcpy(sa.sun_path, test_uri, strlen(test_uri) + 1);
		if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
			close(fd);
			return -1;
		}
	}

	return fd;
}
/* }}} */

/* connectivity {{{ */
static void test_connect(void) {
	yar_client *client = new_client();
	YAR_ASSERT(client != NULL, "connect to %s failed", test_uri);
	yar_client_destroy(client);
}

static void test_connect_refused(void) {
	yar_client *client;

	if (!test_is_tcp) {
		printf("(skipped for unix sockets) ");
		return;
	}

	client = yar_client_init("tcp://127.0.0.1:1");
	YAR_ASSERT(client == NULL, "connect to a closed port should fail");
}
/* }}} */

/* echo {{{ */
static void test_echo_no_args(void) {
	yar_client *client = new_client();
	yar_response *response;
	unsigned int size = 0;
	const yar_data *data;

	YAR_ASSERT(client != NULL, "connect failed");
	response = client->call(client, "echo", 0, NULL);
	YAR_ASSERT(response != NULL, "no response");
	YAR_ASSERT(yar_response_get_status(response) == 0, "unexpected status %d", yar_response_get_status(response));

	data = yar_response_get_response(response);
	YAR_ASSERT(data != NULL, "empty response data");
	/* the C client packs zero arguments as NULL */
	YAR_ASSERT(yar_unpack_data_type(data, &size) == YAR_DATA_NULL, "expected NULL for a no-arg echo");

	free_response(response);
	yar_client_destroy(client);
}

static void test_echo_scalars(void) {
	yar_client *client = new_client();
	yar_response *response;
	yar_packager *args[5];
	const yar_data *data, *elem;
	unsigned int size = 0, dummy = 0;
	long lval;
	double dval;
	int bval;
	const char *str;

	args[0] = yar_pack_start_long();
	yar_pack_push_long(args[0], 42);
	args[1] = yar_pack_start_double();
	yar_pack_push_double(args[1], 3.14);
	args[2] = yar_pack_start_bool();
	yar_pack_push_bool(args[2], 1);
	args[3] = yar_pack_start_string();
	yar_pack_push_string(args[3], "hello", sizeof("hello") - 1);
	/* a bare yar_pack_start_null() serializes to zero bytes (the packing layer
	 * uses it as a wrapper), so a null can not travel as a bare parameter;
	 * wrap it in an array to cover the null roundtrip */
	args[4] = yar_pack_start_array(1);
	yar_pack_push_null(args[4]);

	YAR_ASSERT(client != NULL, "connect failed");
	response = client->call(client, "echo", 5, args);
	{
		int i;
		for (i = 0; i < 5; i++) {
			yar_pack_free(args[i]);
		}
	}
	YAR_ASSERT(response != NULL, "no response");
	YAR_ASSERT(yar_response_get_status(response) == 0, "unexpected status %d", yar_response_get_status(response));

	data = yar_response_get_response(response);
	YAR_ASSERT(yar_unpack_data_type(data, &size) == YAR_DATA_ARRAY && size == 5,
			"expected an array of 5, got type %d size %u", yar_unpack_data_type(data, &size), size);

	elem = array_at(data, 0);
	YAR_ASSERT(elem && data_as_long(elem, &lval) != 0 && lval == 42,
			"long roundtrip failed");
	elem = array_at(data, 1);
	YAR_ASSERT(elem && yar_unpack_data_type(elem, &dummy) == YAR_DATA_DOUBLE && yar_unpack_data_value(elem, &dval) == YAR_DATA_DOUBLE && dval == 3.14,
			"double roundtrip failed");
	elem = array_at(data, 2);
	YAR_ASSERT(elem && yar_unpack_data_type(elem, &dummy) == YAR_DATA_BOOL && yar_unpack_data_bool(elem, &bval) == YAR_DATA_BOOL && bval == 1,
			"bool roundtrip failed");
	elem = array_at(data, 3);
	YAR_ASSERT(elem && yar_unpack_data_type(elem, &size) == YAR_DATA_STRING && size == 5
			&& yar_unpack_data_string(elem, &str) == YAR_DATA_STRING && strncmp(str, "hello", 5) == 0,
			"string roundtrip failed");
	elem = array_at(data, 4);
	YAR_ASSERT(elem && yar_unpack_data_type(elem, &size) == YAR_DATA_ARRAY && size == 1,
			"[null] wrapper lost");
	elem = array_at(elem, 0);
	YAR_ASSERT(elem && yar_unpack_data_type(elem, &dummy) == YAR_DATA_NULL, "null roundtrip failed");

	free_response(response);
	yar_client_destroy(client);
}

static void test_echo_composite(void) {
	yar_client *client = new_client();
	yar_response *response;
	yar_packager *arg;
	const yar_data *data, *elem, *list, *meta;
	unsigned int size = 0, dummy = 0;
	long lval;
	const char *str;

	/* {"list": [1, 2, 3], "meta": {"k": "v"}, "empty": []} */
	arg = yar_pack_start_map(3);
	yar_pack_push_string(arg, "list", 4);
	yar_pack_push_array(arg, 3);
	yar_pack_push_long(arg, 1);
	yar_pack_push_long(arg, 2);
	yar_pack_push_long(arg, 3);
	yar_pack_push_string(arg, "meta", 4);
	yar_pack_push_map(arg, 1);
	yar_pack_push_string(arg, "k", 1);
	yar_pack_push_string(arg, "v", 1);
	yar_pack_push_string(arg, "empty", 5);
	yar_pack_push_array(arg, 0);

	YAR_ASSERT(client != NULL, "connect failed");
	response = client->call(client, "echo", 1, &arg);
	yar_pack_free(arg);
	YAR_ASSERT(response != NULL, "no response");
	YAR_ASSERT(yar_response_get_status(response) == 0, "unexpected status %d", yar_response_get_status(response));

	data = yar_response_get_response(response);
	YAR_ASSERT(yar_unpack_data_type(data, &size) == YAR_DATA_ARRAY && size == 1, "expected an array of 1");
	elem = array_at(data, 0);
	YAR_ASSERT(elem && yar_unpack_data_type(elem, &size) == YAR_DATA_MAP && size == 3, "expected a map of 3");

	list = map_get(elem, "list", 4);
	YAR_ASSERT(list && yar_unpack_data_type(list, &size) == YAR_DATA_ARRAY && size == 3, "list missing or wrong size");
	YAR_ASSERT(array_at(list, 1) && data_as_long(array_at(list, 1), &lval) != 0 && lval == 2,
			"list[1] != 2");

	meta = map_get(elem, "meta", 4);
	YAR_ASSERT(meta && yar_unpack_data_type(meta, &dummy) == YAR_DATA_MAP, "meta missing");
	elem = map_get(meta, "k", 1);
	YAR_ASSERT(elem && yar_unpack_data_type(elem, &size) == YAR_DATA_STRING && size == 1
			&& yar_unpack_data_string(elem, &str) == YAR_DATA_STRING && str[0] == 'v',
			"meta.k != \"v\"");

	elem = map_get(array_at(data, 0), "nope", 4);
	YAR_ASSERT(elem == NULL, "map_get should return NULL for a missing key");

	elem = map_get(array_at(data, 0), "empty", 5);
	YAR_ASSERT(elem && yar_unpack_data_type(elem, &size) == YAR_DATA_ARRAY && size == 0, "empty array lost");

	free_response(response);
	yar_client_destroy(client);
}
/* }}} */

/* type matrix {{{ */
static void test_types(void) {
	yar_client *client = new_client();
	yar_response *response;

	YAR_ASSERT(client != NULL, "connect failed");
	response = client->call(client, "types", 0, NULL);

	if (test_packager == YAR_PACKAGER_JSON) {
		yar_client *probe;
		yar_response *probe_response;

		/* the type matrix contains a binary string with embedded NUL bytes,
		 * that can not be represented in JSON; the server must drop this one
		 * call but keep serving other clients */
		YAR_ASSERT(response == NULL, "types() carries binary data and must fail over JSON");

		probe = new_client();
		YAR_ASSERT(probe != NULL, "server stopped accepting after a JSON encoding failure");
		probe_response = probe->call(probe, "echo", 0, NULL);
		YAR_ASSERT(probe_response != NULL && yar_response_get_status(probe_response) == 0,
				"server no longer functional after a JSON encoding failure");
		free_response(probe_response);
		yar_client_destroy(probe);
		return;
	}

	{
		const yar_data *data, *elem, *arr;
		unsigned int size = 0, dummy = 0;
		long lval;
		double dval;
		int bval;
		const char *str;

		YAR_ASSERT(response != NULL, "no response");
		YAR_ASSERT(yar_response_get_status(response) == 0, "unexpected status %d", yar_response_get_status(response));

		data = yar_response_get_response(response);
		YAR_ASSERT(data && yar_unpack_data_type(data, &size) == YAR_DATA_MAP && size == 12,
				"expected a map of 12, got type %d size %u",
				data ? yar_unpack_data_type(data, &size) : 0, size);

		elem = map_get(data, "null", 4);
		YAR_ASSERT(elem && yar_unpack_data_type(elem, &dummy) == YAR_DATA_NULL, "null value");
		elem = map_get(data, "true", 4);
		YAR_ASSERT(elem && yar_unpack_data_type(elem, &dummy) == YAR_DATA_BOOL
				&& yar_unpack_data_bool(elem, &bval) == YAR_DATA_BOOL && bval == 1, "true value");
		elem = map_get(data, "false", 5);
		YAR_ASSERT(elem && yar_unpack_data_type(elem, &dummy) == YAR_DATA_BOOL
				&& yar_unpack_data_bool(elem, &bval) == YAR_DATA_BOOL && bval == 0, "false value");
		elem = map_get(data, "long", 4);
		YAR_ASSERT(elem && data_as_long(elem, &lval) != 0 && lval == -12345, "long value");
		elem = map_get(data, "ulong", 5);
		YAR_ASSERT(elem && data_as_long(elem, &lval) != 0 && lval == 4294967296L, "ulong value");
		elem = map_get(data, "double", 6);
		YAR_ASSERT(elem && yar_unpack_data_type(elem, &dummy) == YAR_DATA_DOUBLE
				&& yar_unpack_data_value(elem, &dval) == YAR_DATA_DOUBLE && dval == 3.14159, "double value");
		elem = map_get(data, "string", 6);
		YAR_ASSERT(elem && yar_unpack_data_type(elem, &size) == YAR_DATA_STRING && size == 9
				&& yar_unpack_data_string(elem, &str) == YAR_DATA_STRING && strncmp(str, "hello yar", 9) == 0,
				"string value");
		elem = map_get(data, "binary", 6);
		YAR_ASSERT(elem && yar_unpack_data_type(elem, &size) == YAR_DATA_STRING && size == 4
				&& yar_unpack_data_string(elem, &str) == YAR_DATA_STRING
				&& memcmp(str, "\x00\x01\x02\xff", 4) == 0,
				"binary value");

		elem = map_get(data, "array", 5);
		YAR_ASSERT(elem && yar_unpack_data_type(elem, &size) == YAR_DATA_ARRAY && size == 3, "array value");
		YAR_ASSERT(array_at(elem, 0) && data_as_long(array_at(elem, 0), &lval) != 0 && lval == 1, "array[0]");
		arr = array_at(elem, 1);
		YAR_ASSERT(arr && yar_unpack_data_type(arr, &size) == YAR_DATA_STRING && size == 3
				&& yar_unpack_data_string(arr, &str) == YAR_DATA_STRING && strncmp(str, "two", 3) == 0,
				"array[1]");
		arr = array_at(elem, 2);
		YAR_ASSERT(arr && yar_unpack_data_type(arr, &dummy) == YAR_DATA_DOUBLE
				&& yar_unpack_data_value(arr, &dval) == YAR_DATA_DOUBLE && dval == 3.0, "array[2]");

		elem = map_get(data, "map", 3);
		YAR_ASSERT(elem && yar_unpack_data_type(elem, &size) == YAR_DATA_MAP && size == 1, "map value");
		elem = map_get(elem, "nested", 6);
		YAR_ASSERT(elem && yar_unpack_data_type(elem, &size) == YAR_DATA_MAP && size == 1, "map.nested");
		elem = map_get(elem, "deep", 4);
		YAR_ASSERT(elem && yar_unpack_data_type(elem, &size) == YAR_DATA_ARRAY && size == 2, "map.nested.deep");
		YAR_ASSERT(array_at(elem, 0) && yar_unpack_data_type(array_at(elem, 0), &dummy) == YAR_DATA_BOOL
				&& yar_unpack_data_bool(array_at(elem, 0), &bval) == YAR_DATA_BOOL && bval == 1, "deep[0]");
		YAR_ASSERT(array_at(elem, 1) && yar_unpack_data_type(array_at(elem, 1), &dummy) == YAR_DATA_NULL, "deep[1]");

		elem = map_get(data, "empty_array", 11);
		YAR_ASSERT(elem && yar_unpack_data_type(elem, &size) == YAR_DATA_ARRAY && size == 0, "empty_array");
		elem = map_get(data, "empty_map", 9);
		YAR_ASSERT(elem && yar_unpack_data_type(elem, &size) == YAR_DATA_MAP && size == 0, "empty_map");

		free_response(response);
		yar_client_destroy(client);
	}
}
/* }}} */

/* JSON number/string fidelity: only meaningful with --packager json {{{ */
static void test_json_fidelity(void) {
	yar_client *client;
	yar_response *response;
	yar_packager *arg;
	const yar_data *data, *elem;
	unsigned int size = 0, dummy = 0;
	long lval;
	double dval;
	const char *str;
	const int *configured;

	if (test_packager != YAR_PACKAGER_JSON) {
		printf("(skipped for msgpack) ");
		return;
	}

	/* covers the JSON<->msgpack converter edges: the biggest integer a double
	 * can represent exactly (16 significant digits, cJSON would mangle it with
	 * %1.15g), fractional and huge doubles, escapes and UTF-8, empty containers */
	arg = yar_pack_start_map(6);
	yar_pack_push_string(arg, "big", 3);
	yar_pack_push_long(arg, 9007199254740992L); /* 2^53 */
	yar_pack_push_string(arg, "neg", 3);
	yar_pack_push_long(arg, -9007199254740992L);
	yar_pack_push_string(arg, "frac", 4);
	yar_pack_push_double(arg, 0.1);
	yar_pack_push_string(arg, "huge", 4);
	yar_pack_push_double(arg, 1e300);
	yar_pack_push_string(arg, "text", 4);
	yar_pack_push_string(arg, "line1\nline2\t\"q\" 你好", sizeof("line1\nline2\t\"q\" 你好") - 1);
	yar_pack_push_string(arg, "empty_map", 9);
	yar_pack_push_map(arg, 0);

	client = new_client();
	YAR_ASSERT(client != NULL, "connect failed");

	configured = (const int *)yar_client_get_opt(client, YAR_OPT_PACKAGER);
	YAR_ASSERT(configured != NULL && *configured == YAR_PACKAGER_JSON,
			"get_opt(YAR_OPT_PACKAGER) should report json");

	response = client->call(client, "echo", 1, &arg);
	yar_pack_free(arg);
	YAR_ASSERT(response != NULL, "no response");
	YAR_ASSERT(yar_response_get_status(response) == 0, "unexpected status %d", yar_response_get_status(response));

	data = yar_response_get_response(response);
	YAR_ASSERT(data && yar_unpack_data_type(data, &size) == YAR_DATA_ARRAY && size == 1, "expected an array of 1");
	elem = array_at(data, 0);
	YAR_ASSERT(elem && yar_unpack_data_type(elem, &size) == YAR_DATA_MAP && size == 6, "expected a map of 6");

	YAR_ASSERT(map_get(elem, "big", 3) && data_as_long(map_get(elem, "big", 3), &lval) != 0
			&& lval == 9007199254740992L, "2^53 did not roundtrip exactly");
	YAR_ASSERT(map_get(elem, "neg", 3) && data_as_long(map_get(elem, "neg", 3), &lval) != 0
			&& lval == -9007199254740992L, "-2^53 did not roundtrip exactly");
	YAR_ASSERT(map_get(elem, "frac", 4) && yar_unpack_data_type(map_get(elem, "frac", 4), &dummy) == YAR_DATA_DOUBLE
			&& yar_unpack_data_value(map_get(elem, "frac", 4), &dval) == YAR_DATA_DOUBLE && dval == 0.1,
			"0.1 did not roundtrip exactly");
	YAR_ASSERT(map_get(elem, "huge", 4) && yar_unpack_data_type(map_get(elem, "huge", 4), &dummy) == YAR_DATA_DOUBLE
			&& yar_unpack_data_value(map_get(elem, "huge", 4), &dval) == YAR_DATA_DOUBLE && dval == 1e300,
			"1e300 did not roundtrip exactly");

	{
		const char expected[] = "line1\nline2\t\"q\" 你好";
		const yar_data *text = map_get(elem, "text", 4);
		YAR_ASSERT(text && yar_unpack_data_type(text, &size) == YAR_DATA_STRING
				&& size == sizeof(expected) - 1
				&& yar_unpack_data_string(text, &str) == YAR_DATA_STRING
				&& memcmp(str, expected, size) == 0,
				"escaped/UTF-8 string did not roundtrip");
	}

	YAR_ASSERT(map_get(elem, "empty_map", 9)
			&& yar_unpack_data_type(map_get(elem, "empty_map", 9), &size) == YAR_DATA_MAP && size == 0,
			"empty map did not roundtrip");

	free_response(response);
	yar_client_destroy(client);
}
/* }}} */

/* add {{{ */
static void test_add_long(void) {
	yar_client *client = new_client();
	yar_response *response;
	yar_packager *args[2];
	const yar_data *data;
	long result = 0;

	args[0] = yar_pack_start_long();
	yar_pack_push_long(args[0], 2);
	args[1] = yar_pack_start_long();
	yar_pack_push_long(args[1], 3);

	YAR_ASSERT(client != NULL, "connect failed");
	response = client->call(client, "add", 2, args);
	yar_pack_free(args[0]);
	yar_pack_free(args[1]);
	YAR_ASSERT(response != NULL, "no response");
	YAR_ASSERT(yar_response_get_status(response) == 0, "unexpected status %d", yar_response_get_status(response));

	data = yar_response_get_response(response);
	YAR_ASSERT(data && data_as_long(data, &result) != 0 && result == 5,
			"2 + 3 != 5");

	free_response(response);
	yar_client_destroy(client);
}

static void test_add_double(void) {
	yar_client *client = new_client();
	yar_response *response;
	yar_packager *args[2];
	const yar_data *data;
	unsigned int dummy = 0;
	double result = 0;

	args[0] = yar_pack_start_double();
	yar_pack_push_double(args[0], 1.5);
	args[1] = yar_pack_start_double();
	yar_pack_push_double(args[1], 2.25);

	YAR_ASSERT(client != NULL, "connect failed");
	response = client->call(client, "add", 2, args);
	yar_pack_free(args[0]);
	yar_pack_free(args[1]);
	YAR_ASSERT(response != NULL, "no response");
	YAR_ASSERT(yar_response_get_status(response) == 0, "unexpected status %d", yar_response_get_status(response));

	data = yar_response_get_response(response);
	YAR_ASSERT(data && yar_unpack_data_type(data, &dummy) == YAR_DATA_DOUBLE
			&& yar_unpack_data_value(data, &result) == YAR_DATA_DOUBLE && result == 3.75,
			"1.5 + 2.25 != 3.75");

	free_response(response);
	yar_client_destroy(client);
}
/* }}} */

/* error paths {{{ */
static void test_undefined_method(void) {
	yar_client *client = new_client();
	yar_response *response;
	const char *msg = NULL;
	unsigned int len = 0;

	YAR_ASSERT(client != NULL, "connect failed");
	response = client->call(client, "no_such_method", 0, NULL);
	YAR_ASSERT(response != NULL, "no response");
	YAR_ASSERT(yar_response_get_status(response) != 0, "expected a non-zero status");
	YAR_ASSERT(yar_response_get_error(response, &msg, &len) == 1 && len > 0, "expected an error message");
	YAR_ASSERT(len >= sizeof("call to undefined method") - 1
			&& strstr(msg, "undefined method") != NULL,
			"unexpected error message: %.*s", (int)len, msg);

	free_response(response);
	yar_client_destroy(client);
}

static void test_server_error(void) {
	yar_client *client = new_client();
	yar_response *response;
	const char *msg = NULL;
	unsigned int len = 0;

	YAR_ASSERT(client != NULL, "connect failed");
	response = client->call(client, "error", 0, NULL);
	YAR_ASSERT(response != NULL, "no response");
	YAR_ASSERT(yar_response_get_status(response) == TEST_ERR_EXCEPTION,
			"expected status %d, got %d", TEST_ERR_EXCEPTION, yar_response_get_status(response));
	YAR_ASSERT(yar_response_get_error(response, &msg, &len) == 1, "expected an error message");
	YAR_ASSERT(len == sizeof("intentional error from test server") - 1
			&& strncmp(msg, "intentional error from test server", len) == 0,
			"error message mismatch: %.*s", (int)len, msg);

	free_response(response);
	yar_client_destroy(client);
}
/* }}} */

/* persistence {{{ */
static void test_persistent(void) {
	yar_client *client;
	int persistent = 1;
	int i;

	client = new_client();
	YAR_ASSERT(client != NULL, "connect failed");
	YAR_ASSERT(yar_client_set_opt(client, YAR_PERSISTENT_LINK, &persistent) == 1, "set_opt failed");

	for (i = 0; i < 10; i++) {
		yar_response *response = client->call(client, "echo", 0, NULL);
		YAR_ASSERT(response != NULL, "call #%d got no response on a persistent link", i);
		YAR_ASSERT(yar_response_get_status(response) == 0, "call #%d status %d", i, yar_response_get_status(response));
		free_response(response);
	}

	yar_client_destroy(client);
}

static void test_non_persistent_single_call(void) {
	yar_client *client = new_client(); /* persistent = 0 by default */
	yar_response *response;

	YAR_ASSERT(client != NULL, "connect failed");
	response = client->call(client, "echo", 0, NULL);
	YAR_ASSERT(response != NULL, "no response");
	YAR_ASSERT(yar_response_get_status(response) == 0, "unexpected status %d", yar_response_get_status(response));

	free_response(response);
	yar_client_destroy(client);
}
/* }}} */

/* large payload {{{ */
static void check_big_response(yar_client *client, long len) {
	yar_response *response;
	yar_packager *arg;
	const yar_data *data;
	unsigned int size = 0;
	const char *str = NULL;
	long i;

	arg = yar_pack_start_long();
	yar_pack_push_long(arg, len);

	response = client->call(client, "big", 1, &arg);
	yar_pack_free(arg);
	YAR_ASSERT(response != NULL, "no response for %ld bytes", len);
	YAR_ASSERT(yar_response_get_status(response) == 0, "unexpected status %d for %ld bytes",
			yar_response_get_status(response), len);

	data = yar_response_get_response(response);
	YAR_ASSERT(data && yar_unpack_data_type(data, &size) == YAR_DATA_STRING && size == (unsigned int)len,
			"expected a %ld bytes string, got type %d size %u", len,
			data ? yar_unpack_data_type(data, &size) : 0, size);

	YAR_ASSERT(yar_unpack_data_string(data, &str) == YAR_DATA_STRING, "string extract failed");
	for (i = 0; i < len; i++) {
		if (str[i] != 'a' + (i % 26)) {
			YAR_ASSERT(0, "payload corrupted at offset %ld", i);
		}
	}

	free_response(response);
}

static void test_big_payload(void) {
	yar_client *client;

	/* one call per connection: the server closes a non-persistent link
	 * after the response */
	client = new_client();
	YAR_ASSERT(client != NULL, "connect failed");
	check_big_response(client, 1024 * 1024);        /* 1M */
	yar_client_destroy(client);

	client = new_client();
	YAR_ASSERT(client != NULL, "connect failed");
	check_big_response(client, 8 * 1024 * 1024);     /* 8M, exercises multi-segment reads */
	yar_client_destroy(client);
}
/* }}} */

/* timeout & recovery {{{ */
static void test_timeout(void) {
	yar_client *client = new_client_timeout(1);
	yar_response *response;
	yar_packager *arg = yar_pack_start_long();

	yar_pack_push_long(arg, 3);

	YAR_ASSERT(client != NULL, "connect failed");
	/* the server handler sleeps 3 seconds, the client gives up after 1 */
	response = client->call(client, "sleep", 1, &arg);
	yar_pack_free(arg);

	if (response) {
		free_response(response);
		YAR_ASSERT(0, "expected a timeout, got a response");
	}

	yar_client_destroy(client);
}

static void test_recovery_after_timeout(void) {
	yar_client *client = new_client();
	yar_response *response;

	YAR_ASSERT(client != NULL, "connect failed");
	response = client->call(client, "echo", 0, NULL);
	YAR_ASSERT(response != NULL, "server did not recover after a client timeout");
	YAR_ASSERT(yar_response_get_status(response) == 0, "unexpected status %d", yar_response_get_status(response));

	free_response(response);
	yar_client_destroy(client);
}
/* }}} */

/* protocol abuse: the server must survive malformed input {{{ */
static void test_malformed_garbage_header(void) {
	int fd;
	char garbage[82];
	yar_client *client;
	yar_response *response;

	fd = raw_connect();
	YAR_ASSERT(fd != -1, "raw connect failed");

	memset(garbage, 0xAB, sizeof(garbage));
	YAR_ASSERT(send(fd, garbage, sizeof(garbage), 0) == (ssize_t)sizeof(garbage), "send failed");
	close(fd);

	/* give the server a moment to process the garbage */
	usleep(100 * 1000);

	client = new_client();
	YAR_ASSERT(client != NULL, "server stopped accepting after garbage input");
	response = client->call(client, "echo", 0, NULL);
	YAR_ASSERT(response != NULL && yar_response_get_status(response) == 0,
			"server no longer functional after garbage input");
	free_response(response);
	yar_client_destroy(client);
}

static void test_malformed_huge_body_len(void) {
	int fd;
	yar_header header = {0};
	yar_client *client;
	yar_response *response;

	fd = raw_connect();
	YAR_ASSERT(fd != -1, "raw connect failed");

	header.magic_num = htonl(YAR_PROTOCOL_MAGIC_NUM);
	header.id = htonl(1);
	header.body_len = htonl(0xFFFFFFFF); /* 4G, must be rejected, not malloc()ed */
	memcpy(header.provider, YAR_CLIENT_NAME, sizeof(YAR_CLIENT_NAME) > 16 ? 16 : sizeof(YAR_CLIENT_NAME));
	YAR_ASSERT(send(fd, &header, sizeof(header), 0) == (ssize_t)sizeof(header), "send failed");
	close(fd);

	usleep(100 * 1000);

	client = new_client();
	YAR_ASSERT(client != NULL, "server stopped accepting after oversized body_len");
	response = client->call(client, "echo", 0, NULL);
	YAR_ASSERT(response != NULL && yar_response_get_status(response) == 0,
			"server no longer functional after oversized body_len");
	free_response(response);
	yar_client_destroy(client);
}
/* }}} */

/* concurrency {{{ */
static void test_concurrent(void) {
	pid_t children[4];
	int i, num_children = 4, calls = 25;

	for (i = 0; i < num_children; i++) {
		pid_t pid = fork();
		YAR_ASSERT(pid != -1, "fork failed");
		if (pid == 0) {
			int j;
			for (j = 0; j < calls; j++) {
				yar_client *client = new_client();
				yar_response *response;
				if (!client) {
					_exit(1);
				}
				response = client->call(client, "echo", 0, NULL);
				if (!response || yar_response_get_status(response) != 0) {
					_exit(1);
				}
				free_response(response);
				yar_client_destroy(client);
			}
			_exit(0);
		}
		children[i] = pid;
	}

	for (i = 0; i < num_children; i++) {
		int stat = 0;
		YAR_ASSERT(waitpid(children[i], &stat, 0) == children[i], "waitpid failed");
		YAR_ASSERT(WIFEXITED(stat) && WEXITSTATUS(stat) == 0,
				"child %d failed (status %d)", i, stat);
	}
}
/* }}} */

static int probe_server(void) {
	int attempts = 50; /* 50 x 100ms = 5s */

	while (attempts-- > 0) {
		yar_client *client = yar_client_init(test_uri);
		if (client) {
			yar_client_destroy(client);
			return 0;
		}
		usleep(100 * 1000);
	}
	return 1;
}

int main(int argc, char **argv) {
	int i;
	int probe = 0, concurrent_only = 0;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--uri") == 0 && i + 1 < argc) {
			test_uri = argv[++i];
		} else if (strcmp(argv[i], "--probe") == 0) {
			probe = 1;
		} else if (strcmp(argv[i], "--concurrent") == 0) {
			concurrent_only = 1;
		} else if (strcmp(argv[i], "--packager") == 0 && i + 1 < argc) {
			if (strcmp(argv[++i], "json") == 0) {
				test_packager = YAR_PACKAGER_JSON;
			} else if (strcmp(argv[i], "msgpack") != 0) {
				fprintf(stderr, "error: unknown packager '%s', expected msgpack or json\n", argv[i]);
				return 2;
			}
		} else {
			fprintf(stderr, "usage: %s --uri <tcp://host:port | /path/sock> [--probe] [--concurrent] [--packager <msgpack|json>]\n", argv[0]);
			return 2;
		}
	}

	if (!test_uri) {
		fprintf(stderr, "usage: %s --uri <tcp://host:port | /path/sock> [--probe] [--concurrent] [--packager <msgpack|json>]\n", argv[0]);
		return 2;
	}

	test_is_tcp = (strncmp(test_uri, "tcp://", sizeof("tcp://") - 1) == 0);

	if (probe) {
		return probe_server();
	}

	printf("yar-c test suite, uri = %s, packager = %s\n", test_uri,
			test_packager == YAR_PACKAGER_JSON? "json" : "msgpack");

	if (concurrent_only) {
		YAR_RUN(test_concurrent);
		YAR_SUMMARY();
		return yar_tests_failed? 1 : 0;
	}

	YAR_RUN(test_connect);
	YAR_RUN(test_connect_refused);
	YAR_RUN(test_echo_no_args);
	YAR_RUN(test_echo_scalars);
	YAR_RUN(test_echo_composite);
	YAR_RUN(test_types);
	YAR_RUN(test_json_fidelity);
	YAR_RUN(test_add_long);
	YAR_RUN(test_add_double);
	YAR_RUN(test_undefined_method);
	YAR_RUN(test_server_error);
	YAR_RUN(test_persistent);
	YAR_RUN(test_non_persistent_single_call);
	YAR_RUN(test_big_payload);
	YAR_RUN(test_concurrent);
	YAR_RUN(test_malformed_garbage_header);
	YAR_RUN(test_malformed_huge_body_len);
	/* keep the timeout tests last: they occupy the (single-process) server
	   for ~3 seconds */
	YAR_RUN(test_timeout);
	YAR_RUN(test_recovery_after_timeout);

	YAR_SUMMARY();
	return yar_tests_failed? 1 : 0;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
