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
#include "yar_msgpack.h"
#include "yar_json.h"

/* yar_data is a format-agnostic, self-owned value tree. Strings own their
 * bytes (allocated size+1, always with a trailing NUL for convenience,
 * embedded NULs allowed), arrays/maps own a flat array of child nodes.
 * The msgpack and JSON wire formats are just codecs over this tree, see
 * yar_msgpack.c and yar_json.c */
struct _yar_data {
	yar_data_type type;
	union {
		int boolean;
		long i64;
		ulong u64;
		double f64;
		struct { char *ptr; uint size; } str;
		struct { yar_data *ptr; uint size; } array;
		struct { yar_data *ptr; uint size; } map; /* size counts pairs, ptr holds size * 2 nodes */
	} via;
};

/* the builder appends nodes to the tree under construction; open containers
 * (their size is declared upfront) are tracked on a stack */
typedef struct _yar_pack_frame {
	yar_data *children;
	uint total;
	uint filled;
} yar_pack_frame;

struct _yar_packager {
	yar_data *root;
	yar_pack_frame *stack;
	int depth;
	int capacity;
};

struct _yar_unpackager {
	yar_data *root;
};

struct _yar_unpack_iterator {
	uint size;
	uint position;
	yar_data *data;
};

/* value tree maintenance {{{ */

/* release owned contents and zero the node, does not free the node itself;
 * safe on already-destroyed (zeroed) nodes and on nodes whose children are
 * only partially initialized (uninitialized children are zero/type 0) */
void yar_data_destroy(yar_data *data) /* {{{ */ {
	uint i;

	switch (data->type) {
		case YAR_DATA_STRING:
			free(data->via.str.ptr);
			break;
		case YAR_DATA_ARRAY:
			for (i = 0; i < data->via.array.size; i++) {
				yar_data_destroy(&data->via.array.ptr[i]);
			}
			free(data->via.array.ptr);
			break;
		case YAR_DATA_MAP:
			for (i = 0; i < data->via.map.size * 2; i++) {
				yar_data_destroy(&data->via.map.ptr[i]);
			}
			free(data->via.map.ptr);
			break;
		default:
			break;
	}

	memset(data, 0, sizeof(yar_data));
}
/* }}} */

void yar_data_free(yar_data *data) /* {{{ */ {
	if (data) {
		yar_data_destroy(data);
		free(data);
	}
}
/* }}} */

/* deep copy the contents of src into (uninitialized) dst; on failure dst is
 * either untouched or destroyed+zeroed */
static int yar_data_dup_contents(const yar_data *src, yar_data *dst) /* {{{ */ {
	uint i, n;

	switch (src->type) {
		case YAR_DATA_NULL:
			dst->type = YAR_DATA_NULL;
			return 1;
		case YAR_DATA_BOOL:
			dst->type = YAR_DATA_BOOL;
			dst->via.boolean = src->via.boolean;
			return 1;
		case YAR_DATA_LONG:
			dst->type = YAR_DATA_LONG;
			dst->via.i64 = src->via.i64;
			return 1;
		case YAR_DATA_ULONG:
			dst->type = YAR_DATA_ULONG;
			dst->via.u64 = src->via.u64;
			return 1;
		case YAR_DATA_DOUBLE:
			dst->type = YAR_DATA_DOUBLE;
			dst->via.f64 = src->via.f64;
			return 1;
		case YAR_DATA_STRING:
			{
				char *buf = malloc(src->via.str.size + 1);
				if (!buf) {
					return 0;
				}
				if (src->via.str.size) {
					memcpy(buf, src->via.str.ptr, src->via.str.size);
				}
				buf[src->via.str.size] = '\0';
				dst->type = YAR_DATA_STRING;
				dst->via.str.ptr = buf;
				dst->via.str.size = src->via.str.size;
			}
			return 1;
		case YAR_DATA_ARRAY:
			n = src->via.array.size;
			dst->type = YAR_DATA_ARRAY;
			dst->via.array.size = n;
			dst->via.array.ptr = n? calloc(n, sizeof(yar_data)) : NULL;
			if (n && !dst->via.array.ptr) {
				yar_data_destroy(dst);
				return 0;
			}
			for (i = 0; i < n; i++) {
				if (!yar_data_dup_contents(&src->via.array.ptr[i], &dst->via.array.ptr[i])) {
					yar_data_destroy(dst);
					return 0;
				}
			}
			return 1;
		case YAR_DATA_MAP:
			n = src->via.map.size * 2;
			dst->type = YAR_DATA_MAP;
			dst->via.map.size = src->via.map.size;
			dst->via.map.ptr = n? calloc(n, sizeof(yar_data)) : NULL;
			if (n && !dst->via.map.ptr) {
				yar_data_destroy(dst);
				return 0;
			}
			for (i = 0; i < n; i++) {
				if (!yar_data_dup_contents(&src->via.map.ptr[i], &dst->via.map.ptr[i])) {
					yar_data_destroy(dst);
					return 0;
				}
			}
			return 1;
		default:
			return 0;
	}
}
/* }}} */

