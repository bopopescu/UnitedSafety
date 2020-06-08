#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>

#include "socket_interface.h"
#include "ats-common.h"

static void* my_udp_server(void* p)
{
	struct ServerData& sd = *((struct ServerData*)p);

	for(;;)
	{
		char buf[1024];
		struct sockaddr_in src_addr;
		socklen_t addrlen = sizeof(src_addr);
		const ssize_t nread = recvfrom(sd.m_udp->m_sockfd, buf, sizeof(buf), 0, (struct sockaddr*)&src_addr, &addrlen);

		if(!nread)
		{
			fprintf(stderr, "%s,%d: Orderly shutdown\n", __FILE__, __LINE__);
			break;
		}

		if(nread < 0)
		{
			fprintf(stderr, "%s,%d: (%d) %s\n", __FILE__, __LINE__, errno, strerror(errno));
			break;
		}

		printf("[FROM %s (%zd byte%s): ", inet_ntoa(src_addr.sin_addr), nread, ((1 == nread) ? ""  : "s"));

		for(ssize_t i = 0; i < nread; ++i)
		{
			printf("%c", buf[i]);
		}

		printf("]\n");
	}

	return 0;
}

int main(int argc, char *argv[])
{

	if(argc < 2)
	{
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		return 1;
	}

	ServerData sd;
	init_ServerData(&sd, 1);
	sd.m_port = strtol(argv[1], 0, 0);
	sd.m_cs = my_udp_server;
	start_udp_server(&sd);

	ats::infinite_sleep();

	return 0;
}
