#include <iostream>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>

#include "ats-common.h"
#include "socket_interface.h"
#include "command_line_parser.h"

void* do_nothing_server(void* p)
{
	ClientData* cd = (ClientData*)p;

	bool command_too_long = false;
	ats::String cmdline;

	const size_t max_command_length = 1024;

	ClientDataCache cache;
	init_ClientDataCache(&cache);

	for(;;)
	{
		char ebuf[1024];
		const int c = client_getc_cached(cd, ebuf, sizeof(ebuf), &cache);

		if(c < 0)
		{
			if(c != -ENODATA) syslog(LOG_ERR, "%s,%d:ERROR: %s", __FILE__, __LINE__, ebuf);
			break;
		}

		if(c != '\r' && c != '\n')
		{
			if(cmdline.length() >= max_command_length) command_too_long = true;
			else cmdline += c;

			continue;
		}

		if(command_too_long)
		{
			cmdline.clear();
			command_too_long = false;
			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "error: command is too long\n\r");
			continue;
		}

		CommandBuffer cb;
		init_CommandBuffer(&cb);
		const char* err;

		if((err = gen_arg_list(cmdline.c_str(), cmdline.length(), &cb)))
		{
			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "error: gen_arg_list failed: %s\n\r", err);

		}
		else if(cb.m_argc > 0)
		{
			printf("Got command: ");
			int i;

			for(i = 0; i < cb.m_argc; ++i)
			{
				printf("%s%s", i > 0 ? " " : "", cb.m_argv[i]);
			}

			printf("\n");
			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "%s: OK\n\r", cb.m_argv[0]);
		}

		cmdline.clear();
	}

	return 0;
}

int main(int p_argc, char* p_argv[])
{

	if(p_argc < 4)
	{
		fprintf(stderr, "Usage: %s <host> <port> <serve-on-port>\n", p_argv[0]);
		return 1;
	}

	ClientSocket cs;
	init_ClientSocket(&cs);
	const int ret = connect_client(&cs, p_argv[1], strtol(p_argv[2], 0, 0));

	if(ret)
	{
		fprintf(stderr, "%s,%d: (%d) %s\n", __FILE__, __LINE__, errno, strerror(errno));
	}
	else
	{
		printf("Connection OK! (there is a server at %s:%d)\n", p_argv[1], int(strtol(p_argv[2],0,0)));
	}

	{
		ServerData sd;
		init_ServerData(&sd, 64);
        	sd.m_port = strtol(p_argv[3], 0, 0);
	        sd.m_cs = do_nothing_server;
		const int ret = start_server(&sd);

		if(ret)
		{
			fprintf(stderr, "%s,%d: Could not create server on port %d (something is blocking the port)\n", __FILE__, __LINE__,
				sd.m_port);
			return 1;
		}

		printf("Server port (%d) created OK!\n", sd.m_port);
		ats::infinite_sleep();
	}

	return 0;
}
