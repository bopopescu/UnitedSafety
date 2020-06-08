#include <iostream>

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <stdlib.h>
#include <errno.h>

#include "ats-common.h"
#include "SocketInterfaceResponse.h"
#include "redstone-socketcan.h"
#include "linux/can.h"
#include "socket_interface.h"
#include "command_line_parser.h"
#include "AFS_Timer.h"
#include "RedStone_IPC.h"

static bool g_seatbelt_flag = true;
static bool g_testdata = false;
static AFS_Timer* g_debug_timer;

static bool testdatadir_existing()
{
	struct stat st;
	const char* s1 = "/var/log/testdata";

	if(stat(s1, &st) != 0)
	{
		return false;
	}

	return true;
}

static int monitor_seatbelt( ats::StringMap &p_config)
{
	ats::StringMap &config = p_config;
	const int dbg = config.get_int("debug");

	if(config.get_bool("config_33333_CAN_baud"))
	{
		const char* data[] = {
			"/br_presdiv", "45",
			"/br_propseg", "6",
			"/br_pseg1", "7",
			"/br_pseg2", "2",
			0, 0
		};

		const ats::String& can_dev_loc = config.get("can_dev_loc");
		const char** p = data;

		for(; *p;)
		{

			if(ats::write_file(can_dev_loc + p[0], p[1]) < 0)
			{
				syslog(LOG_ERR, "config_33333_CAN_baud: \"%s > %s\" (%d) %s", p[0], p[1], errno, strerror(errno));
				exit(1);
			}

			p += 2;
		}

		std::stringstream err;
		const int ret = ats::system("ifconfig " + config.get("can_dev") + " up 2>&1", &err);

		if(WEXITSTATUS(ret))
		{
			syslog(LOG_ERR, "config_33333_CAN_baud: ifconfig failed: %s", err.str().c_str());
			exit(1);
		}

	}

	CANSocket *s;
	size_t can_socket_create_fail_count = 0;
	for(;;)
	{
		s = create_new_CANSocket();
		if(s) break;

		++can_socket_create_fail_count;
		if(1 == can_socket_create_fail_count)
			syslog(LOG_ERR, "%s,%d: Failed to create CANSocket\n", __FILE__, __LINE__);
		sleep(1);
	}

	CAN_connect(s, config.get("can_dev").c_str());

	CAN_filter(s, 0x130058, CAN_EFF_MASK);

	for(;;)
	{
		struct can_frame msg;
		int bytes_read = CAN_read(s, &msg);

		if(bytes_read > 0)
		{
			if(dbg > 1 || g_testdata)
			{
				std::stringstream s;
				size_t i;
				unsigned char *p = (unsigned char *)&msg;
				for(i = 0; i < sizeof(msg); ++i)
				{
					if(i) s << ",";
					char buf[8];
					snprintf(buf, 7, "%02X", p[i]);
					buf[7] = '\0';
					s << buf;
				}
				std::stringstream ss;
				ss << (g_debug_timer->ElapsedTime()) << ":read";
				syslog(LOG_NOTICE, "%s,%d: %s: %s", __FILE__, __LINE__, ss.str().c_str(),s.str().c_str());
			}

			if(0x80130058 == msg.can_id)
			{
				if(6 == msg.can_dlc)
				{
					if(msg.data[0] & 0x80)
					{
						if(dbg || g_testdata)
						{
							std::stringstream ss;
							ss << (g_debug_timer->ElapsedTime()) << "Seatbelt is unbuckled.\n";
							syslog(LOG_NOTICE, "%s",ss.str().c_str());
						}

						g_seatbelt_flag = false;
						// ATS FIXME: Errors are ignored for now. Log the errors somewhere (but not too many).
						send_redstone_ud_msg("PROC_Seatbelt", 0, "seatbelt 0\r");
						send_redstone_ud_msg("message-assembler", 0, "seatbelt 0\r");
					}
					else
					{
						if(dbg || g_testdata)
						{
							std::stringstream ss;
							ss << (g_debug_timer->ElapsedTime()) << "Seatbelt is buckled.\n";
							syslog(LOG_NOTICE, "%s",ss.str().c_str());
						}

						g_seatbelt_flag = true;
						// ATS FIXME: Errors are ignored for now. Log the errors somewhere (but not too many).
						send_redstone_ud_msg("PROC_Seatbelt", 0, "seatbelt 1\r");
						send_redstone_ud_msg("message-assembler", 0, "seatbelt 1\r");
					}
				}

			}
		}
		else if(bytes_read == 0)
		{
			break;
		}
		else
		{
			syslog(LOG_ERR, "%s,%d: CAN_read error (%d): %s", __FILE__, __LINE__, errno, strerror(errno));
			break;
		}
	}

	syslog(LOG_ERR, "%s,%d:CAN Connection is closed\n", __FILE__, __LINE__);
	return 0;
}

static void* client_server(void* p)
{
	ClientData& cd = *((ClientData*)p);

	CommandBuffer cb;
	init_CommandBuffer(&cb);

	ats::SocketInterfaceResponse response(cd.m_sockfd);

	for(;;)
	{
		const ats::String& cmdline = response.get(0, "\n\r", 0);

		if(response.error())
		{
			break;
		}

		if(gen_arg_list(cmdline.c_str(), int(cmdline.size()), &cb))
		{
			break;
		}

		if(cb.m_argc < 1)
		{
			continue;
		}

		const ats::String cmd(cb.m_argv[0]);

		if("seatbelt" == cmd)
		{

			if(send_cmd(cd.m_sockfd, MSG_NOSIGNAL, "seatbelt: %d\n\r", g_seatbelt_flag ? 1 : 0) <= 0)
			{
				break;
			}

		}
		else
		{

			if(send_cmd(cd.m_sockfd, MSG_NOSIGNAL, "error: Invalid command \"%s\"\n\r", cmd.c_str()) <= 0)
			{
				break;
			}

		}

	}

	shutdown(cd.m_sockfd, SHUT_WR);
	// FIXME: Race-condition exists because "shutdown" does not gaurantee that the receiver will receive all the data.
	close_client(&cd);

	if(response.error())
	{
		syslog(LOG_ERR, "%s,%d: (%d) %s", __FILE__, __LINE__, response.error(), strerror(response.error()));
	}

	return 0;
}


int main(int argc, char* argv[])
{
	ats::StringMap config;
	config.set("can_dev", "can1");
	config.set("config_33333_CAN_baud", "1");
	config.set("can_dev_loc", "/sys/devices/platform/FlexCAN.1");
	config.set("client_server_port", "41204");
	config.set("max_clients", "16");
	config.from_args(argc - 1, argv + 1);

	g_debug_timer = new AFS_Timer();
	g_testdata = testdatadir_existing();

	ServerData* sd = new ServerData();
	init_ServerData(sd, config.get_int("max_clients"));

	{
		sd->m_cs = client_server;
		sd->m_hook = 0;
		sd->m_port = config.get_int("client_server_port");

		if(start_server(sd))
		{
			syslog(LOG_ERR, "Error starting client/device server: %s", sd->m_emsg);
			exit(1);
		}

	}

	for(;;)
	{
		monitor_seatbelt( config);
	}

	return 0;
}
