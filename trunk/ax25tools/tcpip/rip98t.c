#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include "rip98d.h"

static int build_header(unsigned char *message)
{
	message[0] = RIPCMD_RESPONSE;
	message[1] = RIP_VERSION_98;

	message[2] = 0;
	message[3] = 0;

	return RIP98_HEADER;
}


void transmit_routes(int s)
{
	unsigned char message[500];
	struct route_struct *route;
	struct sockaddr_in rem_addr;
	int mess_len;
	int size;
	int i;

	size = sizeof(struct sockaddr_in);

	for (i = 0; i < dest_count; i++) {

		if (debug && logging)
			syslog(LOG_DEBUG, "transmit_routes: sending routing message to %s\n", inet_ntoa(dest_list[i].dest_addr));

		memset((char *)&rem_addr, 0, sizeof(rem_addr));
		rem_addr.sin_family = AF_INET;
		rem_addr.sin_addr   = dest_list[i].dest_addr;
		rem_addr.sin_port   = htons(RIP_PORT);		

		route = first_route;

		while (route != NULL) {

			mess_len = build_header(message);

			while (mess_len < 184 && route != NULL) {
				if (route->action != DEL_ROUTE) {
					memcpy(message + mess_len + 0, (char *)&route->addr, sizeof(struct in_addr));
					message[mess_len + 4] = route->bits;
					message[mess_len + 5] = route->metric;
				
					mess_len += RIP98_ENTRY;
				}

				route = route->next;
			}

			if (mess_len > RIP98_HEADER) {
				if (sendto(s, message, mess_len, 0, (struct sockaddr *)&rem_addr, size) < 0) {
					if (logging)
						syslog(LOG_ERR, "sendto: %m");
				}
			}
		}
	}
}