yar_data * yar_data_dup(const yar_data *data) /* {{{ */ {
	yar_data *dup;

	if (!data) {
		return NULL;
	}

	dup = malloc(sizeof(yar_data));
	if (!dup) {
		return NULL;
	}

	if (!yar_data_dup_contents(data, dup)) {
		free(dup);
		return NULL;
	}

	return dup;
}
/* }}} */

/* }}} */

/* codec dispatch {{{ */

int yar_data_pack(const yar_data *data, yar_payload *out, yar_packager_type type) /* {{{ */ {
	if (!data || !out) {
		return 0;
	}

	switch (type) {
		case YAR_PACKAGER_MSGPACK:
			return yar_msgpack_encode(data, out);
		case YAR_PACKAGER_JSON:
			return yar_json_encode(data, out);
		default:
			return 0;
	}
}
/* }}} */

yar_data * yar_data_unpack(const char *data, uint len, yar_packager_type type) /* {{{ */ {
	if (!data || !len) {
		return NULL;
	}

	switch (type) {
		case YAR_PACKAGER_MSGPACK:
			return yar_msgpack_decode(data, len);
		case YAR_PACKAGER_JSON:
			return yar_json_decode(data, len);
		default:
			return NULL;
	}
}
/* }}} */

int yar_packager_available(yar_packager_type type) /* {{{ */ {
	switch (type) {
		case YAR_PACKAGER_MSGPACK:
			return 1;
		case YAR_PACKAGER_JSON:
#ifdef HAVE_JSON
			return 1;
#else
			return 0;
#endif
		default:
			return 0;
	}
}
/* }}} */

/* }}} */

/* builder {{{ */

static int packager_push_frame(yar_packager *packager, yar_data *children, uint total) /* {{{ */ {
	if (packager->depth == packager->capacity) {
		int capacity = packager->capacity? packager->capacity * 2 : 8;
		yar_pack_frame *stack = realloc(packager->stack, sizeof(yar_pack_frame) * capacity);
		if (!stack) {
			return 0;
		}
		packager->stack = stack;
		packager->capacity = capacity;
	}

	packager->stack[packager->depth].children = children;
	packager->stack[packager->depth].total = total;
	packager->stack[packager->depth].filled = 0;
	packager->depth++;

	return 1;
}
/* }}} */

/* the slot the next pushed value goes into; NULL once the tree is complete */
static yar_data * packager_next_slot(yar_packager *packager) /* {{{ */ {
	if (packager->depth > 0) {
		yar_pack_frame *frame = &packager->stack[packager->depth - 1];
		return &frame->children[frame->filled];
	}

	if (packager->root) {
		return NULL; /* already complete */
	}

	packager->root = malloc(sizeof(yar_data));
	return packager->root;
}
/* }}} */

static void packager_commit(yar_packager *packager) /* {{{ */ {
	if (packager->depth > 0) {
		yar_pack_frame *frame = &packager->stack[packager->depth - 1];
		if (++frame->filled == frame->total) {
			packager->depth--;
		}
	}
}
/* }}} */

