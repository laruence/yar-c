
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
	yar_client *client = yar_client_init("127.0.0.1:8888");
	if (client) {
		int i = 0;
		yar_client_set_opt(client, YAR_PERSISTENT_LINK, &persistent);
		while (i++ < 10) {
			yar_response *response = client->call(client, "default", 0, NULL);
			if (response) {
				output_response(response);
				yar_response_free(response);
				free(response);
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
