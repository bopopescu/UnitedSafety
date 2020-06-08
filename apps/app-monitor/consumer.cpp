#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include "ats-common.h"
#include "socket_interface.h"
#include "command_line_parser.h"

class MyData : public ats::CommonData
{
public:
};

static void* consumer_server(void* p)
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

		printf("%s,%d:%s: Got message: %s\n", __FILE__, __LINE__, __FUNCTION__, cmdline.c_str());

		cmdline.clear();
	}
	return 0;
}

int main(int argc, char* argv[])
{
	MyData md;
	md.set("app-name", "consumer");
	md.set("user", "applet");
	md.set_from_args(argc - 1, argv + 1);

	ServerData server_data;
	{
		ServerData& sd = server_data;
		init_ServerData(&sd, 16);
		sd.m_hook = &md;
		sd.m_cs = consumer_server;
		const ats::String& user = md.get("user");
		set_unix_domain_socket_user_group(&sd, user.c_str(), user.c_str());
		start_redstone_ud_server(&sd, md.get("app-name").c_str(), 1);
	}

	signal_app_unix_socket_ready("consumer", "consumer");
	signal_app_ready("consumer");

	ats::infinite_sleep();
	return 0;
}
