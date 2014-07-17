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

#ifndef YAR_PROTOCOL_H
#define YAR_PROTOCOL_H

#define YAR_PROTOCOL_PERSISTENT	0x1
#define YAR_PROTOCOL_PING		0x2
#define YAR_PROTOCOL_LIST		0x4

#define YAR_PROTOCOL_MAGIC_NUM  0x80DFEC60
typedef struct _yar_header {
    unsigned int   id;
    unsigned short version;
    unsigned int   magic_num;
    unsigned int   reserved;
    unsigned char  provider[32];
    unsigned char  token[32];
    unsigned int   body_len; 
} __attribute__ ((packed)) yar_header;

extern char YAR_PACKAGER[8];


void yar_protocol_render(yar_header *header, uint id, char *provider, char *token, int body_len, uint reserved);
int yar_protocol_parse(yar_header *header);
#endif
/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
