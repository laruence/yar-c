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

#ifndef YAR_RESPONSE_H
#define YAR_RESPONSE_H

typedef struct _yar_response {
	long id;
	int  status;
	char *error;
	uint elen;
	yar_data *in;
	struct _yar_payload payload;  /* do not manipulate following elements */
	void *out;
	void *buffer;
} yar_response;

void yar_response_set_retval(yar_response *response, yar_packager *packager);
void yar_response_set_error(yar_response *response, int code, const char *fmt, ...);
const yar_data * yar_response_get_response(yar_response *response);
int yar_response_get_status(yar_response *response);
int yar_response_get_error(yar_response *response, const char **msg, uint *len);

int yar_response_pack(yar_response *response, struct _yar_payload *payload, int extra_bytes);
int yar_response_unpack(yar_response *response, char *payload, uint len, int extra_bytes);
void yar_response_free(yar_response *response);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
