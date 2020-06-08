#include <iostream>

#include "ats-common.h"
#include "atslogger.h"
#include "packetizer.h"
#include "packetizerCamsStateMachine.h"
#include "packetizerCamsCopy.h"
#include "command_line_parser.h"

ATSLogger g_log;
MyData g_md;
ats::String g_app_name("gemalto-sim");

SList g_msg;
sem_t g_msg_sem;

static void* client_command_server(void* p)
{
	ClientData* cd = (ClientData*)p;
	MyData& md = *((MyData*)(cd->m_data->m_hook));

	bool command_too_long = false;

	const size_t max_command_length = 1024;
	char cmdline_buf[max_command_length + 1];
	char* cmdline = cmdline_buf;

	ClientDataCache cdc;
	init_ClientDataCache(&cdc);

	CommandBuffer cb;
	init_CommandBuffer(&cb);

	for(;;)
	{
		char ebuf[256];
		const int c = client_getc_cached(cd, ebuf, sizeof(ebuf), &cdc);

		if(c < 0)
		{
			if(c != -ENODATA) ats_logf(ATSLOG(3), "%s,%d: %s", __FILE__, __LINE__, ebuf);
			break;
		}

		if((c != '\r') && (c != '\n'))
		{
			if(size_t(cmdline - cmdline_buf) >= max_command_length) command_too_long = true;
			else *(cmdline++) = c;

			continue;
		}

		if(command_too_long)
		{
			ats_logf(ATSLOG(0), "%s,%d: command is too long", __FILE__, __LINE__);
			cmdline = cmdline_buf;
			command_too_long = false;
			ClientData_send_cmd(cd, MSG_NOSIGNAL, "%s,%d: command is too long\r", __FILE__, __LINE__);
			continue;
		}

		{
			const char* err = gen_arg_list(cmdline_buf, cmdline - cmdline_buf, &cb);
			cmdline = cmdline_buf;

			if(err)
			{
				ats_logf(ATSLOG(0), "%s,%d: gen_arg_list failed (%s)", __FILE__, __LINE__, err);
				ClientData_send_cmd(cd, MSG_NOSIGNAL, "%s,%d: gen_arg_list failed: %s\r", __FILE__, __LINE__, err);
				continue;
			}

		}

		if(cb.m_argc <= 0)
		{
			continue;
		}

		const ats::String cmd(cb.m_argv[0]);

		if("msg" == cmd)
		{
			ats::StringMap m;
			m.from_args(cb.m_argc - 1, cb.m_argv + 1);

			md.lock_data();
			g_msg.push_back(m);
			sem_post(&g_msg_sem);
			md.unlock_data();

			ClientData_send_cmd(cd, MSG_NOSIGNAL, "%s: ok\n\r", cmd.c_str());
		}
		else
		{
			ClientData_send_cmd(cd, MSG_NOSIGNAL, "Invalid command \"%s\"\n\r", cmd.c_str());
		}

	}

	return 0;
}


int main(int argc, char* argv[])
{
	g_log.open_testdata(g_app_name);
	g_log.set_global_logger(&g_log);

	MyData& md = g_md;
	md.set("debug", "1");
	md.set_from_args(argc - 1, argv +1);
	int dbg;
	dbg = md.get_int("debug");
	g_log.set_level(dbg);

	sem_init(&g_msg_sem, 0, 0);

	PacketizerCamsStateMachine *psm = new PacketizerCamsStateMachine(g_md);
	psm->run_state_machine(*psm);

	PacketizerCamsCopy *pc = new PacketizerCamsCopy(g_md);
	pc->run_state_machine(*pc);

	{
		static ServerData sd;
		init_ServerData(&sd, 64);
		sd.m_hook = &md;
		sd.m_cs = client_command_server;
		start_redstone_ud_server(&sd, g_app_name.c_str(), 1);
		signal_app_unix_socket_ready(g_app_name.c_str(), g_app_name.c_str());
		signal_app_ready(g_app_name.c_str());
	}

	ats::infinite_sleep();
	return 0;
}
