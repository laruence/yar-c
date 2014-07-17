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

#ifndef YAR_PACK_H
#define YAR_PACK_H

typedef enum _yar_data_type {
	YAR_DATA_NULL = 1,
	YAR_DATA_BOOL,
	YAR_DATA_LONG,
	YAR_DATA_ULONG,
	YAR_DATA_DOUBLE,
	YAR_DATA_STRING,
	YAR_DATA_MAP,
	YAR_DATA_ARRAY
} yar_data_type;

typedef struct _yar_packager yar_packager;
typedef struct _yar_unpackager yar_unpackager;
typedef struct _yar_unpack_iterator yar_unpack_iterator;
typedef struct _yar_data yar_data;

#define yar_pack_start_null() yar_pack_start(YAR_DATA_NULL, 0)
#define yar_pack_start_bool() yar_pack_start(YAR_DATA_BOOL, 0)
#define yar_pack_start_long() yar_pack_start(YAR_DATA_LONG, 0)
#define yar_pack_start_ulong() yar_pack_start(YAR_DATA_ULONG, 0)
#define yar_pack_start_double() yar_pack_start(YAR_DATA_DOUBLE, 0)
#define yar_pack_start_string() yar_pack_start(YAR_DATA_STRING, 0)
#define yar_pack_start_map(size) yar_pack_start(YAR_DATA_MAP, size)
#define yar_pack_start_array(size) yar_pack_start(YAR_DATA_ARRAY, size)

/* serialization */
yar_packager * yar_pack_start(yar_data_type type, uint size);
int yar_pack_push_array(yar_packager *packager, uint size);
int yar_pack_push_map(yar_packager *packager, uint size);
int yar_pack_push_null(yar_packager *packager);
int yar_pack_push_bool(yar_packager *packager, int val);
int yar_pack_push_long(yar_packager *packager, long num);
int yar_pack_push_ulong(yar_packager *packager, ulong num);
int yar_pack_push_double(yar_packager *packager, double num);
int yar_pack_push_string(yar_packager *packager, char *str, uint len);
int yar_pack_push_data(yar_packager *packager, const yar_data *data);
int yar_pack_push_packager(yar_packager *packager, yar_packager *data);
int yar_pack_to_string(yar_packager *packager, yar_payload *payload);
void yar_pack_free(yar_packager *packager);

/* deserialization */
void yar_unpack_free(yar_unpackager *unpk);
yar_unpackager * yar_unpack_init(char *data, uint len);
const yar_data * yar_unpack_unpack(yar_unpackager *unpk);

yar_data_type yar_unpack_data_type(const yar_data *data, uint *size);
int yar_unpack_data_value(const yar_data *data, void *arg);
int yar_unpack_data_null(const yar_data *data, int *val);
int yar_unpack_data_bool(const yar_data *data, int *bval);
int yar_unpack_data_long(const yar_data *data, long *num);
int yar_unpack_data_ulong(const yar_data *data, ulong *num);
int yar_unpack_data_string(const yar_data *data, const char **str);
int yar_unpack_data_array(const yar_data *data, const yar_data **arg);
int yar_unpack_data_map(const yar_data *data, const yar_data **arg);

yar_unpack_iterator * yar_unpack_iterator_init(const yar_data *data);
void yar_unpack_iterator_reset(yar_unpack_iterator *it);
int yar_unpack_iterator_next(yar_unpack_iterator *it);
const yar_data *yar_unpack_iterator_current(yar_unpack_iterator *it);
void yar_unpack_iterator_free(yar_unpack_iterator *it);

void yar_debug_print_data(const yar_data *data, FILE *fp);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
