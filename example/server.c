
/**
 * async.c
 *
 * @author  Laruence
 * @date    2012-10-16 17:47
 * @version $Id$
 */

#include <stdio.h>   	/* for fprintf */
#include <string.h>
#include <getopt.h> 	/* for getopt */
#include <assert.h>     /* for assert */
#include <stdlib.h>

#include "yar.h"
#include "msgpack.h" 

void yar_handler_example(yar_request *request, yar_response *response, void *cookie) {
	yar_packager *packager;
	const yar_data *parameters = yar_request_get_parameters(request);
	assert(((long)cookie) == 1);

	packager = yar_pack_start_map(3);
	/* first element */
	{
		/* key */
		yar_pack_push_string(packager, "status", 6);
		/* value */
		yar_pack_push_long(packager, 0);
	}

	/* second element */
	{
		/* key */
		yar_pack_push_string(packager, "parameters", 10);
		/* value */
		yar_pack_push_data(packager, parameters);
	}

	/* third element */ 
	{
		/* key */
		yar_pack_push_string(packager, "data", 4);
		/* value */
		yar_pack_push_array(packager, 3);
		/* array start */
		{
			/* first element */
			yar_pack_push_bool(packager, 1);
			/* second element */
			yar_pack_push_double(packager, 0.2342);
			/* third element */
			yar_pack_push_string(packager, "dummy", 5);
		}
	}
	yar_response_set_retval(response, packager);
	yar_pack_free(packager);
}

yar_server_handler example_handlers[] = {
	{"default", sizeof("default") - 1, yar_handler_example},
	{NULL, 0, NULL}
};

int main(int argc, char **argv) {
	int opt, max_childs = 5;
	char *hostname = NULL, *yar_pid = NULL, *log_file = NULL;
	char *user = NULL, *group = NULL;
	int standalone = 0;

	while ((opt = getopt(argc, argv, "hS:n:K:p:Xl:u:g:")) != -1) {
		switch (opt) {
			case 'n':
				max_childs = atoi(optarg);
				break;
			case 'S':
				hostname = optarg;
				break;
			case 'p':
				yar_pid = optarg;
				break;
			case 'X':
				standalone = 1;
				break;
			case 'l':
				log_file = optarg;
				break;
			case 'u':
				user = optarg;
				break;
			case 'g':
				group = optarg;
				break;
			default:
				yar_server_print_usage(argv[0]);
				return 0;
		}
	}

	if (!hostname) {
		yar_server_print_usage(argv[0]);
		return 0;
	}	

	if (yar_server_init(hostname)) {
		yar_server_set_opt(YAR_STAND_ALONE, &standalone);
		yar_server_set_opt(YAR_MAX_CHILDREN, &max_childs);
		yar_server_set_opt(YAR_PID_FILE, yar_pid);
		yar_server_set_opt(YAR_LOG_FILE, log_file);
		yar_server_set_opt(YAR_CUSTOM_DATA, (void *)1);
		yar_server_set_opt(YAR_CHILD_USER, user);
		yar_server_set_opt(YAR_CHILD_GROUP, group);
		yar_server_register_handler(example_handlers);
		yar_server_run();
	}
	return 0;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
