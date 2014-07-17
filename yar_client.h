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

#ifndef YAR_CLIENT_H
#define YAR_CLIENT_H

#define YAR_CLIENT_NAME "Yar(C)-"YAR_VERSION

typedef struct _yar_client yar_client;

typedef yar_response * (*yar_client_call)(yar_client *client, char *method, uint num_args, yar_packager *packager[]);

struct _yar_client {
	int fd;
	char *hostname;
	int persistent;
	yar_client_call call;
};

typedef enum _yar_client_opt {
	YAR_PERSISTENT_LINK = 1,
	YAR_CONNECT_TIMEOUT
} yar_client_opt;

yar_client * yar_client_init(char *hostname);
int yar_client_set_opt(yar_client *client, yar_client_opt opt, void *val);
const void * yar_client_get_opt(yar_client *client, yar_client_opt opt);

void yar_client_destroy(yar_client *client);
#endif
/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
