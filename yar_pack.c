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

struct _yar_packager {
	msgpack_packer *pk;
	msgpack_sbuffer *bf;
};

struct _yar_data {
	msgpack_object obj;
};

struct _yar_unpackager {
	msgpack_unpacked msg;
};

struct _yar_unpack_iterator {
	uint size;
	uint position;
	yar_data *data;
};

yar_packager * yar_pack_start(yar_data_type type, uint size) /* {{{ */ {
	yar_packager *packager = malloc(sizeof(yar_packager));
	
	packager->bf = msgpack_sbuffer_new();
	packager->pk = msgpack_packer_new(packager->bf, msgpack_sbuffer_write);

	switch (type) {
		case YAR_DATA_ARRAY:
			msgpack_pack_array(packager->pk, size);
			break;
		case YAR_DATA_MAP:
			msgpack_pack_map(packager->pk, size);
			break;
		default:
			break;
	}

	return packager;
}
/* }}} */

int yar_pack_push_array(yar_packager *packager, uint size) /* {{{ */ {
	msgpack_packer *pk = packager->pk;
	
	return msgpack_pack_array(pk, size) < 0? 0: 1;
}
/* }}} */

int yar_pack_push_map(yar_packager *packager, uint size) /* {{{ */ {
	msgpack_packer *pk = packager->pk;

	return msgpack_pack_map(pk, size) < 0? 0 : 1;
}
/* }}} */

int yar_pack_push_null(yar_packager *packager) /* {{{ */ {
	msgpack_packer *pk = packager->pk;

	msgpack_pack_nil(pk);

	return 1;
}
/* }}} */

int yar_pack_push_bool(yar_packager *packager, int val) /* {{{ */ {
	msgpack_packer *pk = packager->pk;

	if (val == 0) {
		if (msgpack_pack_false(pk) < 0) {
			return 0;
		}
	} else {
		if (msgpack_pack_true(pk) < 0) {
			return 0;
		}
	}

	return 1;
}
/* }}} */

int yar_pack_push_long(yar_packager *packager, long num) /* {{{ */ {
	msgpack_packer *pk = packager->pk;

	return msgpack_pack_int64(pk, num) < 0? 0 : 1;
}
/* }}} */

int yar_pack_push_ulong(yar_packager *packager, ulong num) /* {{{ */ {
	msgpack_packer *pk = packager->pk;

	return msgpack_pack_uint64(pk, num) < 0? 0 : 1;
}
/* }}} */

int yar_pack_push_double(yar_packager *packager, double num) /* {{{ */ {
	msgpack_packer *pk = packager->pk;

	return msgpack_pack_double(pk, num) < 0? 0 : 1;
}
/* }}} */

int yar_pack_push_string(yar_packager *packager, char *str, uint len) /* {{{ */ {
	int ret;
	msgpack_packer *pk = packager->pk;

#if defined(MSGPACK_OBJECT_RAW)
	ret = msgpack_pack_raw(pk, len);

	if (ret < 0) {
		return 0;
	}
	
	if (msgpack_pack_raw_body(pk, str, len) < 0) {
		return 0;
	}
#else
	ret = msgpack_pack_str(pk, len);

	if (ret < 0) {
		return 0;
	}

	if (msgpack_pack_str_body(pk, str, len) < 0) {
		return 0;
	}
#endif

	return 1;
}
/* }}} */

int yar_pack_push_data(yar_packager *packager, const yar_data *data) /* {{{ */ {
	msgpack_packer *pk = packager->pk;

	return msgpack_pack_object(pk, *(msgpack_object *)data) < 0? 0 : 1;
}
/* }}} */

int yar_pack_push_packager(yar_packager *packager, yar_packager *data) /* {{{ */ {
	msgpack_sbuffer *buffer = packager->bf;
	yar_payload payload = {0};

	yar_pack_to_string(data, &payload);
	return msgpack_sbuffer_write(buffer, payload.data, payload.size) < 0? 0 : 1;
}
/* }}} */

int yar_pack_to_string(yar_packager *packager, yar_payload *payload) /* {{{ */ {
	msgpack_sbuffer *bf = packager->bf;
	payload->size = bf->size;
	payload->data = bf->data;
	return 1;
}
/* }}} */

void yar_pack_free(yar_packager *packager) /* {{{ */ {
	msgpack_sbuffer_free(packager->bf);
	msgpack_packer_free(packager->pk); 
	free(packager);
}
/* }}} */

void yar_unpack_free(yar_unpackager *unpk) /* {{{ */ {
	msgpack_unpacked_destroy(&unpk->msg);
	free(unpk);
}
/* }}} */

yar_unpackager * yar_unpack_init(char *data, uint len) /* {{{ */ {
	yar_unpackager *unpk = malloc(sizeof(yar_unpackager));
	msgpack_unpacked_init(&unpk->msg);
	if (!msgpack_unpack_next(&unpk->msg, data, len, NULL)) {
		yar_unpack_free(unpk);
		return NULL;
	}
	return unpk;
}
/* }}} */

const yar_data * yar_unpack_unpack(yar_unpackager *unpk) /* {{{ */ {
	return (const yar_data *)&unpk->msg.data;
}
/* }}} */

