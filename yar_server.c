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

#include <stdarg.h> 	/* for va_list */
#include <stdio.h>   	/* for fprintf */
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>  	/* for fork & setsid */
#include <time.h>  		/* for ctime */
#include <sys/types.h>
#include <sys/stat.h> 	/* for umask */
#include <sys/socket.h> /* for sockets */
#include <sys/un.h>  	/* for un */
#include <sys/wait.h>   /* for waitpid */
#include <sys/time.h>   /* for gettimeofday */
#include <netdb.h>  	/* for gethostbyname */
#include <pwd.h>        /* for getpwnam */
#include <grp.h>        /* for getgrnam */
#include <arpa/inet.h> 	/* for inet_ntop */

#include "event.h" 		/* for libevent */

#include "yar_common.h"
#include "yar_log.h"
#include "yar_pack.h"
#include "yar_protocol.h"
#include "yar_response.h"
#include "yar_request.h"
#include "yar_server.h"

typedef struct _yar_request_context {
	size_t bytes_sent;
	struct event ev_read;
	struct event ev_write;
	struct _yar_server *server;
	struct _yar_response *response;
	struct _yar_request *request;
	struct _yar_header *header;
	struct timeval timeout;
	ulong start_time;
	char *remote_addr;
	long remote_port;
} yar_request_context;

struct _yar_server {
	char *hostname;
	int fd;
	int ppid;
	int max_children;
	int stand_alone;
	int running_children;
	int running;
	int timeout;
	char *user;
	char *group;
	int uid;
	int gid;
	char *pid_file;
	char *log_file;
	int  log_level;
	void *data;
	yar_server_handler *handlers;
	yar_init parent_init;
	yar_init child_init;
} *server;

ulong inline yar_get_microsec() /* {{{ */ {
	struct timeval tv;
	if (gettimeofday(&tv, (struct timezone *)NULL) == 0) {
		return (tv.tv_sec * 1000000) + tv.tv_usec;
	} else {
		return 0;
	}
}
/* }}} */

static inline void yar_server_log(yar_request_context *ctx) /* {{{ */ {
	yar_response *response = ctx->response;
	yar_request *request = ctx->request;

	if (response->status) {
		/* request id#remote addr:port#api name#error#provider#bytes sent */
		alog(YAR_ERROR, "%ld %s:%ld \"%.*s\" %.*s \"%s\" %u -", response->id, ctx->remote_addr,
				ctx->remote_port, request->mlen, request->method, response->elen, response->error,
				ctx->header->provider[0]? (const char *)ctx->header->provider : "-", ctx->bytes_sent);
	} else {
		ulong current_t = yar_get_microsec();
		/* request id#remote addr:port#api name#provider#bytes sent#time used */
		alog(YAR_OKEY, "%ld %s:%ld \"%.*s\" \"%s\" %u %lu", response->id, ctx->remote_addr, ctx->remote_port,
				request->mlen, request->method, ctx->header->provider[0]? (const char *)ctx->header->provider : "-",
				ctx->bytes_sent, current_t - ctx->start_time);
	}
}
/* }}} */

static inline void yar_server_log_error(yar_request_context *ctx, const char *fmt, ...) /* {{{ */ {
	char buf[1024];
	va_list args;
	uint len;

	va_start(args, fmt);
	len = vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	if (len >= sizeof(buf)) {
		memcpy(buf + sizeof(buf) - sizeof("..."), "...", sizeof("...") - 1);
		len = sizeof(buf) - 1;
	}

	alog(YAR_ERROR, "%ld %s:%ld\t%s %ld", ctx->request? ctx->request->id : 0, ctx->remote_addr, ctx->remote_port, buf, 0);
}
/* }}} */

static int yar_server_start_daemon(void) /* {{{ */ {
	pid_t pid;
	int i, fd;

	pid = fork();
	switch (pid) {
		case -1:
			return 0;
		case 0:
			setsid();
			break;
		default:
			exit(0);
			break;
	}

	umask(0);
	chdir("/");
	for (i=0; i < 3; i++) {
		close(i);
	}

	fd = open("/dev/null", O_RDWR);
	if (dup2(fd, 0) < 0 || dup2(fd, 1) < 0 || dup2(fd, 2) < 0) {
		close(fd);
		return 0;
	}

	close(fd);
	return 1;
}
/* }}} */

