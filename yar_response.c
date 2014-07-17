/**
 * Yar - Concurrent RPC Server for PHP, C etc
 *
 * Copyright (C) 2012-2012 Xinchen Hui <laruence at gmail dot com>
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
#include <stdarg.h>     /* for va_list */
#include <string.h>
#include "msgpack.h"

#include "yar_common.h"
#include "yar_pack.h"
#include "yar_protocol.h"
#include "yar_response.h"

static char yar_response_keys[] = {'i', 's', 'r', 'e'};

int yar_response_pack(yar_response *response, yar_payload *payload, int extra_bytes) /* {{{ */ {
	uint index;
	yar_packager *pk = yar_pack_start_map(4);

	for (index = 0; index < (sizeof(yar_response_keys) / sizeof(char)); index++) {
		switch (yar_response_keys[index]) {
			case 'i':
				yar_pack_push_string(pk, "i", 1);
				yar_pack_push_ulong(pk, response->id);
				break;
			case 's':
				yar_pack_push_string(pk, "s", 1);
				yar_pack_push_long(pk, response->status);
				break;
			case 'r':
				{
					yar_packager *packager = response->out;
					yar_pack_push_string(pk, "r", 1);
					if (packager) {
						yar_pack_push_packager(pk, packager);
					} else {
						yar_pack_push_null(pk);
					}
				}
				break;
			case 'e':
				yar_pack_push_string(pk, "e", 1);
				if (response->error) {
					yar_pack_push_string(pk, response->error, response->elen);
				} else {
					yar_pack_push_null(pk);
				}
				break;
			default:
				break;
		}
	}

	{
		yar_payload tmp;
		yar_pack_to_string(pk, &tmp);
		payload->data = malloc(tmp.size + extra_bytes);
		memcpy(payload->data + extra_bytes, tmp.data, tmp.size);
		payload->size = tmp.size + extra_bytes;
	}

	yar_pack_free(pk);

	return 1;
} 
/* }}} */

int yar_response_unpack(yar_response *response, char *payload, uint len, int extra_bytes) /* {{{ */ {
	uint size;
	const yar_data *obj;
	yar_unpackager *unpk = yar_unpack_init(payload + extra_bytes, len - extra_bytes);

	if (!unpk) {
		return 0;
	}

	response->buffer = (void *)unpk;

	obj = yar_unpack_unpack(unpk);
	if (yar_unpack_data_type(obj, &size) != YAR_DATA_MAP || size < 2) {
		return 0;
	} else {
		yar_unpack_iterator *it = yar_unpack_iterator_init(obj);
		do {
			const char *key;
			obj = yar_unpack_iterator_current(it);
			if (yar_unpack_data_type(obj, &size) != YAR_DATA_STRING) {
				yar_unpack_iterator_free(it);
				return 0;
			}
			yar_unpack_data_string(obj, &key);
			/* fetch value */
			if (!yar_unpack_iterator_next(it)) {
				yar_unpack_iterator_free(it);
				return 0;
			}
			obj = yar_unpack_iterator_current(it);
			if (strncmp(key, "i", sizeof("i") - 1) == 0) {
				if (yar_unpack_data_type(obj, &size) == YAR_DATA_ULONG) {
					ulong id;
					yar_unpack_data_ulong(obj, &id);
					response->id = id;
				}
			} else if (strncmp(key, "s", sizeof("s") - 1) == 0) {
				if (yar_unpack_data_type(obj, &size) == YAR_DATA_ULONG) {
					long status;
					yar_unpack_data_long(obj, &status);
					response->status = status;
				}
			} else if (strncmp(key, "e", sizeof("e") - 1) == 0) {
				if (yar_unpack_data_type(obj, &size) == YAR_DATA_STRING) {
					const char *errmsg;
					yar_unpack_data_string(obj, &errmsg);
					response->error = malloc(size);
					memcpy(response->error, errmsg, size);
					response->elen = size;
				}
			} else if (strncmp(key, "r", sizeof("r") - 1) == 0) {
				response->in = (void *)obj;
			} else {
				continue;
			}
		} while (yar_unpack_iterator_next(it));
		yar_unpack_iterator_free(it);
	}

	return 1;
}
/* }}} */

void yar_response_set_error(yar_response *response, int code, const char *fmt, ...) /* {{{ */ {
	va_list args;
	uint len;
	char buf[1024];

	va_start(args, fmt);
	len = vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	if (len >= sizeof(buf)) {
		memcpy(buf + sizeof(buf) - sizeof("..."), "...", sizeof("...") - 1);
		len = sizeof(buf) - 1;
	}

	response->status = code;
	response->error = malloc(len);
	memcpy(response->error, buf, len);
	response->elen = len;
}
/* }}} */

void yar_response_set_retval(yar_response *response, yar_packager *packager) /* {{{ */ {
	if (response->out) {
		yar_pack_free((yar_packager *)response->out);
	}
	response->out = yar_pack_start_null();
	yar_pack_push_packager((yar_packager *)response->out, packager);
}
/* }}} */

const yar_data * yar_response_get_response(yar_response *response) /* {{{ */ {
	return (const yar_data *)response->in;
}
/* }}} */

int yar_response_get_status(yar_response *response) /* {{{ */ {
	return response->status;
}
/* }}} */

int yar_response_get_error(yar_response *response, const char **msg, uint *len) /* {{{ */ {
	if (!response->elen) {
		return 0;
	}

	*msg = response->error;
	*len = response->elen;
	return 1;
}
/* }}} */

void yar_response_free(yar_response *response) /* {{{ */ { 
	if (response->payload.data) {
		free(response->payload.data);
	}
	if (response->error) {
		free(response->error);
	}
	if (response->out) {
		yar_pack_free((yar_packager *)response->out);
	}
	if (response->buffer) {
		yar_unpack_free((yar_unpackager *)response->buffer);
	}
}
/*}}}*/

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
