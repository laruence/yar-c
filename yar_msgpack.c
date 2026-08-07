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

#include <stdlib.h>
#include <string.h>
#include "msgpack.h"

#include "yar_common.h"
#include "yar_pack.h"
#include "yar_msgpack.h"

/* encode: value tree -> msgpack bytes {{{ */

static int yar_msgpack_pack_data(msgpack_packer *pk, const yar_data *data) /* {{{ */ {
	uint size = 0;

	switch (yar_unpack_data_type(data, &size)) {
		case YAR_DATA_NULL:
			return msgpack_pack_nil(pk) >= 0;
		case YAR_DATA_BOOL:
			{
				int val = 0;
				yar_unpack_data_bool(data, &val);
				return val? msgpack_pack_true(pk) >= 0 : msgpack_pack_false(pk) >= 0;
			}
		case YAR_DATA_LONG:
			{
				long num = 0;
				yar_unpack_data_long(data, &num);
				return msgpack_pack_int64(pk, num) >= 0;
			}
		case YAR_DATA_ULONG:
			{
				ulong num = 0;
				yar_unpack_data_ulong(data, &num);
				return msgpack_pack_uint64(pk, num) >= 0;
			}
		case YAR_DATA_DOUBLE:
			{
				double num = 0;
				yar_unpack_data_value(data, &num);
				return msgpack_pack_double(pk, num) >= 0;
			}
		case YAR_DATA_STRING:
			{
				const char *str;
				yar_unpack_data_string(data, &str);
				if (msgpack_pack_str(pk, size) < 0) {
					return 0;
				}
				return msgpack_pack_str_body(pk, str, size) >= 0;
			}
		case YAR_DATA_ARRAY:
			{
				yar_unpack_iterator *it;

				if (msgpack_pack_array(pk, size) < 0) {
					return 0;
				}
				if (!size) {
					return 1;
				}
				it = yar_unpack_iterator_init(data);
				if (!it) {
					return 0;
				}
				do {
					if (!yar_msgpack_pack_data(pk, yar_unpack_iterator_current(it))) {
						yar_unpack_iterator_free(it);
						return 0;
					}
				} while (yar_unpack_iterator_next(it));
				yar_unpack_iterator_free(it);
			}
			return 1;
		case YAR_DATA_MAP:
			{
				yar_unpack_iterator *it;

				if (msgpack_pack_map(pk, size) < 0) {
					return 0;
				}
				if (!size) {
					return 1;
				}
				it = yar_unpack_iterator_init(data);
				if (!it) {
					return 0;
				}
				do {
					if (!yar_msgpack_pack_data(pk, yar_unpack_iterator_current(it))) {
						yar_unpack_iterator_free(it);
						return 0;
					}
				} while (yar_unpack_iterator_next(it));
				yar_unpack_iterator_free(it);
			}
			return 1;
		default:
			return 0;
	}
}
/* }}} */

int yar_msgpack_encode(const yar_data *data, yar_payload *out) /* {{{ */ {
	msgpack_sbuffer *bf;
	msgpack_packer *pk;
	int ret = 0;

	if (!data || !out) {
		return 0;
	}

	bf = msgpack_sbuffer_new();
	if (!bf) {
		return 0;
	}

	pk = msgpack_packer_new(bf, msgpack_sbuffer_write);
	if (!pk) {
		msgpack_sbuffer_free(bf);
		return 0;
	}

	if (yar_msgpack_pack_data(pk, data)) {
		out->data = malloc(bf->size);
		if (out->data) {
			if (bf->size) {
				memcpy(out->data, bf->data, bf->size);
			}
			out->size = bf->size;
			ret = 1;
		}
	}

	msgpack_packer_free(pk);
	msgpack_sbuffer_free(bf);

	return ret;
}
/* }}} */

/* }}} */

/* decode: msgpack bytes -> value tree {{{ */

static int yar_msgpack_unpack_object(yar_packager *pk, const msgpack_object *obj) /* {{{ */ {
	uint i;

	switch (obj->type) {
		case MSGPACK_OBJECT_NIL:
			return yar_pack_push_null(pk);
		case MSGPACK_OBJECT_BOOLEAN:
			return yar_pack_push_bool(pk, obj->via.boolean);
		case MSGPACK_OBJECT_POSITIVE_INTEGER:
			return yar_pack_push_ulong(pk, obj->via.u64);
		case MSGPACK_OBJECT_NEGATIVE_INTEGER:
			return yar_pack_push_long(pk, obj->via.i64);
		case MSGPACK_OBJECT_FLOAT32:
		case MSGPACK_OBJECT_FLOAT:
			return yar_pack_push_double(pk, obj->via.f64);
		case MSGPACK_OBJECT_STR:
			return yar_pack_push_string(pk, (char *)obj->via.str.ptr, obj->via.str.size);
		case MSGPACK_OBJECT_ARRAY:
			if (!yar_pack_push_array(pk, obj->via.array.size)) {
				return 0;
			}
			for (i = 0; i < obj->via.array.size; i++) {
				if (!yar_msgpack_unpack_object(pk, &obj->via.array.ptr[i])) {
					return 0;
				}
			}
			return 1;
		case MSGPACK_OBJECT_MAP:
			if (!yar_pack_push_map(pk, obj->via.map.size)) {
				return 0;
			}
			for (i = 0; i < obj->via.map.size; i++) {
				if (!yar_msgpack_unpack_object(pk, &obj->via.map.ptr[i].key)) {
					return 0;
				}
				if (!yar_msgpack_unpack_object(pk, &obj->via.map.ptr[i].val)) {
					return 0;
				}
			}
			return 1;
		default:
			/* BIN / EXT / anything else is not part of the yar type system */
			return 0;
	}
}
/* }}} */

yar_data * yar_msgpack_decode(const char *data, uint len) /* {{{ */ {
	msgpack_unpacked msg;
	yar_packager *pk;
	yar_data *root = NULL;

	if (!data || !len) {
		return NULL;
	}

	msgpack_unpacked_init(&msg);

	if (msgpack_unpack_next(&msg, data, len, NULL)) {
		pk = yar_pack_start(YAR_DATA_NULL, 0);
		if (pk) {
			if (yar_msgpack_unpack_object(pk, &msg.data)) {
				root = yar_pack_take_root(pk);
			}
			yar_pack_free(pk);
		}
	}

	msgpack_unpacked_destroy(&msg);

	return root;
}
/* }}} */

/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
