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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "yar_common.h"
#include "yar_pack.h"
#include "yar_json.h"

#ifdef HAVE_JSON

#include <cjson/cJSON.h>

/* decode: JSON text -> value tree {{{ */

static int yar_json_pack_item(yar_packager *pk, const cJSON *item) /* {{{ */ {
	const cJSON *child;

	if (!item) {
		return 0;
	}

	if (cJSON_IsNull(item)) {
		return yar_pack_push_null(pk);
	}
	if (cJSON_IsFalse(item)) {
		return yar_pack_push_bool(pk, 0);
	}
	if (cJSON_IsTrue(item)) {
		return yar_pack_push_bool(pk, 1);
	}
	if (cJSON_IsNumber(item)) {
		double num = item->valuedouble;
		/* JSON numbers carry no type information, keep integral values as
		 * integers (in line with php json_decode()); doubles are only
		 * representable exactly up to 2^53 */
		if (num >= -9007199254740992.0 && num <= 9007199254740992.0
				&& num == (double)(long)num) {
			if (num >= 0) {
				return yar_pack_push_ulong(pk, (ulong)num);
			}
			return yar_pack_push_long(pk, (long)num);
		}
		return yar_pack_push_double(pk, num);
	}
	if (cJSON_IsString(item)) {
		if (!item->valuestring) {
			return 0;
		}
		return yar_pack_push_string(pk, item->valuestring, strlen(item->valuestring));
	}
	if (cJSON_IsArray(item)) {
		if (!yar_pack_push_array(pk, cJSON_GetArraySize((cJSON *)item))) {
			return 0;
		}
		for (child = item->child; child; child = child->next) {
			if (!yar_json_pack_item(pk, child)) {
				return 0;
			}
		}
		return 1;
	}
	if (cJSON_IsObject(item)) {
		if (!yar_pack_push_map(pk, cJSON_GetArraySize((cJSON *)item))) {
			return 0;
		}
		for (child = item->child; child; child = child->next) {
			if (!child->string) {
				return 0;
			}
			if (!yar_pack_push_string(pk, (char *)child->string, strlen(child->string))) {
				return 0;
			}
			if (!yar_json_pack_item(pk, child)) {
				return 0;
			}
		}
		return 1;
	}

	/* cJSON_Raw / cJSON_Invalid / cJSON_Reference are not expected in parsed input */
	return 0;
}
/* }}} */

yar_data * yar_json_decode(const char *json, uint len) /* {{{ */ {
	cJSON *root_cjson;
	yar_packager *pk;
	yar_data *root = NULL;
	const char *end = NULL;

	if (!json || !len) {
		return NULL;
	}

	root_cjson = cJSON_ParseWithLengthOpts(json, len, &end, 0);
	if (!root_cjson) {
		return NULL;
	}

	/* cJSON accepts trailing garbage, the protocol must not */
	if (end) {
		const char *p;
		for (p = end; p < json + len; p++) {
			if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
				cJSON_Delete(root_cjson);
				return NULL;
			}
		}
	}

	pk = yar_pack_start(YAR_DATA_NULL, 0); /* empty packager, the root item is pushed below */
	if (pk) {
		if (yar_json_pack_item(pk, root_cjson)) {
			root = yar_pack_take_root(pk);
		}
		yar_pack_free(pk);
	}

	cJSON_Delete(root_cjson);

	return root;
}
/* }}} */

/* }}} */

/* encode: value tree -> JSON text {{{ */

/* shortest decimal that round-trips to the same double (17 digits always do),
 * keeps the wire format close to php json_encode() */
static void yar_json_format_double(double num, char *buf, uint buflen) /* {{{ */ {
	int precision;

	for (precision = 15; precision <= 16; precision++) {
		char *check_end;
		double parsed;

		snprintf(buf, buflen, "%.*g", precision, num);
		parsed = strtod(buf, &check_end);
		if (check_end != buf && *check_end == '\0' && parsed == num) {
			return;
		}
	}

	snprintf(buf, buflen, "%.17g", num);
}
/* }}} */

