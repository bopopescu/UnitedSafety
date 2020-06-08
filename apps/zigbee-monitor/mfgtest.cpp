#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

#include "ats-common.h"
#include "socket_interface.h"
#include "atslogger.h"

ATSLogger g_log;
static ClientSocket g_cs;

static const ats::String recv()
{
	char buf[8192];
	const ssize_t nread = recv(g_cs.m_fd, buf, sizeof(buf), 0);

	if(nread <= 0)
	{
		ats_logf(ATSLOG_ERROR, "%s,%d: Failed to recv from socket", __FILE__, __LINE__);
		return ats::String();
	}

	buf[sizeof(buf) - 1] = '\0';

	return buf;
}

static int write(const ats::String& cmd)
{

	const ssize_t nwrite = send(g_cs.m_fd, cmd.c_str(), cmd.size(), MSG_NOSIGNAL);

	if(nwrite <= 0)
	{
		fprintf(stderr, "Failed to write to socket\n");
		ats_logf(ATSLOG_ERROR, "%s,%d: Failed to write to socket", __FILE__, __LINE__);
	}

	return nwrite;
}


int main(int argc, char* argv[])
{
	g_log.set_global_logger(&g_log);
	g_log.set_level(0);
	g_log.open_testdata("zigbee-mfg-test");

	ats_logf(ATSLOG_ERROR, "zigbee-mfg-test start");

	if(argc < 2)
	{
		fprintf(stderr, "usage: %s linkkey\n", argv[0]);
		ats_logf(ATSLOG_ERROR, "usage: %s linkkey",argv[0]);
		return 1;
	}

	const ats::String& linkkey = argv[1];

	if(linkkey.size() != 32)
	{
		fprintf(stderr, "Link key should be 32 characters\n");
		ats_logf(ATSLOG_ERROR, "%s,%d: Link key should be 32 characters, exit...", __FILE__, __LINE__);
		return 1;
	}


	init_ClientSocket(&g_cs);
	if(connect_redstone_ud_client(&g_cs, "zigbee-monitor"))
	{
		fprintf(stderr, "Fail to connect zigbee-monitor unix domain\n");
		ats_logf(ATSLOG_ERROR, "%s,%d: Fail to connect zigbee-monitor unix domain, exit...", __FILE__, __LINE__);
		return 1;
	}

	ats::String response;
	ats::String cmd = "linkkey key=" + linkkey + "\n\r";

	write(cmd);
	response = recv();
	if(response.find("OK") == ats::String::npos)
	{
		fprintf(stderr, "Fail to set linkkey\n");
		ats_logf(ATSLOG_ERROR, "%s,%d: Fail to set linkkey, exit...", __FILE__, __LINE__);
		close_ClientSocket(&g_cs);
		return 1;
	}

	write(ats::String("routemode\n\r"));
	response = recv();
	if(response.find("OK") == ats::String::npos)
	{
		fprintf(stderr, "Fail to enter route mode\n");
		ats_logf(ATSLOG_ERROR, "%s,%d: Fail to enter route mode, exit...", __FILE__, __LINE__);
		close_ClientSocket(&g_cs);
		return 1;
	}

	write("ucast pid=\"0000\" msg=\"E0100009\"\n\r");
	response = recv();
	if(response.find("OK") == ats::String::npos)
	{
		fprintf(stderr, "Fail to send ucast command\n");
		ats_logf(ATSLOG_ERROR, "%s,%d: Fail to send ucast command, exit...", __FILE__, __LINE__);
		close_ClientSocket(&g_cs);
		return 1;
	}

	ats_logf(ATSLOG_ERROR, "exited on success");
	close_ClientSocket(&g_cs);
	return 0;
}