yar_packager * yar_pack_start(yar_data_type type, uint size) /* {{{ */ {
	yar_packager *packager = calloc(1, sizeof(yar_packager));

	if (!packager) {
		return NULL;
	}

	if (type == YAR_DATA_ARRAY || type == YAR_DATA_MAP) {
		uint count = (type == YAR_DATA_ARRAY)? size : size * 2;
		yar_data *children = NULL;

		packager->root = malloc(sizeof(yar_data));
		if (!packager->root) {
			free(packager);
			return NULL;
		}

		if (count) {
			children = calloc(count, sizeof(yar_data));
			if (!children) {
				free(packager->root);
				free(packager);
				return NULL;
			}
		}

		packager->root->type = type;
		if (type == YAR_DATA_ARRAY) {
			packager->root->via.array.ptr = children;
			packager->root->via.array.size = size;
		} else {
			packager->root->via.map.ptr = children;
			packager->root->via.map.size = size;
		}

		if (count && !packager_push_frame(packager, children, count)) {
			free(children);
			free(packager->root);
			free(packager);
			return NULL;
		}
	}
	/* scalar types: root stays NULL until the first push */

	return packager;
}
/* }}} */

int yar_pack_push_array(yar_packager *packager, uint size) /* {{{ */ {
	yar_data *slot, *children = NULL;

	if (size) {
		children = calloc(size, sizeof(yar_data));
		if (!children) {
			return 0;
		}
	}

	slot = packager_next_slot(packager);
	if (!slot) {
		free(children);
		return 0;
	}

	slot->type = YAR_DATA_ARRAY;
	slot->via.array.ptr = children;
	slot->via.array.size = size;
	packager_commit(packager);

	if (size && !packager_push_frame(packager, children, size)) {
		return 0;
	}

	return 1;
}
/* }}} */

int yar_pack_push_map(yar_packager *packager, uint size) /* {{{ */ {
	yar_data *slot, *children = NULL;

	if (size) {
		children = calloc(size * 2, sizeof(yar_data));
		if (!children) {
			return 0;
		}
	}

	slot = packager_next_slot(packager);
	if (!slot) {
		free(children);
		return 0;
	}

	slot->type = YAR_DATA_MAP;
	slot->via.map.ptr = children;
	slot->via.map.size = size;
	packager_commit(packager);

	if (size && !packager_push_frame(packager, children, size * 2)) {
		return 0;
	}

	return 1;
}
/* }}} */

int yar_pack_push_null(yar_packager *packager) /* {{{ */ {
	yar_data *slot = packager_next_slot(packager);

	if (!slot) {
		return 0;
	}

	slot->type = YAR_DATA_NULL;
	packager_commit(packager);

	return 1;
}
/* }}} */

int yar_pack_push_bool(yar_packager *packager, int val) /* {{{ */ {
	yar_data *slot = packager_next_slot(packager);

	if (!slot) {
		return 0;
	}

	slot->type = YAR_DATA_BOOL;
	slot->via.boolean = val? 1 : 0;
	packager_commit(packager);

	return 1;
}
/* }}} */

int yar_pack_push_long(yar_packager *packager, long num) /* {{{ */ {
	yar_data *slot = packager_next_slot(packager);

	if (!slot) {
		return 0;
	}

	slot->type = YAR_DATA_LONG;
	slot->via.i64 = num;
	packager_commit(packager);

	return 1;
}
/* }}} */

int yar_pack_push_ulong(yar_packager *packager, ulong num) /* {{{ */ {
	yar_data *slot = packager_next_slot(packager);

	if (!slot) {
		return 0;
	}

	slot->type = YAR_DATA_ULONG;
	slot->via.u64 = num;
	packager_commit(packager);

	return 1;
}
/* }}} */

