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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>   	/* for fprintf */
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h> /* for sockets */
#include <sys/un.h>  	/* for un */
#include <netdb.h>  	/* for gethostbyname */

#include "yar_common.h"
#include "yar_pack.h"
#include "yar_log.h"
#include "yar_protocol.h"
#include "yar_response.h"
#include "yar_request.h"
#include "yar_client.h"

static void yar_client_hangup(yar_client *client) /* {{{ */ {
	if (client->fd > 0) {
		close(client->fd);
	}
	client->fd = 0;
}
/* }}} */

void yar_client_destroy(yar_client *client) /* {{{ */ {
	yar_client_hangup(client);
	free(client);
}
/* }}} */

/* wait for the fd to become readable (for_read = 1) or writable (for_read = 0),
   the fd_set and timeout are re-armed on every call, returns the select() status */
static int yar_client_wait(int fd, int for_read, int timeout) /* {{{ */ {
	fd_set fds;
	struct timeval tv;

	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	tv.tv_sec = timeout;
	tv.tv_usec = 0;

	if (for_read) {
		return select(fd + 1, &fds, NULL, NULL, &tv);
	} else {
		return select(fd + 1, NULL, &fds, NULL, &tv);
	}
}
/* }}} */

static yar_response * yar_client_caller(yar_client *client, char *method, uint num_args, yar_packager *parameters[]) /* {{{ */ {
	int bytes_sent, bytes_read, select_result;
	uint bytes_left, offset, total_read, header_read;
	uint timeout;
	unsigned int request_id = 1000; /* dummy id */
	char header_buf[sizeof(yar_header)];
	yar_header *response_header;
	yar_response *response = NULL;
	yar_request  *request;
	yar_header header = {0};
	yar_payload payload = {0};

	if (client->fd <= 0) {
		alog(YAR_ERROR, "Client is not connected");
		return NULL;
	}

	timeout = client->timeout? client->timeout : 1; /* default 1 second */

	request = calloc(1, sizeof(yar_request));
	request->id = request_id;
	request->method = strdup(method);
	request->mlen = strlen(method);
	if (num_args) {
		uint i;
		yar_packager *packager = yar_pack_start_array(num_args);
		for (i = 0; i < num_args; i++) {
			yar_pack_push_packager(packager, parameters[i]);
		}
		yar_request_set_parameters(request, packager);
		yar_pack_free(packager);
	}

	if (!yar_request_pack(request, &payload, sizeof(yar_header) + sizeof(YAR_PACKAGER), (yar_packager_type)client->packager)) {
		alog(YAR_ERROR, "Packing request failed");
		yar_request_free(request);
		free(request);
		return NULL;
	}

	yar_protocol_render(&header, request_id, YAR_CLIENT_NAME, NULL, payload.size - sizeof(yar_header), client->persistent? YAR_PROTOCOL_PERSISTENT : 0);

	memcpy(payload.data, (char *)&header, sizeof(yar_header));
	memcpy(payload.data + sizeof(yar_header), client->packager == YAR_PACKAGER_JSON? YAR_PACKAGER_JSON_TAG : YAR_PACKAGER, sizeof(YAR_PACKAGER));
	yar_request_free(request);
	free(request);

	offset = 0;
	bytes_left = payload.size;
	while (bytes_left) {
		if ((select_result = yar_client_wait(client->fd, 0, timeout)) == 0) {
			alog(YAR_ERROR, "Send request timeout");
			goto error;
		} else if (select_result == -1) {
			if (errno == EINTR) {
				continue;
			}
			alog(YAR_ERROR, "Select for client failed '%s'", strerror(errno));
			goto error;
		}

		do {
			bytes_sent = send(client->fd, payload.data + offset, bytes_left, 0);
		} while (bytes_sent == -1 && errno == EINTR);

		if (bytes_sent == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				continue;
			}
			alog(YAR_ERROR, "Send request failed '%s'", strerror(errno));
			goto error;
		}

		offset += bytes_sent;
		bytes_left -= bytes_sent;
	}

	free(payload.data);
	payload.data = NULL;

	response = calloc(1, sizeof(yar_response));

	/* read the response header, it may arrive in several segments */
	header_read = 0;
	while (header_read < sizeof(yar_header)) {
		if ((select_result = yar_client_wait(client->fd, 1, timeout)) == 0) {
			alog(YAR_ERROR, "Read response timeout");
			goto error;
		} else if (select_result == -1) {
			if (errno == EINTR) {
				continue;
			}
			alog(YAR_ERROR, "Select for client failed '%s'", strerror(errno));
			goto error;
		}

		do {
			bytes_read = recv(client->fd, header_buf + header_read, sizeof(yar_header) - header_read, 0);
		} while (bytes_read == -1 && errno == EINTR);

		if (bytes_read == 0) {
			alog(YAR_ERROR, "Server closed connection prematurely");
			goto error;
		} else if (bytes_read == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				continue;
			}
			alog(YAR_ERROR, "Failed read response '%s'", strerror(errno));
			goto error;
		}

		header_read += bytes_read;
	}

	response_header = (yar_header *)header_buf;
	if (!yar_protocol_parse(response_header)) {
		alog(YAR_ERROR, "Parsing response header failed, maybe not responsed by a rpc server?");
		goto error;
	}

	if (response_header->body_len > YAR_MAX_BODY_SIZE) {
		alog(YAR_ERROR, "Response body too large %u", response_header->body_len);
		goto error;
	}

	response->payload.data = malloc(sizeof(yar_header) + response_header->body_len);
	response->payload.size = sizeof(yar_header) + response_header->body_len;
	memcpy(response->payload.data, header_buf, sizeof(yar_header));
	total_read = sizeof(yar_header);

	/* read the response body */
	while (total_read < response->payload.size) {
		if ((select_result = yar_client_wait(client->fd, 1, timeout)) == 0) {
			alog(YAR_ERROR, "Read response timeout");
			goto error;
		} else if (select_result == -1) {
			if (errno == EINTR) {
				continue;
			}
			alog(YAR_ERROR, "Select for client failed '%s'", strerror(errno));
			goto error;
		}

		do {
			bytes_read = recv(client->fd, response->payload.data + total_read, response->payload.size - total_read, 0);
		} while (bytes_read == -1 && errno == EINTR);

		if (bytes_read == 0) {
			alog(YAR_ERROR, "Lost connection to server");
			goto error;
		} else if (bytes_read == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				continue;
			}
			alog(YAR_ERROR, "Failed read response '%s'", strerror(errno));
			goto error;
		}

		total_read += bytes_read;
	}

	{
		char *tag = response->payload.data + sizeof(yar_header);
		if (client->packager == YAR_PACKAGER_JSON) {
			if (strncmp(tag, YAR_PACKAGER_JSON_TAG, sizeof(YAR_PACKAGER_JSON_TAG) - 1) != 0) {
				alog(YAR_ERROR, "Response packager is not JSON");
				goto error;
			}
		} else if (strncmp(tag, YAR_PACKAGER, sizeof(YAR_PACKAGER) - 1) != 0) {
			alog(YAR_ERROR, "Response packager is not msgpack");
			goto error;
		}
	}

	if (!yar_response_unpack(response, response->payload.data, response->payload.size, sizeof(yar_header) + sizeof(YAR_PACKAGER), (yar_packager_type)client->packager)) {
		alog(YAR_ERROR, "Unpack response failed");
		goto error;
	}

	if (response->id != (long)request_id) {
		alog(YAR_ERROR, "Response id mismatch, expect %u, got %ld", request_id, response->id);
		goto error;
	}

	return response;

