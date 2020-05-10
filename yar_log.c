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

#include <stdarg.h> 	/* for va_list */
#include <stdio.h>   	/* for fprintf */
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>

#include "yar_common.h"
#include "yar_log.h"

typedef struct _yar_logger {
	FILE *fp;
	int pipe;
	int enable;
	int level;
	const char *hostname;
} yar_logger;

static yar_logger *logger;

int yar_logger_init(const char *path, int level) /* {{{ */ {
	yar_logger *lg = calloc(1, sizeof(yar_logger));
	lg->enable = 1;
	if (path) {
		if (path[0] != '|') {
			lg->fp = fopen(path, "a");
		} else {
			lg->pipe = 1;
			lg->fp = popen(path + 1, "w");
		}
		if (!lg->fp) {
			free(lg);
			alog(YAR_ERROR, "Failed to start log '%s'", strerror(errno));
			return 0;
		}
	}
	lg->level = level;
	logger = lg;
	return 1;
}
/* }}} */

int yar_logger_setopt(yar_logger_opt opt, void *value) /* {{{ */ {
	if (!logger) {
		return 0;
	}

	switch (opt) {
		case YAR_LOGGER_HOSTNAME:
			logger->hostname = (const char *)value;
			break;
		default:
			break;
	}
	return 1;
}
/* }}} */

void yar_logger_destroy() /* {{{ */ {
	if (logger->fp) {
		if (logger->pipe) {
			pclose(logger->fp);
		} else {
			fclose(logger->fp);
		}
	} else {
	}
	free(logger);
	logger = NULL;
}
/* }}} */

void yar_log_ex(int type, const char *fmt, ...) /* {{{ */ {
	char buf[1024];
	va_list args;
	char *stype, *stv;
	time_t tv;
	uint len;

	if (logger && (!logger->enable || (type !=  YAR_OKEY && type < logger->level))) {
		return;
	}

	switch (type) {
		case YAR_OKEY:
			stype = "OKEY";
			break;
		case YAR_NOTICE:
			stype = "NOTICE";
			break;
		case YAR_DEBUG:
			stype = "DEBUG";
			break;
		case YAR_WARNING:
			stype = "WARN";
			break;
		case YAR_ERROR:
			stype = "ERROR";
			break;
		default:
			stype = "";
			break;
	}

	va_start(args, fmt);
	len = vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	if (len >= sizeof(buf)) {
		memcpy(buf + sizeof(buf) - sizeof("..."), "...", sizeof("...") - 1);
		len = sizeof(buf) - 1;
	}
	buf[len] = '\0';

	tv = time(NULL);
	stv = ctime(&tv);
	if (logger && logger->fp) {
		fprintf(logger->fp, "%s [%.*s] %s: %s\n",
				logger->hostname? logger->hostname : "-", (int)(strlen(stv) - 1), stv, stype, buf);
		fflush(logger->fp);
	} else {
		fprintf(stderr, "%s [%.*s] %s: %s\n", 
			logger && logger->hostname? logger->hostname : "-", (int)(strlen(stv) - 1), stv, stype, buf);
	}

	return;
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
