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

#ifndef YAR_JSON_H
#define YAR_JSON_H

#include "yar_pack.h"

/* JSON codec over the yar_data value tree (requires cJSON at build time;
 * without it both entry points fail).
 *
 * Known limitations (in line with the PHP yar JSON packager):
 * - strings are UTF-8/ASCII text, binary strings with embedded NUL bytes can
 *   not round-trip through JSON
 * - numbers are limited to what an IEEE double can represent exactly
 */
int yar_json_encode(const yar_data *data, yar_payload *out);
yar_data * yar_json_decode(const char *data, uint len);

#endif
/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