int yar_pack_push_double(yar_packager *packager, double num) /* {{{ */ {
	yar_data *slot = packager_next_slot(packager);

	if (!slot) {
		return 0;
	}

	slot->type = YAR_DATA_DOUBLE;
	slot->via.f64 = num;
	packager_commit(packager);

	return 1;
}
/* }}} */

int yar_pack_push_string(yar_packager *packager, char *str, uint len) /* {{{ */ {
	yar_data *slot;
	char *buf = malloc(len + 1);

	if (!buf) {
		return 0;
	}
	if (len) {
		memcpy(buf, str, len);
	}
	buf[len] = '\0';

	slot = packager_next_slot(packager);
	if (!slot) {
		free(buf);
		return 0;
	}

	slot->type = YAR_DATA_STRING;
	slot->via.str.ptr = buf;
	slot->via.str.size = len;
	packager_commit(packager);

	return 1;
}
/* }}} */

int yar_pack_push_data(yar_packager *packager, const yar_data *data) /* {{{ */ {
	yar_data *slot;

	if (!data) {
		return 0;
	}

	slot = packager_next_slot(packager);
	if (!slot) {
		return 0;
	}

	if (!yar_data_dup_contents(data, slot)) {
		return 0;
	}

	packager_commit(packager);

	return 1;
}
/* }}} */

int yar_pack_push_packager(yar_packager *packager, yar_packager *data) /* {{{ */ {
	if (!data || !data->root) {
		return 0;
	}

	return yar_pack_push_data(packager, data->root);
}
/* }}} */

int yar_pack_encode(yar_packager *packager, yar_payload *payload, yar_packager_type type) /* {{{ */ {
	if (!packager || !packager->root || !payload) {
		return 0;
	}

	return yar_data_pack(packager->root, payload, type);
}
/* }}} */

int yar_pack_to_string(yar_packager *packager, yar_payload *payload) /* {{{ */ {
	return yar_pack_encode(packager, payload, YAR_PACKAGER_MSGPACK);
}
/* }}} */

yar_data * yar_pack_take_root(yar_packager *packager) /* {{{ */ {
	yar_data *root = packager->root;
	packager->root = NULL;
	return root;
}
/* }}} */

void yar_pack_free(yar_packager *packager) /* {{{ */ {
	if (!packager) {
		return;
	}
	yar_data_free(packager->root);
	free(packager->stack);
	free(packager);
}
/* }}} */

/* }}} */

/* unpackager {{{ */

void yar_unpack_free(yar_unpackager *unpk) /* {{{ */ {
	yar_data_free(unpk->root);
	free(unpk);
}
/* }}} */

yar_unpackager * yar_unpack_init(char *data, uint len, yar_packager_type type) /* {{{ */ {
	yar_unpackager *unpk;
	yar_data *root = yar_data_unpack(data, len, type);

	if (!root) {
		return NULL;
	}

	unpk = malloc(sizeof(yar_unpackager));
	if (!unpk) {
		yar_data_free(root);
		return NULL;
	}

	unpk->root = root;
	return unpk;
}
/* }}} */

const yar_data * yar_unpack_unpack(yar_unpackager *unpk) /* {{{ */ {
	return unpk->root;
}
/* }}} */

/* }}} */

/* accessors {{{ */

yar_data_type yar_unpack_data_type(const yar_data *data, uint *size) /* {{{ */ {
	switch (data->type) {
		case YAR_DATA_STRING:
			*size = data->via.str.size;
			break;
		case YAR_DATA_ARRAY:
			*size = data->via.array.size;
			break;
		case YAR_DATA_MAP:
			*size = data->via.map.size;
			break;
		default:
			break;
	}

	return data->type;
}
/* }}} */