static cJSON *yar_json_from_data(const yar_data *data) /* {{{ */ {
	uint size = 0;

	switch (yar_unpack_data_type(data, &size)) {
		case YAR_DATA_NULL:
			return cJSON_CreateNull();
		case YAR_DATA_BOOL:
			{
				int bval = 0;
				yar_unpack_data_bool(data, &bval);
				return bval? cJSON_CreateTrue() : cJSON_CreateFalse();
			}
		case YAR_DATA_LONG:
			{
				/* emit integers verbatim, cJSON would print large ones in
				 * scientific notation with a loss of precision */
				long num = 0;
				char buf[24];
				yar_unpack_data_long(data, &num);
				snprintf(buf, sizeof(buf), "%ld", num);
				return cJSON_CreateRaw(buf);
			}
		case YAR_DATA_ULONG:
			{
				ulong num = 0;
				char buf[24];
				yar_unpack_data_ulong(data, &num);
				snprintf(buf, sizeof(buf), "%lu", num);
				return cJSON_CreateRaw(buf);
			}
		case YAR_DATA_DOUBLE:
			{
				double num = 0;
				char buf[40];
				yar_unpack_data_value(data, &num);
				if (num != num || num - num != 0) { /* NaN or Inf, not representable in JSON */
					return cJSON_CreateNull();
				}
				yar_json_format_double(num, buf, sizeof(buf));
				return cJSON_CreateRaw(buf);
			}
		case YAR_DATA_STRING:
			{
				const char *str;
				yar_unpack_data_string(data, &str);
				/* embedded NUL bytes can not be represented in JSON, reject
				 * instead of truncating */
				if (memchr(str, '\0', size)) {
					return NULL;
				}
				return cJSON_CreateString(str);
			}
		case YAR_DATA_ARRAY:
			{
				yar_unpack_iterator *it;
				cJSON *arr;

				if (!size) {
					return cJSON_CreateArray();
				}
				it = yar_unpack_iterator_init(data);
				arr = cJSON_CreateArray();
				if (!it || !arr) {
					if (it) {
						yar_unpack_iterator_free(it);
					}
					if (arr) {
						cJSON_Delete(arr);
					}
					return NULL;
				}
				do {
					cJSON *child = yar_json_from_data(yar_unpack_iterator_current(it));
					if (!child) {
						yar_unpack_iterator_free(it);
						cJSON_Delete(arr);
						return NULL;
					}
					cJSON_AddItemToArray(arr, child);
				} while (yar_unpack_iterator_next(it));
				yar_unpack_iterator_free(it);
				return arr;
			}
		case YAR_DATA_MAP:
			{
				yar_unpack_iterator *it;
				cJSON *obj;

				if (!size) {
					return cJSON_CreateObject();
				}
				it = yar_unpack_iterator_init(data);
				obj = cJSON_CreateObject();
				if (!it || !obj) {
					if (it) {
						yar_unpack_iterator_free(it);
					}
					if (obj) {
						cJSON_Delete(obj);
					}
					return NULL;
				}
				do {
					const yar_data *key = yar_unpack_iterator_current(it);
					const yar_data *val;
					char key_buf[24];
					const char *key_str = NULL;
					uint key_type, key_len = 0;
					cJSON *child;

					if (!yar_unpack_iterator_next(it)) {
						/* a map must have values for its keys */
						goto map_failed;
					}
					val = yar_unpack_iterator_current(it);

					key_type = yar_unpack_data_type(key, &key_len);
					if (key_type == YAR_DATA_STRING) {
						yar_unpack_data_string(key, &key_str);
						if (memchr(key_str, '\0', key_len)) {
							goto map_failed;
						}
					} else if (key_type == YAR_DATA_LONG) {
						long num = 0;
						yar_unpack_data_long(key, &num);
						snprintf(key_buf, sizeof(key_buf), "%ld", num);
						key_str = key_buf;
					} else if (key_type == YAR_DATA_ULONG) {
						ulong num = 0;
						yar_unpack_data_ulong(key, &num);
						snprintf(key_buf, sizeof(key_buf), "%lu", num);
						key_str = key_buf;
					} else {
						/* JSON object keys must be strings */
						goto map_failed;
					}

					child = yar_json_from_data(val);
					if (!child) {
						goto map_failed;
					}
					/* cJSON_AddItemToObject() duplicates the key */
					if (!cJSON_AddItemToObject(obj, key_str, child)) {
						cJSON_Delete(child);
						goto map_failed;
					}
				} while (yar_unpack_iterator_next(it));
				yar_unpack_iterator_free(it);
				return obj;

map_failed:
				yar_unpack_iterator_free(it);
				cJSON_Delete(obj);
				return NULL;
			}
		default:
			return NULL;
	}
}
/* }}} */

int yar_json_encode(const yar_data *data, yar_payload *out) /* {{{ */ {
	cJSON *json;
	char *text;

	if (!data || !out) {
		return 0;
	}

	json = yar_json_from_data(data);
	if (!json) {
		return 0;
	}

	text = cJSON_PrintUnformatted(json);
	cJSON_Delete(json);

	if (!text) {
		return 0;
	}

	out->data = text;
	out->size = strlen(text);

	return 1;
}
/* }}} */

/* }}} */

#else /* no cJSON, stubs */

int yar_json_encode(const yar_data *data, yar_payload *out) /* {{{ */ {
	(void)data;
	(void)out;
	return 0;
}
/* }}} */

yar_data * yar_json_decode(const char *data, uint len) /* {{{ */ {
	(void)data;
	(void)len;
	return NULL;
}
/* }}} */

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
