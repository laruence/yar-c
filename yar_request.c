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

#include <string.h>
#include "msgpack.h"

#include "yar_common.h"
#include "yar_pack.h"
#include "yar_request.h"

static char yar_request_keys[] = {'i', 'm', 'p'};

int yar_request_pack(yar_request *request, yar_payload *payload, int extra_bytes) /* {{{ */ {
	uint index;
	yar_packager *pk = yar_pack_start_map(3);

	for (index = 0; index < (sizeof(yar_request_keys) / sizeof(char)); index++) {
		switch (yar_request_keys[index]) {
			case 'i':
				yar_pack_push_string(pk, "i", 1);
				yar_pack_push_ulong(pk, request->id);
				break;
			case 'm':
				yar_pack_push_string(pk, "m", 1);
				yar_pack_push_string(pk, request->method, request->mlen);
				break;
			case 'p':
				{
					yar_packager *packager = (yar_packager *)request->out;

					yar_pack_push_string(pk, "p", 1);
					if (packager) {
						yar_pack_push_packager(pk, packager);
					} else {
						yar_pack_push_null(pk);
					}
				}
				break;
			default:
				continue;
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

int yar_request_unpack(yar_request *request, char *payload, uint len, int extra_bytes) /* {{{ */ {
	uint size;
	const yar_data *obj;
	yar_unpackager *unpk = yar_unpack_init(payload + extra_bytes, len - extra_bytes);

	if (!unpk) {
		return 0;
	}

	request->buffer = (void *)unpk;

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
					request->id = id;
				}
			} else if (strncmp(key, "m", sizeof("m") - 1) == 0) {
				if (yar_unpack_data_type(obj, &size) == YAR_DATA_STRING) {
					const char *method;
					yar_unpack_data_string(obj, &method);
					request->method = malloc(size);
					memcpy(request->method, method, size);
					request->mlen = size;
				}
			} else if (strncmp(key, "p", sizeof("p") - 1) == 0) {
				request->in = (void *)obj;
			} else {
				continue;
			}
		} while (yar_unpack_iterator_next(it));
		yar_unpack_iterator_free(it);
	}

	return 1;
}
/* }}} */

void yar_request_set_parameters(yar_request *request, yar_packager *packager) /* {{{ */ {
	if (request->out) {
		yar_pack_free((yar_packager *)request->out);
	}
	request->out  = yar_pack_start_null();
	yar_pack_push_packager((yar_packager *)request->out, packager);
}
/* }}} */

const yar_data * yar_request_get_parameters(yar_request *request) /* {{{ */ {
	return request->in;
}
/* }}} */

void yar_request_free(yar_request *request) /* {{{ */ {
	if (request->method) {
		free(request->method);
	}
	if (request->out) {
		yar_pack_free((yar_packager *)request->out);
	}
	if (request->buffer) {
		yar_unpack_free((yar_unpackager *)request->buffer);
	}
	if (request->body) {
		free(request->body);
	}
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
