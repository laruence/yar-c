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
#include <arpa/inet.h> /* for htonl */

#include "yar_common.h"
#include "yar_protocol.h"

char YAR_PACKAGER[8] = "MSGPACK";

void yar_protocol_render(yar_header *header, uint id, char *provider, char *token, int body_len, uint reserved) /* {{{ */ {
	header->magic_num = htonl(YAR_PROTOCOL_MAGIC_NUM);
	header->id = htonl(id);
	header->body_len = htonl(body_len);
	header->reserved = htonl(reserved);
	if (provider) {
		memcpy(header->provider, provider, 16);
	}
	if (token) {
		memcpy(header->token, token, 16);
	}
	return;
} /* }}} */

int yar_protocol_parse(yar_header *header) /* {{{ */ {
	header->magic_num = ntohl(header->magic_num);

	if (header->magic_num != YAR_PROTOCOL_MAGIC_NUM) {
		return 0;
	}

	header->id = ntohl(header->id);
	header->body_len = ntohl(header->body_len);
	header->reserved = ntohl(header->reserved);

	return 1;
} /* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