int yar_unpack_data_value(const yar_data *data, void *arg) /* {{{ */ {
	switch (data->type) {
		case YAR_DATA_NULL:
			*(void **)arg = NULL;
			return YAR_DATA_NULL;
		case YAR_DATA_BOOL:
			*(int *)arg = data->via.boolean;
			return YAR_DATA_BOOL;
		case YAR_DATA_LONG:
			*(long *)arg = data->via.i64;
			return YAR_DATA_LONG;
		case YAR_DATA_ULONG:
			*(ulong *)arg = data->via.u64;
			return YAR_DATA_ULONG;
		case YAR_DATA_DOUBLE:
			*(double *)arg = data->via.f64;
			return YAR_DATA_DOUBLE;
		case YAR_DATA_STRING:
			*(const char **)arg = data->via.str.ptr;
			return YAR_DATA_STRING;
		case YAR_DATA_ARRAY:
			*(const yar_data **)arg = data->via.array.ptr;
			return YAR_DATA_ARRAY;
		case YAR_DATA_MAP:
			*(const yar_data **)arg = data->via.map.ptr;
			return YAR_DATA_MAP;
	}

	return 0;
}
/* }}} */

int yar_unpack_data_null(const yar_data *data, int *val) /* {{{ */ {
	uint size;

	if (yar_unpack_data_type(data, &size) != YAR_DATA_NULL) {
		return 0;
	}
	if (val) {
		*val = 0;
	}
	return YAR_DATA_NULL;
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

/* }}} */

/* iterator {{{ */

yar_unpack_iterator * yar_unpack_iterator_init(const yar_data *data) /* {{{ */ {
	yar_unpack_iterator *it = NULL;

	switch (data->type) {
		case YAR_DATA_ARRAY:
			it = malloc(sizeof(yar_unpack_iterator));
			if (!it) {
				return NULL;
			}
			it->position = 0;
			it->size = data->via.array.size;
			it->data = (yar_data *)data->via.array.ptr;
			break;
		case YAR_DATA_MAP:
			it = malloc(sizeof(yar_unpack_iterator));
			if (!it) {
				return NULL;
			}
			it->position = 0;
			it->size = data->via.map.size * 2; /* kv */
			it->data = (yar_data *)data->via.map.ptr;
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
	return &it->data[it->position];
}
/* }}} */

void yar_unpack_iterator_free(yar_unpack_iterator *it) /* {{{ */ {
	free(it);
}
/* }}} */

/* }}} */

/* debug {{{ */

static void yar_debug_print_data_inner(const yar_data *data, FILE *fp) /* {{{ */ {
	uint i;

	switch (data->type) {
		case YAR_DATA_NULL:
			fputs("NULL", fp);
			break;
		case YAR_DATA_BOOL:
			fputs(data->via.boolean? "true" : "false", fp);
			break;
		case YAR_DATA_LONG:
			fprintf(fp, "%ld", data->via.i64);
			break;
		case YAR_DATA_ULONG:
			fprintf(fp, "%lu", data->via.u64);
			break;
		case YAR_DATA_DOUBLE:
			fprintf(fp, "%g", data->via.f64);
			break;
		case YAR_DATA_STRING:
			fprintf(fp, "\"%.*s\"", (int)data->via.str.size, data->via.str.ptr);
			break;
		case YAR_DATA_ARRAY:
			fputc('[', fp);
			for (i = 0; i < data->via.array.size; i++) {
				if (i) {
					fputs(", ", fp);
				}
				yar_debug_print_data_inner(&data->via.array.ptr[i], fp);
			}
			fputc(']', fp);
			break;
		case YAR_DATA_MAP:
			fputc('{', fp);
			for (i = 0; i < data->via.map.size; i++) {
				if (i) {
					fputs(", ", fp);
				}
				yar_debug_print_data_inner(&data->via.map.ptr[i * 2], fp);
				fputs(" => ", fp);
				yar_debug_print_data_inner(&data->via.map.ptr[i * 2 + 1], fp);
			}
			fputc('}', fp);
			break;
		default:
			fputs("UNKNOWN", fp);
			break;
	}
}
/* }}} */

void yar_debug_print_data(const yar_data *data, FILE *fp) /* {{{ */ {
	if (!fp) {
		fp = stdout;
	}
	if (!data) {
		fputs("NULL", fp);
		return;
	}
	yar_debug_print_data_inner(data, fp);
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
