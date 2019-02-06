/*
 * Copyright (C) 2018 Intel Corporation
 *
 * Intel Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions: The
 * above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
#include <stdio.h>
#include <wayland-util.h>
#include <libdrm/intel_bufmgr.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "../shared/config-parser.h"
#include "../shared/helpers.h"

#include "transport_plugin.h"

struct udpSocket {
	int sockDesc;
	struct sockaddr_in sockAddr;
};


struct private_data {
	int verbose;
	struct udpSocket socket;
	char *ipaddr;
	unsigned short port;
};

WL_EXPORT int init(int *argc, char **argv, void **plugin_private_data, int verbose)
{
	printf("Using UDP remote display transport plugin...\n");
	struct private_data *private_data = calloc(1, sizeof(*private_data));
	*plugin_private_data = (void *)private_data;
	if (private_data) {
		private_data->verbose = verbose;
	} else {
		return(-ENOMEM);
	}

	int port = 0;
	const struct weston_option options[] = {
		{ WESTON_OPTION_STRING,  "ipaddr", 0, &private_data->ipaddr},
		{ WESTON_OPTION_INTEGER, "port", 0, &port},
	};
	parse_options(options, ARRAY_LENGTH(options), argc, argv);
	private_data->port = port;

	if ((private_data->ipaddr != NULL) && (private_data->ipaddr[0] != 0)) {
		printf("Sending to %s:%d.\n", private_data->ipaddr, port);
	} else {
		fprintf(stderr, "Invalid network configuration.\n");
		free(private_data);
		*plugin_private_data = NULL;
		return -1;
	}

	private_data->socket.sockDesc = socket(AF_INET, SOCK_DGRAM, 0);
	if (private_data->socket.sockDesc < 0) {
		fprintf(stderr, "Socket creation failed.\n");
		free(private_data);
		*plugin_private_data = NULL;
		return -1;
	}

	private_data->socket.sockAddr.sin_addr.s_addr = inet_addr(private_data->ipaddr);
	private_data->socket.sockAddr.sin_family = AF_INET;
	private_data->socket.sockAddr.sin_port = htons(private_data->port);

	return 0;
}


WL_EXPORT void help(void)
{
	printf("\tThe udp plugin uses the following parameters:\n");
	printf("\t--ipaddr=<ip_address>\t\tIP address of receiver.\n");
	printf("\t--port=<port_number>\t\tPort to use on receiver.\n");
	printf("\n\tThe receiver should be started using:\n");
	printf("\t\"gst-launch-1.0 udpsrc port=<port_number>"
			"! h264parse ! mfxdecode live-mode=true ! mfxsinkelement\"\n");
}


WL_EXPORT int send_frame(void *plugin_private_data, drm_intel_bo *drm_bo,
		int32_t stream_size, uint32_t timestamp)
{
	uint8_t *bufdata = (uint8_t *)(drm_bo->virtual);
	struct private_data *private_data = (struct private_data *)plugin_private_data;

	if (private_data == NULL) {
		fprintf(stderr, "Private data is null!\n");
		return -1;
	}

	if (private_data->verbose) {
		printf("Sending frame over UDP...\n");
	}

	int rval = sendto(private_data->socket.sockDesc, bufdata, stream_size, 0,
			(struct sockaddr *) &private_data->socket.sockAddr, sizeof(private_data->socket.sockAddr));

	if (rval <= 0) {
		fprintf(stderr, "Send failed.\n");
	}

	return 0;
}

WL_EXPORT void destroy(void **plugin_private_data)
{
	struct private_data *private_data = (struct private_data *)*plugin_private_data;

	if (private_data == NULL) {
		return;
	}

	if (private_data->verbose) {
		fprintf(stdout, "Closing network connection...\n");
	}
	close(private_data->socket.sockDesc);
	private_data->socket.sockDesc = -1;
	memset(&private_data->socket.sockAddr, 0, sizeof(private_data->socket.sockAddr));

	if (private_data->verbose) {
		fprintf(stdout, "Freeing plugin private data...\n");
	}
	free(private_data);
	*plugin_private_data = NULL;
}