static int yar_check_previous_run(char *pfile) /* {{{ */ {
	FILE *fp;
	if (access(pfile, F_OK) == 0) {
		alog(YAR_ERROR, "There is already a yar_sever run, pid '%s'", pfile);
		return 0;
	}
	fp = fopen(pfile, "w+");
	if (!fp) {
		alog(YAR_ERROR, "Failed to write pid file '%s'", strerror(errno));
		return 0;
	}
	fprintf(fp, "%d", getpid());
	fclose(fp);
	return 1;
} /* }}} */

static int yar_server_start_listening() /* {{{ */ {
	struct sockaddr sa;
	char addrstr[INET_ADDRSTRLEN];
	int port = 0, sockfd = 0;

	char *hostname = server->hostname;

	if (strncasecmp(hostname, "http://", sizeof("http://")) == 0 
			|| strncasecmp(hostname, "https://", sizeof("https://")) == 0) {
		alog(YAR_ERROR, "Http server doesn't support yet");
		return 0;
	} else if (hostname[0] == '/') {
		struct sockaddr_un *usa;
		usa = (struct sockaddr_un *)&sa;
		unlink(hostname);
		sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
		usa->sun_family = AF_UNIX;
		memcpy(usa->sun_path, hostname, strlen(hostname) + 1);
	} else {
		char *delim, *p, host[512];
		struct hostent *hptr;
		struct sockaddr_in *isa;
		if ((delim = strchr(hostname, ':'))) {
			if ((delim - hostname) >= sizeof(host)) {
				alog(YAR_ERROR, "Host name too long");
				return 0;
			}
			memcpy(host, hostname, delim - hostname);
			host[delim - hostname] = '\0';
			port = atoi(delim + 1);
		} else {
			alog(YAR_ERROR, "Port doesn't specificed");
			return 0;
		}

		if ((hptr = gethostbyname(host)) == NULL) {
			alog(YAR_ERROR, "Failed to resolve host name '%s'", strerror(errno));
			return 0;
		}

		p = hptr->h_addr;
		if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
			alog(YAR_ERROR, "Failed to create a socket '%s'", strerror(errno));
			return 0;
		}
		switch (hptr->h_addrtype) {
			case AF_INET:
				{
					int val = 1;
					isa = (struct sockaddr_in *)&sa;
					bzero(isa, sizeof(struct sockaddr_in));

					isa->sin_family = AF_INET;
					isa->sin_port = htons(port);
					memcpy(&isa->sin_addr, p, sizeof(struct in_addr));

					setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&val, sizeof(val));
				}
				break;
			default:
				alog(YAR_ERROR, "Unknown address type %d", hptr->h_addrtype);
				close(sockfd);
				return 0;
		}

		inet_ntop(isa->sin_family, p, addrstr, sizeof(addrstr));
	}

	if (bind(sockfd, (const struct sockaddr*)&sa, sizeof(struct sockaddr)) == -1) {
		alog(YAR_ERROR, "Failed to bind socket '%s'", strerror(errno));
		if (errno != EINVAL && errno != EADDRINUSE) {
			close(sockfd);
		}
		return 0;
	}

	if (listen(sockfd, SOMAXCONN)) {
		alog(YAR_DEBUG, "Failed start listening '%s'", strerror(errno));
		close(sockfd);
		return 0;
	}

	if (!yar_set_non_blocking(sockfd)) {
		alog(YAR_ERROR, "Failed to set non-blocking to server socket fd '%s'", strerror(errno));
		close(sockfd);
		return 0;
	}

	server->fd = sockfd;

	if (port) {
		alog(YAR_DEBUG, "Start listening at %s:%d", addrstr, port);
	} else {
		alog(YAR_DEBUG, "Start listening at %s", hostname);
	}

	return 1;
}
/* }}} */

