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

#ifndef YAR_REQUEST_H
#define YAR_REQUEST_H

typedef struct _yar_request {
	ulong id;
	char *method;
	uint  mlen;
	void  *out;
	void  *buffer;
	yar_data *in;
	size_t  size;
	size_t  blen;
	char *body;
} yar_request;

int yar_request_pack(yar_request *request, struct _yar_payload *payload, int extra_bytes);
int yar_request_unpack(yar_request *request, char *payload, uint len, int extra_bytes);
void yar_request_set_parameters(yar_request *request, yar_packager *packager);
const yar_data * yar_request_get_parameters(yar_request *request);
void yar_request_free(yar_request *request);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
