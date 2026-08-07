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

#ifndef YAR_COMMON_H
#define YAR_COMMON_H

#include <fcntl.h> 		/* for fcntl */
#include <sys/types.h>

/* BSD/macOS <sys/types.h> provides `uint`, glibc does not */
#if defined(__linux__) && !defined(uint)
typedef unsigned int uint;
#endif

#define YAR_OKEY    0x0
#define YAR_DEBUG   0x1
#define YAR_NOTICE 	0x2
#define YAR_WARNING 0x4
#define YAR_ERROR   0x8

#define YAR_VERSION "0.0.1"

typedef struct _yar_payload {
	char *data;
	uint size;
} yar_payload;

static inline int yar_set_non_blocking(int fd) {
	int flags = fcntl(fd, F_GETFL);
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
		return 0;
	}
	return 1;
}

/* On some platforms (macOS) gethostbyname() returns a packed static buffer
 * where even the hostent members (and the address entries) are misaligned
 * for their own types. Loading them directly is UB, and memcpy() is not a
 * safe replacement either because the compiler may lower a constant-sized
 * memcpy() back into aligned loads, so copy byte-by-byte through volatile,
 * which the compiler is not allowed to merge or widen. */
static inline void yar_copy_unaligned(void *dst, const void *src, uint n) {
	const volatile unsigned char *s = (const volatile unsigned char *)src;
	unsigned char *d = (unsigned char *)dst;
	while (n-- > 0) {
		*d++ = *s++;
	}
}

#endif
/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