yar_data_type yar_unpack_data_type(const yar_data *data, uint *size) /* {{{ */ {
	msgpack_object *obj = (msgpack_object *)data;

	switch (obj->type) {
		case MSGPACK_OBJECT_NIL:
			return YAR_DATA_NULL;
		case MSGPACK_OBJECT_BOOLEAN:
			return YAR_DATA_BOOL;
		case MSGPACK_OBJECT_POSITIVE_INTEGER:
			return YAR_DATA_ULONG;
		case MSGPACK_OBJECT_NEGATIVE_INTEGER:
			return YAR_DATA_LONG;
#if defined(MSGPACK_USE_LEGACY_NAME_AS_FLOAT)
		case MSGPACK_OBJECT_DOUBLE:
			return YAR_DATA_DOUBLE;
#else
		case MSGPACK_OBJECT_FLOAT:
			return YAR_DATA_DOUBLE;
#endif
#if defined(MSGPACK_USE_LEGACY_NAME_AS_FLOAT)
		case MSGPACK_OBJECT_RAW:
			*size = obj->via.raw.size;
			return YAR_DATA_STRING;
#else
		case MSGPACK_OBJECT_STR:
			*size = obj->via.str.size;
			return YAR_DATA_STRING;
#endif
		case MSGPACK_OBJECT_ARRAY:
			*size = obj->via.array.size;
			return YAR_DATA_ARRAY;
		case MSGPACK_OBJECT_MAP:
			*size = obj->via.map.size;
			return YAR_DATA_MAP;
	}
	return 0;
}
/* }}} */

int yar_unpack_data_value(const yar_data *data, void *arg) /* {{{ */ {
	msgpack_object *obj = (msgpack_object *)data;

	switch (obj->type) {
		case MSGPACK_OBJECT_NIL:
			*(void **)arg = NULL;
			return YAR_DATA_NULL;
		case MSGPACK_OBJECT_BOOLEAN:
			*(int *)arg = obj->via.boolean;
			return YAR_DATA_BOOL;
		case MSGPACK_OBJECT_POSITIVE_INTEGER:
			*(ulong *)arg = obj->via.u64;
			return YAR_DATA_ULONG;
		case MSGPACK_OBJECT_NEGATIVE_INTEGER:
			*(long *)arg = obj->via.i64;
			return YAR_DATA_LONG;
#if defined(MSGPACK_USE_LEGACY_NAME_AS_FLOAT)
		case MSGPACK_OBJECT_DOUBLE:
			*(double *)arg = obj->via.dec;
#else
		case MSGPACK_OBJECT_FLOAT:
#endif
			*(double *)arg = obj->via.f64;
			return YAR_DATA_DOUBLE;
#if defined(MSGPACK_USE_LEGACY_NAME_AS_FLOAT)
		case MSGPACK_OBJECT_RAW:
			*(const char **)arg = obj->via.raw.ptr;
#else
		case MSGPACK_OBJECT_STR:
			*(const char **)arg = obj->via.str.ptr;
#endif
			return YAR_DATA_STRING;
		case MSGPACK_OBJECT_ARRAY:
			*(const yar_data **)arg = (yar_data *)obj->via.array.ptr;
			return YAR_DATA_ARRAY;
		case MSGPACK_OBJECT_MAP:
			*(const yar_data **)arg = (yar_data *)obj->via.map.ptr;
			return YAR_DATA_MAP;
	}
	return 0;
}
/* }}} */

int yar_unpack_data_null(const yar_data *data, int *val) /* {{{ */ {
	return yar_unpack_data_value(data, &val);
}
/* }}} */

int yar_unpack_data_bool(const yar_data *data, int *bval) /* {{{ */ {
	return yar_unpack_data_value(data, bval);
}
/* }}} */

int yar_unpack_data_long(const yar_data *data, long *num) /* {{{ */ {
	return yar_unpack_data_value(data, num);
}
/* }}} */

int yar_unpack_data_ulong(const yar_data *data, ulong *num) /* {{{ */ {
	return yar_unpack_data_value(data, num);
}
/* }}} */

int yar_unpack_data_string(const yar_data *data, const char **str) /* {{{ */ {
	return yar_unpack_data_value(data, str);
}
/* }}} */

int yar_unpack_data_array(const yar_data *data, const yar_data **arg) /* {{{ */ {
	return yar_unpack_data_value(data, arg);
}
/* }}} */

int yar_unpack_data_map(const yar_data *data, const yar_data **arg) /* {{{ */ {
	return yar_unpack_data_value(data, arg);
}
/* }}} */

yar_unpack_iterator * yar_unpack_iterator_init(const yar_data *data) /* {{{ */ {
	yar_unpack_iterator *it = NULL;

	switch (data->obj.type) {
		case MSGPACK_OBJECT_ARRAY:
			{
				it = malloc(sizeof(yar_unpack_iterator));
				it->position = 0;
				it->size = data->obj.via.array.size;
				it->data = (yar_data *)data->obj.via.array.ptr;
			}
			break;
		case MSGPACK_OBJECT_MAP:
			{
				it = malloc(sizeof(yar_unpack_iterator));
				it->position = 0;
				it->size = data->obj.via.map.size * 2; /* kv */
				it->data = (yar_data *)data->obj.via.map.ptr;
			}
			break;
		default:
			break;
	}

	return it;
}
/* }}} */

void yar_unpack_iterator_reset(yar_unpack_iterator *it) /* {{{ */ {
	it->position = 0;
}
/* }}} */

int yar_unpack_iterator_next(yar_unpack_iterator *it) /* {{{ */ {
	if (it->position + 1 < it->size) {
		return ++it->position;
	}
	return 0;
}
/* }}} */

const yar_data *yar_unpack_iterator_current(yar_unpack_iterator *it) /* {{{ */ {
	msgpack_object *obj = (msgpack_object *)it->data;
	obj += it->position;
	return (yar_data *)obj;
}
/* }}} */

void yar_unpack_iterator_free(yar_unpack_iterator *it) /* {{{ */ {
	free(it);
}
/* }}} */

void yar_debug_print_data(const yar_data *data, FILE *fp) /* {{{ */ {
	msgpack_object_print(fp? fp : stdout, *(msgpack_object *)data);
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
