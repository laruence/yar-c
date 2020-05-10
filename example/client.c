
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
#include <stdlib.h>
#include <sys/types.h>

#include "yar.h"
#include "msgpack.h"

void output_response(yar_response *response) {
	if (!yar_response_get_status(response)) {
		uint size;
		const yar_data *data = yar_response_get_response(response);
		switch (yar_unpack_data_type(data, &size)) {
			case YAR_DATA_STRING:
				{
					const char *str;
					yar_unpack_data_string(data, &str);
					fprintf(stdout, "[OKEY]: %.*s\n", size, str);
					fflush(stdout);
				}
				break;
			default:
				fprintf(stdout, "[OKEY]:");
				msgpack_object_print(stdout, *(msgpack_object *)data);
				fprintf(stdout, "\n");
				fflush(stdout);
				break;
		}
	} else {
		const char *msg;
		uint len;
		yar_response_get_error(response, &msg, &len);
		fprintf(stderr, "[ERROR]: %.*s\n", len, msg);
		fflush(stderr);
	}
}

int main(int argc, char **argv) {
	int persistent = 1;
	int opt;
	char *hostname = NULL;
	int standalone = 0;
	yar_client *client;
	int number_calls = 10;

	while ((opt = getopt(argc, argv, "h:n:")) != -1) {
		switch (opt) {
			case 'h':
				hostname = optarg;
			break;
			case 'n':
				number_calls = atoi(optarg);
			break;
			default:
			printf( "Usage: %s -h <host>:<port> -n <number calls> \n", argv[0]);
			break;
		}
	}
	if (!hostname) {
		printf( "Usage: %s -h <host>:<port> \n", argv[0]);
		return 0;
	}
	client = yar_client_init(hostname);
	if (client) {
		int i = 0;
		yar_client_set_opt(client, YAR_PERSISTENT_LINK, &persistent);
		while (i++ < number_calls) {
			yar_response *response = client->call(client, "default", 0, NULL);
			if (response) {
				output_response(response);
				yar_response_free(response);
				free(response);
			} else {
				printf( "No response\n" );
				return 1;
			}
		}
		yar_client_destroy(client);
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
