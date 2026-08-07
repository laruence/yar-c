/**
 * Yar C test suite - fixture server
 *
 * Provides a fixed set of RPC methods used by the test clients
 * (tests/test_client.c and the PHP files under tests/php):
 *
 *   echo(params...)  -> returns the parameter array unchanged
 *   add(a, b)        -> numeric sum (long if both are integers, double otherwise)
 *   types()          -> fixed map covering all supported data types
 *   sleep(seconds)   -> blocks for the given seconds, then returns them
 *   error()          -> always fails with YAR_ERR_EXCEPTION and a fixed message
 *   big(len)         -> returns a string of len bytes ('a' + i % 26 pattern)
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

#include "yar.h"

#define TEST_ERR_EXCEPTION 0x40 /* keep in sync with YAR_ERR_EXCEPTION in php-yar */

/* fetch parameter #idx (0-based) from the request, NULL if absent */
static const yar_data * test_get_parameter(yar_request *request, unsigned int idx) /* {{{ */ {
	const yar_data *parameters = yar_request_get_parameters(request);
	const yar_data *current = NULL;
	unsigned int size, i = 0;

	if (!parameters || yar_unpack_data_type(parameters, &size) != YAR_DATA_ARRAY || size <= idx) {
		return NULL;
	}

	{
		yar_unpack_iterator *it = yar_unpack_iterator_init(parameters);
		do {
			current = yar_unpack_iterator_current(it);
			if (i == idx) {
				yar_unpack_iterator_free(it);
				return current;
			}
			i++;
		} while (yar_unpack_iterator_next(it));
		yar_unpack_iterator_free(it);
	}

	return NULL;
}
/* }}} */

static void test_handler_echo(yar_request *request, yar_response *response, void *data) /* {{{ */ {
	const yar_data *parameters = yar_request_get_parameters(request);
	yar_packager *pk = yar_pack_start_null();

	if (parameters) {
		yar_pack_push_data(pk, parameters);
	} else {
		yar_pack_push_null(pk);
	}

	yar_response_set_retval(response, pk);
	yar_pack_free(pk);
}
/* }}} */

static void test_handler_add(yar_request *request, yar_response *response, void *data) /* {{{ */ {
	const yar_data *parameters = yar_request_get_parameters(request);
	const yar_data *a, *b;
	unsigned int size;
	yar_data_type ta, tb;
	yar_packager *pk;

	if (!parameters || yar_unpack_data_type(parameters, &size) != YAR_DATA_ARRAY || size != 2) {
		yar_response_set_error(response, TEST_ERR_EXCEPTION, "add expects exactly 2 parameters");
		return;
	}

	a = test_get_parameter(request, 0);
	b = test_get_parameter(request, 1);

	{
		unsigned int dummy = 0;
		ta = yar_unpack_data_type(a, &dummy);
		tb = yar_unpack_data_type(b, &dummy);
	}

	pk = yar_pack_start_null();
	if ((ta == YAR_DATA_LONG || ta == YAR_DATA_ULONG) && (tb == YAR_DATA_LONG || tb == YAR_DATA_ULONG)) {
		long x = 0, y = 0;
		yar_unpack_data_long(a, &x);
		yar_unpack_data_long(b, &y);
		yar_pack_push_long(pk, x + y);
	} else if (ta == YAR_DATA_DOUBLE || tb == YAR_DATA_DOUBLE) {
		double x = 0, y = 0;
		const yar_data *args[2] = {a, b};
		double *out[2] = {&x, &y};
		int i;
		for (i = 0; i < 2; i++) {
			unsigned int dummy = 0;
			switch (yar_unpack_data_type(args[i], &dummy)) {
				case YAR_DATA_DOUBLE:
					{
						double v;
						yar_unpack_data_value(args[i], &v);
						*out[i] = v;
					}
					break;
				case YAR_DATA_LONG:
				case YAR_DATA_ULONG:
					{
						long v = 0;
						yar_unpack_data_long(args[i], &v);
						*out[i] = (double)v;
					}
					break;
				default:
					yar_pack_free(pk);
					yar_response_set_error(response, TEST_ERR_EXCEPTION, "add expects numeric parameters");
					return;
			}
		}
		yar_pack_push_double(pk, x + y);
	} else {
		yar_pack_free(pk);
		yar_response_set_error(response, TEST_ERR_EXCEPTION, "add expects numeric parameters");
		return;
	}

	yar_response_set_retval(response, pk);
	yar_pack_free(pk);
}
/* }}} */