error:
	if (payload.data) {
		free(payload.data);
	}
	if (response) {
		yar_response_free(response);
		free(response);
	}
	/* the stream can not be trusted anymore, do not let further calls reuse it */
	yar_client_hangup(client);
	return NULL;
}
/* }}} */

yar_client * yar_client_init(char *hostname) /* {{{ */ {
	struct sockaddr_storage sa;
	socklen_t sa_len = 0;
	bzero(&sa, sizeof(sa));
	int sockfd;
	yar_client *client = calloc(1, sizeof(yar_client));

	/* accept the PHP-style tcp:// scheme so one URI works for both clients */
	if (strncasecmp(hostname, "tcp://", sizeof("tcp://") - 1) == 0) {
		hostname += sizeof("tcp://") - 1;
	}

	client->hostname = hostname;
	client->call = yar_client_caller;

	if (strncasecmp(hostname, "http://", sizeof("http://")) == 0
			|| strncasecmp(hostname, "https://", sizeof("https://")) == 0) {
		/* libcurl */
		free(client);
		return NULL;
	} else if (hostname[0] == '/') {
		/* unix domain socket */
		struct sockaddr_un *usa;
		usa = (struct sockaddr_un *)&sa;
		if (strlen(hostname) >= sizeof(usa->sun_path)) {
			alog(YAR_ERROR, "Unix socket path too long '%s'", hostname);
			free(client);
			return NULL;
		}
		if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
			alog(YAR_ERROR, "Failed to create a socket '%s'", strerror(errno));
			free(client);
			return NULL;
		}
		usa->sun_family = AF_UNIX;
		memcpy(usa->sun_path, hostname, strlen(hostname) + 1);
		sa_len = SUN_LEN(usa);
	} else {
		/* IPV4, we don't support IPV6 for now */
		char *delim, *p, host[1024];
		int port, addrtype;
		struct hostent *hptr;
		struct sockaddr_in *isa;
		if ((delim = strchr(hostname, ':'))) {
			if ((size_t)(delim - hostname) >= sizeof(host)) {
				alog(YAR_ERROR, "Host name too long");
				free(client);
				return NULL;
			}
			memcpy(host, hostname, delim - hostname);
			host[delim - hostname] = '\0';
			port = atoi(delim + 1);
		} else {
			alog(YAR_ERROR, "Port doesn't specificed");
			free(client);
			return NULL;
		}

		if ((hptr = gethostbyname(host)) == NULL) {
			alog(YAR_ERROR, "Failed to resolve host name '%s'", host);
			free(client);
			return NULL;
		}

		{
			/* see yar_copy_unaligned(): the hostent members are misaligned on
			 * some platforms, copy them out byte-wise instead of loading them */
			char **addr_list;
			char *addr;
			yar_copy_unaligned(&addrtype, &hptr->h_addrtype, sizeof(addrtype));
			yar_copy_unaligned(&addr_list, &hptr->h_addr_list, sizeof(addr_list));
			yar_copy_unaligned(&addr, addr_list, sizeof(addr));
			p = addr;
		}
		if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
			alog(YAR_ERROR, "Failed to create a socket '%s'", strerror(errno));
			free(client);
			return NULL;
		}

		switch (addrtype) {
			case AF_INET:
				{
					int val = 1;
					isa = (struct sockaddr_in *)&sa;
					bzero(isa, sizeof(struct sockaddr_in));

					isa->sin_family = AF_INET;
					isa->sin_port = htons(port);
					memcpy(&isa->sin_addr, p, sizeof(struct in_addr));

					setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&val, sizeof(val));
					sa_len = sizeof(struct sockaddr_in);
				}
				break;
			default:
				alog(YAR_ERROR, "Unknown address type %d", addrtype);
				close(sockfd);
				free(client);
				return NULL;
		}
	}

	if (connect(sockfd, (const struct sockaddr *)&sa, sa_len) == -1) {
		alog(YAR_ERROR, "Failed to connect to host '%s'", strerror(errno));
		close(sockfd);
		free(client);
		return NULL;
	}

	yar_set_non_blocking(sockfd);

	client->fd = sockfd;

	return client;
}
/* }}} */

int yar_client_set_opt(yar_client *client, yar_client_opt opt, void *val) /* {{{ */ {
	switch (opt) {
		case YAR_PERSISTENT_LINK:
			client->persistent = *(int *)val;
		break;
		case YAR_CONNECT_TIMEOUT:
			client->timeout = *(int *)val;
		break;
		case YAR_OPT_PACKAGER:
			client->packager = *(int *)val;
		break;
		default:
			return 0;
	}
	return 1;
}
/* }}} */

const void * yar_client_get_opt(yar_client *client, yar_client_opt opt) /* {{{ */ {
	switch (opt) {
		case YAR_PERSISTENT_LINK:
			return &client->persistent;
		break;
		case YAR_CONNECT_TIMEOUT:
			return &client->timeout;
		break;
		case YAR_OPT_PACKAGER:
			return &client->packager;
		break;
		default:
			return NULL;
	}
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