/* {{{ static void yar_server_sig_child(int signo) */
#if 0
static void yar_server_sig_child(int signo) {
	pid_t pid;
	int stat;

	while ((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
		alog(YAR_DEBUG, "Child %d exit with status %d", pid, stat);
		server->running_children--;
	}
	
	return;
}
#endif
/* }}} */

static void yar_server_sig_handler(int signo) /* {{{ */ {
	yar_server_shutdown(signo);
	return;
}
/* }}} */

static void yar_server_parent_init() /* {{{ */ {
	struct sigaction act;

	act.sa_handler = yar_server_sig_handler;
	sigemptyset(&act.sa_mask); 

#ifdef SA_INTERRUPT
	act.sa_flags = SA_INTERRUPT;
#else
	act.sa_flags = 0;
#endif

	signal(SIGPIPE, SIG_IGN);
	sigaction(SIGTERM, &act, NULL);
	sigaction(SIGINT, &act, NULL);
	sigaction(SIGQUIT, &act, NULL);

	server->ppid = getpid();
	if (server->parent_init) {
		server->parent_init(server->data);
	}
}
/* }}} */

static void yar_server_child_init() /* {{{ */ {

	/* install signal handler */
	signal(SIGPIPE, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);
	signal(SIGTERM, yar_server_sig_handler);
	signal(SIGINT, yar_server_sig_handler);
	signal(SIGQUIT, yar_server_sig_handler);

	/* setuid & set gid */
	if (server->gid) {
		if (setgid(server->gid) < 0) {
			alog(YAR_NOTICE, "Failed to setgid to %d '%s'", server->gid, strerror(errno));
		}
	}

	if (server->uid) {
		if (setuid(server->uid) < 0) {
			alog(YAR_NOTICE, "Failed to setuid to %d '%s'", server->uid, strerror(errno));
		}
	}

	if (server->child_init) {
		server->child_init(server->data);
	}
}
/* }}} */

static int yar_server_startup_workers() /* {{{ */ {
	int max_childs;
	pid_t pid = 0;

	max_childs = server->max_children;
	if (server->stand_alone || !max_childs) {
		yar_server_parent_init();
		return 1;
	} else {
		while (max_childs-- && (pid = fork())) {
			server->running_children++;
		}
		if (pid) {
			yar_server_parent_init();
			return 0;
		} else {
			yar_server_child_init();
			return 1;
		}
	}
}
/* }}} */

static inline yar_server_handler * yar_server_find_handler(char *name, int len) /* {{{ */ {
	int i = 0;
	yar_server_handler *handlers;
	if (!server->handlers) {
		return NULL;
	}
	handlers = server->handlers;

	while (handlers[i].name) {
		if (handlers[i].len == len && strncmp(name, handlers[i].name, len) == 0) {
			return &(handlers[i]);
		}
		i++;
	}

	return NULL;
}
/* }}} */

static void yar_server_reset(yar_request_context *ctx) /* {{{ */ {
	ctx->header = NULL;
	ctx->bytes_sent = 0;
	ctx->start_time = yar_get_microsec();
	yar_request_free(ctx->request);
	yar_response_free(ctx->response);
	memset(ctx->request, 0, sizeof(yar_request));
	memset(ctx->response, 0, sizeof(yar_response));
	event_del(&ctx->ev_write);
	event_add(&ctx->ev_read, &ctx->timeout);
}
/* }}} */

static void yar_server_close_connection(int fd, yar_request_context *ctx) /* {{{ */ {
	close(fd);
	event_del(&ctx->ev_read);
	event_del(&ctx->ev_write);
	yar_request_free(ctx->request);
	yar_response_free(ctx->response);
	free(ctx);
}
/* }}} */

static void yar_server_on_write(int fd, short ev, void *arg) /* {{{ */ {
	yar_request_context *ctx = (yar_request_context *)arg;
	yar_response *response = ctx->response;

	if (ev == EV_TIMEOUT) {
		yar_server_close_connection(fd, ctx);
		return;
	} else {
		int bytes_sent;
		do {
			if ((bytes_sent = send(fd, response->payload.data + ctx->bytes_sent, response->payload.size - ctx->bytes_sent, 0)) > 0) {
				ctx->bytes_sent += bytes_sent;
			}
		} while (bytes_sent == -1 && errno == EINTR);

		if (ctx->bytes_sent < response->payload.size) {
			return;
		}
	}

	yar_server_log(ctx);
	if (ctx->header->reserved & YAR_PROTOCOL_PERSISTENT) {
		yar_server_reset(ctx);
	} else {
		yar_server_close_connection(fd, ctx);
	}
}
/* }}} */

static void yar_server_on_read(int fd, short ev, void *arg) /* {{{ */ {
	yar_request_context *ctx = (yar_request_context *)arg;
	yar_request *request = ctx->request;

	if (ev == EV_TIMEOUT) {
		yar_server_close_connection(fd, ctx);
		return;
	} else {
		int read_bytes;
		if (!ctx->header) {
			char buf[sizeof(yar_header)];
			ctx->start_time = yar_get_microsec();
			do {
				read_bytes = recv(fd, buf, sizeof(buf), 0);
			} while (read_bytes == -1 && errno == EINTR);

			if (read_bytes == 0) {
				yar_server_close_connection(fd, ctx);
				return;
			} else if (read_bytes < 0) {
				yar_server_log_error(ctx, "Failed read request '%s'", strerror(errno));
				yar_server_close_connection(fd, ctx);
				return;
			}
		
			if (!yar_protocol_parse((yar_header *)buf)) {
				yar_server_log_error(ctx, "Failed to parse request header, maybe not sent by a Yar client");
				return;
			}

			request->size = ((yar_header*)buf)->body_len + sizeof(yar_header);
			request->body = malloc(request->size);
			memcpy(request->body, buf, read_bytes);
			request->blen = read_bytes;
			ctx->header = (yar_header *)request->body;
			if (request->blen < request->size) {
				return;
			}
		}

		if (request->blen < request->size) {
			do {
				read_bytes = recv(fd, request->body + request->blen, request->size - request->blen, 0);
			} while (read_bytes == -1 && errno == EINTR);
			if (read_bytes == 0) {
				yar_server_close_connection(fd, ctx);
				return;
			} else if (read_bytes < 0) {
				yar_server_log_error(ctx, "Failed read request '%s'", strerror(errno));
				yar_server_close_connection(fd, ctx);
				return;
			}
			request->blen += read_bytes;
		}

		if (request->blen < request->size) {
			/* there are more data to read */
			return;
		} else {
			yar_server_handler *handler;
			yar_header header = {0};
			yar_response *response = ctx->response;

			if (strncmp(request->body + sizeof(yar_header), YAR_PACKAGER, sizeof(YAR_PACKAGER) - 1) != 0) {
				yar_response_set_error(response, YAR_ERROR, "package protocol %.*s is not supported, only msgpack does",
					   	sizeof(YAR_PACKAGER) - 1, request->body + sizeof(yar_header));
			} else if (!yar_request_unpack(request, request->body, request->blen, sizeof(yar_header) + sizeof(YAR_PACKAGER))) {
				yar_response_set_error(response, YAR_ERROR, "%s", "request header verify failed");
			} else {
				response->id = request->id;
				handler = yar_server_find_handler(request->method, request->mlen);
				if (!handler) {
					yar_response_set_error(response, YAR_ERROR, "call to undefined method '%.*s'", request->mlen, request->method);
				} else {
					handler->handler(request, response, server->data);
				}
			}
			if (!yar_response_pack(response, &response->payload, sizeof(yar_header) + sizeof(YAR_PACKAGER))) {
				yar_server_log_error(ctx, "Failed to pack response");
				yar_server_close_connection(fd, ctx);
				return;
			}
			yar_protocol_render(&header, request->id, YAR_SERVER_NAME, NULL, response->payload.size - sizeof(yar_header), 0);
			memcpy(response->payload.data, (char *)&header, sizeof(yar_header));
			memcpy(response->payload.data + sizeof(yar_header), YAR_PACKAGER, sizeof(YAR_PACKAGER));
			event_set(&ctx->ev_write, fd, EV_WRITE|EV_PERSIST, yar_server_on_write, ctx);
			event_add(&ctx->ev_write, &ctx->timeout);
			event_del(&ctx->ev_read);
			return;
		}
	}
}
/* }}} */

static void yar_server_on_accept(int fd, short ev, void *arg) /* {{{ */ {
	int client_fd;
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(struct sockaddr_in);
	yar_request_context *ctx;

	client_fd = accept(fd, (struct sockaddr *)&client_addr, &client_len);

	if (client_fd == -1) {
		return;
	}

	ctx = calloc(1, sizeof(yar_request_context) + sizeof(yar_request) + sizeof(yar_response));
	if (!yar_set_non_blocking(fd)) {
		alog(YAR_WARNING, "Setting non-block mode failed '%s'", strerror(errno));
		free(ctx);
		close(client_fd);
		return;
	}

	if (getpeername(client_fd, (struct sockaddr *)&client_addr, &client_len) == 0) {
		ctx->remote_addr = inet_ntoa(((struct sockaddr_in *)&client_addr)->sin_addr);
		ctx->remote_port = ntohs(((struct sockaddr_in *)&client_addr)->sin_port);
	}

	ctx->request = (yar_request *)((char *)ctx + sizeof(yar_request_context));
	ctx->response = (yar_response *)((char *)ctx->request + sizeof(yar_request));
	ctx->timeout.tv_sec = server->timeout;
	ctx->timeout.tv_usec = 0;
	event_set(&ctx->ev_read, client_fd, EV_READ|EV_PERSIST, yar_server_on_read, ctx);
	event_add(&ctx->ev_read, &ctx->timeout);

	return;
}
/* }}} */

void yar_server_print_usage(char *argv0) /* {{{ */ { 
	char * prog = strrchr(argv0, '/');
	if (prog) {
		prog++;
	} else {
		prog = "yar";
	}

	printf( "Usage: %s -S <host>:<port> -n <workers number>\n"
			"\n"
			"    -S <host>:<port>  Run yar at host:port\n"
			"    -n <workers num>  How many wokers\n"
			"    -K <restart|stop> \n"
			"    -h                help\n",
			prog );
	return;
}
/* }}} */

int yar_server_init(char *hostname) /* {{{ */ {
	yar_server *instance;
	if (server) {
		alog(YAR_WARNING, "Server already be initialized");
		return 0;
	}

	instance = calloc(1, sizeof(yar_server));
	instance->hostname = hostname;
	instance->timeout = 3;
	server = instance;

	return 1;
}
/* }}} */

int yar_server_set_opt(yar_server_opt opt, void * val) /* {{{ */ {
	switch (opt) {
		case YAR_STAND_ALONE:
			server->stand_alone = *(int *)val;
			break;
		case YAR_MAX_CHILDREN:
			if (*(int *)val < 0 || *(int *)val > 128) {
				alog(YAR_WARNING, "Number of workers must between 0 ~ 128", opt);
				return 0;
			}
			server->max_children = *(int *)val;
			break;
		case YAR_READ_TIMEOUT:
			server->timeout = *(int *)val;
			break;
		case YAR_PARENT_INIT:
			server->parent_init = (yar_init)val;
			break;
		case YAR_CHILD_INIT:
			server->child_init = (yar_init)val;
			break;
		case YAR_CUSTOM_DATA:
			server->data = val;
			break;
		case YAR_PID_FILE:
			server->pid_file = (char *)val;
			break;
		case YAR_LOG_FILE:
			server->log_file = (char *)val;
			break;
		case YAR_LOG_LEVEL:
			server->log_level = *(int *)val;
			break;
		case YAR_CHILD_USER:
			{
				struct passwd *pwd;
				char *user = (char *)val;
				if (!user) {
					return 1;
				}
				if (!(pwd = getpwnam(user))) {
					alog(YAR_WARNING, "Can not getpwnam for user %s '%s'", user, errno? strerror(errno) : "user not found");
					return 0;
				}
				server->user = user;
				server->uid = pwd->pw_uid;
				server->gid = pwd->pw_gid;
			}
			break;
		case YAR_CHILD_GROUP:
			{
				struct group *grp;
				char *grpname = (char *)val;
				if (!grpname) {
					return 1;
				}
				if (!(grp = getgrnam(grpname))) {
					alog(YAR_WARNING, "Can not getgrpnam for group %s '%s'", grpname, errno? strerror(errno) : "group not found");
					return 0;
				}
				server->group = grpname;
				server->gid = grp->gr_gid;
			}
			break;
		default:
			alog(YAR_WARNING, "Unrecognized opt %d", opt);
			return 0;
	}
	return 1;
}
/*}}} */

const void * yar_server_get_opt(yar_server_opt opt) /* {{{ */ {
	switch (opt) {
		case YAR_STAND_ALONE:
			return &server->stand_alone;
		case YAR_MAX_CHILDREN:
			return &server->max_children;
		case YAR_READ_TIMEOUT:
			return &server->timeout;
		case YAR_PARENT_INIT:
			return &server->parent_init;
		case YAR_CHILD_INIT:
			return &server->child_init;
		case YAR_CUSTOM_DATA:
			return &server->data;
		case YAR_PID_FILE:
			return &server->pid_file;
		case YAR_LOG_FILE:
			return &server->log_file;
		case YAR_CHILD_USER:
			return &server->user;
		case YAR_CHILD_GROUP:
			return &server->group;
		case YAR_LOG_LEVEL:
			return &server->log_level;
		default:
			alog(YAR_WARNING, "Unrecognized opt %d", opt);
			return NULL;
	}
}
/*}}} */

int yar_server_register_handler(yar_server_handler *handlers) /* {{{ */ {
	server->handlers = handlers;
	return 1;
} 
/* }}} */

void yar_server_shutdown(signo) /* {{{ */ {
	(void)signo;

	server->running = 0;
	if (server->ppid != getpid() || server->stand_alone || !server->max_children) {
		event_loopexit(NULL);
	}
}
/* }}} */

void yar_server_destroy() /* {{{ */ {
	if (server->fd) {
		close(server->fd);
	}
	if (server->pid_file && server->ppid == getpid()) {
		unlink(server->pid_file);
	}
	yar_logger_destroy();
	free(server);
	server = NULL;
}
/* }}} */

int yar_server_run() /* {{{ */ {

	if (!yar_logger_init(server->log_file, server->log_level)) {
		return 0;
	}

	yar_logger_setopt(YAR_LOGGER_HOSTNAME, server->hostname);

    if (server->pid_file && !yar_check_previous_run(server->pid_file)) {
		return 0;
	}

	if (!yar_server_start_listening(server)) {
		alog(YAR_ERROR, "Failed to setup server at %s", server->hostname);
		yar_server_destroy(server);
		return 0;
	}

	alog(YAR_DEBUG, "Attempt to start %d wokers", (server->stand_alone || !server->max_children)? 1 : server->max_children);
	if (!server->stand_alone && !yar_server_start_daemon()) {
		alog(YAR_ERROR, "Failed to setup daemon");
		return 0;
	}

	server->running = 1;
	if (!yar_server_startup_workers(server)) {
		/* master */
		pid_t cid;
		int stat;

		while (server->running) {
			if ((cid = waitpid(-1, &stat, 0)) > 0) {
				alog(YAR_DEBUG, "Child %d exit with status %d", cid, stat);
				if (!(cid = fork())) {
					alog(YAR_DEBUG, "Startup new worker, now running worker is %d, max worker is %d", server->running_children, server->max_children);
					yar_server_child_init();
					goto worker;
				}
			} 
		}

		alog(YAR_DEBUG, "Server is going down");
		signal(SIGQUIT, SIG_IGN);
		kill(-(server->ppid), SIGQUIT);

		while(server->running_children) {
			while ((cid = waitpid(-1, &stat, 0)) > 0) {
				server->running_children--;
				alog(YAR_DEBUG, "Child %d shutdown with status %d", cid, stat);
			}
		}

		yar_server_destroy(server);
	} else {
		/* slavers */
		struct event ev_accept;
worker:
		event_init();
		while (server->running) {
			/* we can not run more than one server anyway */
			event_set(&ev_accept, server->fd, EV_READ|EV_PERSIST, yar_server_on_accept, NULL);
			event_add(&ev_accept, NULL);
			event_dispatch();
		}
		/* server has been shutdown */
		yar_server_destroy(server);
	}
	return 1;
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