static void test_handler_types(yar_request *request, yar_response *response, void *data) /* {{{ */ {
	static const char binary[] = {'\x00', '\x01', '\x02', '\xff'};
	yar_packager *pk = yar_pack_start_map(12);

	yar_pack_push_string(pk, "null", sizeof("null") - 1);
	yar_pack_push_null(pk);

	yar_pack_push_string(pk, "true", sizeof("true") - 1);
	yar_pack_push_bool(pk, 1);

	yar_pack_push_string(pk, "false", sizeof("false") - 1);
	yar_pack_push_bool(pk, 0);

	yar_pack_push_string(pk, "long", sizeof("long") - 1);
	yar_pack_push_long(pk, -12345);

	yar_pack_push_string(pk, "ulong", sizeof("ulong") - 1);
	yar_pack_push_ulong(pk, 4294967296UL); /* 2^32, still fits a PHP integer */

	yar_pack_push_string(pk, "double", sizeof("double") - 1);
	yar_pack_push_double(pk, 3.14159);

	yar_pack_push_string(pk, "string", sizeof("string") - 1);
	yar_pack_push_string(pk, "hello yar", sizeof("hello yar") - 1);

	yar_pack_push_string(pk, "binary", sizeof("binary") - 1);
	yar_pack_push_string(pk, (char *)binary, sizeof(binary));

	yar_pack_push_string(pk, "array", sizeof("array") - 1);
	yar_pack_push_array(pk, 3);
	yar_pack_push_long(pk, 1);
	yar_pack_push_string(pk, "two", sizeof("two") - 1);
	yar_pack_push_double(pk, 3.0);

	yar_pack_push_string(pk, "map", sizeof("map") - 1);
	yar_pack_push_map(pk, 1);
	yar_pack_push_string(pk, "nested", sizeof("nested") - 1);
	yar_pack_push_map(pk, 1);
	yar_pack_push_string(pk, "deep", sizeof("deep") - 1);
	yar_pack_push_array(pk, 2);
	yar_pack_push_bool(pk, 1);
	yar_pack_push_null(pk);

	yar_pack_push_string(pk, "empty_array", sizeof("empty_array") - 1);
	yar_pack_push_array(pk, 0);

	yar_pack_push_string(pk, "empty_map", sizeof("empty_map") - 1);
	yar_pack_push_map(pk, 0);

	yar_response_set_retval(response, pk);
	yar_pack_free(pk);
}
/* }}} */

static void test_handler_sleep(yar_request *request, yar_response *response, void *data) /* {{{ */ {
	const yar_data *param = test_get_parameter(request, 0);
	unsigned int dummy = 0;
	long seconds = 0;

	if (param && yar_unpack_data_type(param, &dummy) == YAR_DATA_LONG) {
		yar_unpack_data_long(param, &seconds);
	} else if (param && yar_unpack_data_type(param, &dummy) == YAR_DATA_ULONG) {
		yar_unpack_data_long(param, &seconds);
	}

	if (seconds < 0) {
		seconds = 0;
	}
	if (seconds > 0) {
		sleep((unsigned int)seconds);
	}

	{
		yar_packager *pk = yar_pack_start_null();
		yar_pack_push_long(pk, seconds);
		yar_response_set_retval(response, pk);
		yar_pack_free(pk);
	}
}
/* }}} */

static void test_handler_error(yar_request *request, yar_response *response, void *data) /* {{{ */ {
	yar_response_set_error(response, TEST_ERR_EXCEPTION, "intentional error from test server");
}
/* }}} */

static void test_handler_big(yar_request *request, yar_response *response, void *data) /* {{{ */ {
	const yar_data *param = test_get_parameter(request, 0);
	unsigned int dummy = 0;
	long len = 0;

	if (param && (yar_unpack_data_type(param, &dummy) == YAR_DATA_LONG
			|| yar_unpack_data_type(param, &dummy) == YAR_DATA_ULONG)) {
		yar_unpack_data_long(param, &len);
	}

	if (len <= 0 || len > 8 * 1024 * 1024) {
		yar_response_set_error(response, TEST_ERR_EXCEPTION, "big expects a length in (0, 8M]");
		return;
	}

	{
		long i;
		char *buf = malloc(len);
		yar_packager *pk;

		if (!buf) {
			yar_response_set_error(response, TEST_ERR_EXCEPTION, "out of memory");
			return;
		}
		for (i = 0; i < len; i++) {
			buf[i] = 'a' + (char)(i % 26);
		}

		pk = yar_pack_start_null();
		yar_pack_push_string(pk, buf, len);
		free(buf);
		yar_response_set_retval(response, pk);
		yar_pack_free(pk);
	}
}
/* }}} */

static yar_server_handler test_handlers[] = {
	{"echo", sizeof("echo") - 1, test_handler_echo},
	{"add", sizeof("add") - 1, test_handler_add},
	{"types", sizeof("types") - 1, test_handler_types},
	{"sleep", sizeof("sleep") - 1, test_handler_sleep},
	{"error", sizeof("error") - 1, test_handler_error},
	{"big", sizeof("big") - 1, test_handler_big},
	{NULL, 0, NULL}
};

int main(int argc, char **argv) {
	int opt;
	int max_children = 0;
	int standalone = 0;
	int read_timeout = 10;
	char *hostname = NULL, *log_file = NULL, *pid_file = NULL;

	while ((opt = getopt(argc, argv, "S:n:l:p:X")) != -1) {
		switch (opt) {
			case 'S':
				hostname = optarg;
				break;
			case 'n':
				max_children = atoi(optarg);
				break;
			case 'l':
				log_file = optarg;
				break;
			case 'p':
				pid_file = optarg;
				break;
			case 'X':
				standalone = 1;
				break;
			default:
				fprintf(stderr, "usage: %s -S <host:port|/path/sock> [-n workers] [-l logfile] [-p pidfile] [-X]\n", argv[0]);
				return 2;
		}
	}

	if (!hostname) {
		fprintf(stderr, "usage: %s -S <host:port|/path/sock> [-n workers] [-l logfile] [-p pidfile] [-X]\n", argv[0]);
		return 2;
	}

	if (!yar_server_init(hostname)) {
		return 1;
	}
	yar_server_set_opt(YAR_STAND_ALONE, &standalone);
	yar_server_set_opt(YAR_MAX_CHILDREN, &max_children);
	yar_server_set_opt(YAR_READ_TIMEOUT, &read_timeout);
	if (log_file) {
		yar_server_set_opt(YAR_LOG_FILE, log_file);
	}
	if (pid_file) {
		yar_server_set_opt(YAR_PID_FILE, pid_file);
	}
	yar_server_register_handler(test_handlers);

	yar_server_run();

	return 0;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
