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

#ifndef YAR_SERVER_H
#define YAR_SERVER_H

#define YAR_SERVER_NAME "Yar C Server "YAR_VERSION

typedef enum _yar_server_opt {
	YAR_STAND_ALONE,
	YAR_READ_TIMEOUT,
	YAR_PARENT_INIT,
	YAR_CHILD_INIT,
	YAR_CHILD_USER,
	YAR_CHILD_GROUP,
	YAR_MAX_CHILDREN,
	YAR_CUSTOM_DATA,
	YAR_PID_FILE,
	YAR_LOG_FILE,
	YAR_LOG_LEVEL
} yar_server_opt;

typedef struct _yar_server yar_server; 

typedef void (*yar_init) (void *data);
typedef void (*yar_handler) (yar_request *request, yar_response *response, void *data);

typedef struct _yar_server_handler {
	char *name;
	int len;
	yar_handler handler;
} yar_server_handler;

void yar_server_print_usage(char *argv0);
int yar_server_init(char *hostname);
int yar_server_set_opt(yar_server_opt opt, void *val);
const void * yar_server_get_opt(yar_server_opt opt);
int yar_server_register_handler(yar_server_handler *handlers);
void yar_server_shutdown();
void yar_server_destroy();
int yar_server_run();

#endif
/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
