#include <iostream>

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <semaphore.h>
#include <unistd.h>

#include "ats-common.h"
#include "ats-string.h"
#include "ats-serial.h"
#include "atslogger.h"
#include "socket_interface.h"
#include "command_line_parser.h"
#include "ConfigDB.h"
#include "telit-monitor.h"
#include "PACSP1_AMH.h"
#include "PBREADY.h"
#include "ATCIMI_AMH.h"
#include "ATCCID_AMH.h"
#include "ATCSQ_AMH.h"
#include "ATCREG_AMH.h"
#include "ATCOPS_AMH.h"
#include "ATCGDCONT_AMH.h"
#include "ATRFSTS_AMH.h"
#include "ATPSNT_AMH.h"
#include "ATCNUM_AMH.h"
#include <RedStone_IPC.h>

#define APPNAME "telit-monitor"

ATSLogger g_log;
REDSTONE_IPC g_RedStone;
static int g_dbg = 0;
db_monitor::ConfigDB db;


int g_creg_mode = 0;	// g_creg_mode: Tracks the current operating mode of the "creg" command.
int g_psnt_mode = 0;
int g_AcT = -1;
int g_network_type = -1;
std::string g_CCID = "unknown";

const int g_signal_undetectable = 99;
int g_rssi = g_signal_undetectable + 1;

//=================================================================================================
static int ac_debug(AdminCommandContext &p_acc, int p_argc, char *p_argv[])
{
	if(p_argc <= 1)
	{
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "debug=%d\n", g_dbg);
	}
	else
	{
		g_dbg = strtol(p_argv[1], 0, 0);
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "debug=%d\n", g_dbg);
	}
	return 0;
}

//=================================================================================================
static int ac_help(AdminCommandContext &p_acc, int p_argc, char *p_argv[])
{
	send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "Available commands:\n\r");
	send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "  getcellimsi\n\r");
	send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "  getcellccid\n\r");
	send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "  getcellpnumber -  get the cell phone number\n\r");
	send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "  getcellnetwork - get the cellular network name\n\r");
	send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "  getcellapn - display the APN\n\r");
	send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "  debug [level] - set the debug level\n\r");
	send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "  stat\n\r");
	return 0;
}

//=================================================================================================
static int ac_getcellimsi(AdminCommandContext &p_acc, int p_argc, char *p_argv[])
{
	ats::StringMap &config = p_acc.m_data->m_config;
	const ats::String port(config.get("modem_port"));

	if(ats::file_exists(port))
	{
		ats::write_file(port, "at+cimi\r");
	}

	return 0;
}

//=================================================================================================
static int ac_getcellccid(AdminCommandContext &p_acc, int p_argc, char *p_argv[])
{
	ats::StringMap &config = p_acc.m_data->m_config;
	const ats::String port(config.get("modem_port"));

	if(ats::file_exists(port))
	{
		ats::write_file(port, "at+ccid\r");
	}

	return 0;
}

//=================================================================================================
static int ac_getcellpnumber(AdminCommandContext &p_acc, int p_argc, char *p_argv[])
{
	ats::StringMap &config = p_acc.m_data->m_config;
	const ats::String port(config.get("modem_port"));

	if(ats::file_exists(port))
	{
		ats::write_file(port, "at+cnum\r");
	}

	return 0;
}

//=================================================================================================
static int ac_getcellnetwork(AdminCommandContext &p_acc, int p_argc, char *p_argv[])
{
	ats::StringMap &config = p_acc.m_data->m_config;
	const ats::String port(config.get("modem_port"));

	if(ats::file_exists(port))
	{
		ats::write_file(port, "at+cops?\r");
	}

	return 0;
}

//=================================================================================================
static int ac_getcellapn(AdminCommandContext &p_acc, int p_argc, char *p_argv[])
{
	ats::StringMap &config = p_acc.m_data->m_config;
	const ats::String port(config.get("modem_port"));

	if(ats::file_exists(port))
	{
		ats::write_file(port, "at+cgdcont?\r");
	}

	return 0;
}

//=================================================================================================
static int ac_stat(AdminCommandContext &p_acc, int p_argc, char *p_argv[])
{
	send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "stat: rssi=%d creg_mode=%d psnt_mode=%d AcT=%d network_type=%d CCID=%s\n\r",
						g_rssi, g_creg_mode, g_psnt_mode, g_AcT, g_network_type, g_CCID.c_str());
	return 0;
}

//=================================================================================================
static int ac_ppp_up(AdminCommandContext &p_acc, int p_argc, char *p_argv[])
{
	ats_logf(ATSLOG_ERROR, "ppp_up received");
	g_RedStone.ModemState(AMS_PPP_ESTABLISHED);
	return 0;
}
//=================================================================================================
static int ac_ppp_down(AdminCommandContext &p_acc, int p_argc, char *p_argv[])
{
	ats_logf(ATSLOG_ERROR, "ppp_down received");
	g_RedStone.ModemState(AMS_NO_CARRIER);
	return 0;
}

//=================================================================================================
MyData::MyData()
{
	m_ppp_thread = 0;

	m_command.insert(AdminCommandPair("getcellimsi", ac_getcellimsi));
	m_command.insert(AdminCommandPair("getcellccid", ac_getcellccid));
	m_command.insert(AdminCommandPair("getcellpnumber", ac_getcellpnumber));
	m_command.insert(AdminCommandPair("getcellnetwork", ac_getcellnetwork));
	m_command.insert(AdminCommandPair("getcellapn", ac_getcellapn));
	m_command.insert(AdminCommandPair("stat", ac_stat));
	m_command.insert(AdminCommandPair("ppp_up", ac_ppp_up));
	m_command.insert(AdminCommandPair("ppp_down", ac_ppp_down));
	m_command.insert(AdminCommandPair("debug", ac_debug));
	m_command.insert(AdminCommandPair("help", ac_help));
}

// Description: Issues the given AT command "p_cmd", which MUST be carriage-return ('\r') terminated,
//	to the modem on port "p_port". The error history regarding issuing of the command is stored
//	in "p_err".
//
//	XXX: This function assumes that the last character of a non-empty string is always carriage-return ('\r').
//
// Return: False is returned if an error is detected while issuing the command, and true is returned
//	otherwise.
static bool issue_command(const ats::String& p_port, const ats::String& p_cmd, bool& p_err)
{

	if(p_cmd.empty())
	{
		return p_err;
	}

	if(ats::write_file(p_port, p_cmd.c_str()) < 0)
	{

		if(!p_err)
		{
			p_err = true;
			ats_logf(ATSLOG_DEBUG, "Failed to send \"%.*s\"", int(p_cmd.length()) - 1, p_cmd.c_str());
		}

	}
	else if(p_err)
	{
		p_err = false;
		ats_logf(ATSLOG_DEBUG, "\"%.*s\" OK", int(p_cmd.length()) - 1, p_cmd.c_str());
	}

	return p_err;
}

//=================================================================================================
// Description: This thread is responsible for sending all commands to the cellular modem.
static void* h_writer_thread(void* p)
{
	MyData &md = *((MyData *)p);
	ats::StringMap &config = md.m_config;
	const ats::String port(config.get("modem_port"));

	// Error Reported flags
	bool ER_exists = false;
	bool ER_csq = false;
	bool ER_creg_init = false;
	bool ER_creg = false;
	bool bCCID = false;
	const int poll_rate = 2;  // DRH removed from db-config Aug 2017

	for(;;)
	{

		if(!ats::file_exists(port))
		{

			if(!ER_exists)
			{
				ER_exists = true;
				ats_logf(ATSLOG_ERROR, "%d: Failed to configure modem port \"%s\"", __LINE__, port.c_str());
			}

			sleep(poll_rate);
			continue;
		}
		else if(ER_exists)
		{
			ER_exists = false;
			ats_logf(ATSLOG_ERROR, "%d: Modem port \"%s\" exists", __LINE__, port.c_str());
		}

		// Constantly try to set "creg" to mode "ENABLE_NETWORK_REGISTRATION_UNSOLICITED_RESULT_CODE_WITH_NETWORK_CELL_IDENTIFICATION_DATA".
		{
			#define ENABLE_NETWORK_REGISTRATION_UNSOLICITED_RESULT_CODE_WITH_NETWORK_CELL_IDENTIFICATION_DATA 2
			md.lock_data();
			const int mode = g_creg_mode;
			md.unlock_data();

			if(mode != ENABLE_NETWORK_REGISTRATION_UNSOLICITED_RESULT_CODE_WITH_NETWORK_CELL_IDENTIFICATION_DATA)
			{
				ats::String s;
				ats_sprintf(&s, "at+creg=%d\r", ENABLE_NETWORK_REGISTRATION_UNSOLICITED_RESULT_CODE_WITH_NETWORK_CELL_IDENTIFICATION_DATA);
				issue_command(port, s.c_str(), ER_creg_init);
			}
		}

		if (g_RedStone.ModemState() >= AMS_NO_CARRIER)  // only look at signal strength if we have a sim
		{
			issue_command(port, "at+creg?\r", ER_creg);
			issue_command(port, "at+csq\r", ER_csq);
			if (!bCCID) issue_command(port, "at+CCID\r", bCCID);
		}
		sleep(poll_rate);
	}

	return 0;
}

//=================================================================================================
static void process_response(MyData& md, const ats::String& line, ats::String& responseType)
{
	{
		ats::String cmd;
		size_t i = 0;

		if(ats::next_token(cmd, line, i, ":=? \t\r\n"))
		{
			ATMessageHandlerMap::const_iterator i = md.m_mh.find(cmd);

			if(i != md.m_mh.end())
			{
				ats_logf(ATSLOG_DEBUG, "processing cmd=\"%s\"", cmd.c_str());
				(i->second)->on_message(md, cmd, line);
				return;
			}
		}
	}

	// AWARE360 FIXME: Must choose message handler based on current response string, not previous response string.
	//	"responseType" memorizes the last command and this function currently assumes responses are for that command.
	//	However the modem can send "unsolicited" messages, therefore responses do not have to be in response to a previous
	//	command. Fix/change the code so that it determines the message handler based on the response returned (all responses
	//	will be prefixed with the name of the message handler they are for).
	//
	// Example: "+CSQ: 99,99"   // This unsolicited message is for the "+CSQ" message handler.
	ats::String response;

	for(size_t i = 0; i < line.length(); ++i)
	{
		const char c = line[i];

		if((c >= ' ') && (c <= 127))
		{
			response += c;
		}
		else
		{
			char buf[4];
			snprintf(buf, 3, "%02X", (unsigned char)c);
			response += "\\x";
			response += buf;
		}
	}

	if(responseType.empty())
	{
		responseType = ats::rtrim(line, "\r\n");
		return;
	}

	if(response.empty())
	{
		return;
	}

	if(("ERROR" == response) || ("OK" == response))
	{
		responseType.clear();
	}
	else
	{
		ats_logf(ATSLOG_DEBUG, "UNKNOWN response=\"%s\" for responseType=\"%s\"", response.c_str(), responseType.c_str());
	}
}

//=================================================================================================
void MyData::add_message_handler(const ats::String& p_name, ATMessageHandler* p_handler)
{
	m_mh.insert(ATMessageHandlerPair(p_name, p_handler));
}

//=================================================================================================
static void* h_reader_thread(void* p)
{
	MyData &md = *((MyData *)p);
	ats::String responseType;
	ats::String line;
	bool bModemStateSet = false;

	ats::ReadDataCache_fd rdc(md.fd);

	md.add_message_handler("+PBREADY", new (PBREADY));

	{
		ATMessageHandler* amh = new PACSP1_AMH();
		md.add_message_handler("+PACSP1", amh);
	}

	{
		ATMessageHandler* amh = new ATCIMI_AMH();
		md.add_message_handler("at+cimi", amh);
	}

	{
		ATMessageHandler* amh = new ATCCID_AMH();
		md.add_message_handler("at+ccid", amh);
		md.add_message_handler("+CCID", amh);
	}

	{
		ATMessageHandler* amh = new ATCNUM_AMH();
		md.add_message_handler("at+cnum", amh);
		md.add_message_handler("+CNUM", amh);
	}

	{
		ATMessageHandler* amh = new ATCSQ_AMH();
		md.add_message_handler("at+csq", amh);
		md.add_message_handler("+CSQ", amh);
	}

	{
		ATMessageHandler* amh = new ATCOPS_AMH();
		md.add_message_handler("at+cops", amh);
		md.add_message_handler("+COPS", amh);
	}

	{
		ATMessageHandler* amh = new ATCREG_AMH();
		md.add_message_handler("at+creg", amh);
		md.add_message_handler("at+creg", amh);
		md.add_message_handler("+CREG", amh);
	}

	{
		ATMessageHandler* amh = new ATRFSTS_AMH();
		md.add_message_handler("at#rfsts", amh);
		md.add_message_handler("#RFSTS", amh);
	}

	{
		ATMessageHandler* amh = new ATPSNT_AMH();
		md.add_message_handler("at#psnt", amh);
		md.add_message_handler("#PSNT", amh);
	}

	{
		ATMessageHandler* amh = new ATCGDCONT_AMH();
		md.add_message_handler("at+cgdcont", amh);
		md.add_message_handler("+CGDCONT", amh);
	}

	for(;;)
	{
		const int c = rdc.getc();

		if(c < 0)
		{
//			g_RedStone.ModemState(0);
			ats_logf(ATSLOG_DEBUG, "%s,%d: Failed to read: (%d) %s", __FILE__, __LINE__, c, strerror(-c));
			// XXX: If "read" fails, then the write/read file descriptor pair has failed, and this process
			//	can no longer send and receive responses on the modem AT command port. In this event, the
			//	process shall terminate (so that the write/read file descriptor pair can be re-created).
			exit(1);
		}

		if (!bModemStateSet)  // only set the state once.
		{
			g_RedStone.ModemState(AMS_NO_SIM);  // set state to 'talking to internal modem'
			bModemStateSet = true;
			ats_logf(ATSLOG_ERROR, "Communication with modem module established.  Setting ModemState to AMS_NO_SIM");
		}
		
		if(('\n' == c) || ('\r' == c))
		{
			process_response(md, line, responseType);
			line.clear();
		}
		else
		{
			line += c;
		}

	}

	return 0;
}

// ************************************************************************
// Description: A local command server so that other applications or
//      developers can query this application.
//
//      One instance of this function is created for every local connection
//      made.
//
// Parameters:
// Return: NULL pointer
// ************************************************************************
static void* local_command_server( void* p)
{
	ClientData* cd = (ClientData*)p;
	MyData &md =* ((MyData*)(cd->m_data->m_hook));

	bool command_too_long = false;
	ats::String cmd;

	const size_t max_command_length = 1024;

	AdminCommandContext acc(md, *cd);

	ClientDataCache cdc;
	init_ClientDataCache(&cdc);

	for(;;)
	{
		char ebuf[1024];
		const int c = client_getc_cached(cd, ebuf, sizeof(ebuf), &cdc);

		if(c < 0)
		{
			if(c != -ENODATA) ats_logf(ATSLOG_DEBUG, "%s,%d: %s", __FILE__, __LINE__, ebuf);
			break;
		}

		if(c != '\r' && c != '\n')
		{
			if(cmd.length() >= max_command_length) command_too_long = true;
			else cmd += c;

			continue;
		}

		if(command_too_long)
		{
			ats_logf(ATSLOG_DEBUG, "%s,%d: command is too long", __FILE__, __LINE__);
			cmd.clear();
			command_too_long = false;
			send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "command is too long\n");
			continue;
		}

		CommandBuffer cb;
		init_CommandBuffer(&cb);
		const char* err;

		if((err = gen_arg_list(cmd.c_str(), cmd.length(), &cb)))
		{
			ats_logf(ATSLOG_ERROR, "%s,%d: gen_arg_list failed (%s)", __FILE__, __LINE__, err);
			send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "gen_arg_list failed: %s\n", err);
			cmd.clear();
			continue;
		}

		if(g_dbg >= 2)
		{
			ats_logf(ATSLOG_DEBUG, "%s,%d:%s: Command received: %s", __FILE__, __LINE__, __FUNCTION__, cmd.c_str());
		}

		cmd.clear();

		if(cb.m_argc <= 0)
		{
			continue;
		}

		{
			const ats::String cmd(cb.m_argv[0]);
			AdminCommandMap::const_iterator i = md.m_command.find( cmd);

			if(i != md.m_command.end())
			{
				(i->second)(acc, cb.m_argc, cb.m_argv);
				break;
			}
			else
			{
				send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "Invalid command \"%s\"\n", cmd.c_str());
			}
		}
	}

	return 0;
}

//=================================================================================================
void MyData::start_server()
{
	ServerData& sd = m_command_server;
	init_ServerData(&sd, 64);
	sd.m_hook = this;
	sd.m_cs = local_command_server;
	set_unix_domain_socket_user_group(&sd, m_config.get("ipc_user").c_str(), m_config.get("ipc_user").c_str());
	start_redstone_ud_server(&sd, APPNAME, 1);
	signal_app_unix_socket_ready(APPNAME, APPNAME);
}
//=================================================================================================
int main(int argc, char* argv[])
{
	MyData& md = *(new MyData());
	ats::StringMap &config = md.m_config;
	config.set("user", "telit");
	config.set("ipc_user", "applet");
	config.set("debug", "1");
	config.set("modem_port", TELIT_PORT);
	config.from_args(argc - 1, argv + 1);

	g_CCID = db.Get("Cellular", "CCID");

	g_log.set_global_logger(&g_log);
	g_log.set_level( db.GetInt("RedStone","LogLevel", 0) );
	g_log.open_testdata(APPNAME);

	g_dbg = config.get_int("debug");

	const ats::String port(config.get("modem_port"));

	{
		ats::String svn;
		ats::String version;
		ats::String full;
		ats::trulink_firmware_version(&svn, &version, &full);
		ats_logf(ATSLOG_DEBUG, "Telit Monitor (%s, %s, %s) Started (pid=%d, ppid=%d)", svn.c_str(), version.c_str(), full.c_str(), getpid(), getppid());
	}

	if(!((access(port.c_str(), F_OK) != -1) ?  1: 0))
	{
		ats_logf(ATSLOG_DEBUG, "No telit device %s", port.c_str());
		exit(1);
	}

	{
		const int ret = ats::setup_modem_serial_port(port);

		if(ret < 0)
		{
			ats_logf(ATSLOG_DEBUG, "%s,%d: Failed to configure modem port \"%s\": (%d) %s", __FILE__, __LINE__, port.c_str(), -ret, strerror(-ret));
		}
	}

	int open_fail_count = 0;

	for(;;)
	{
		md.fd = open(port.c_str(), O_RDONLY);

		if(md.fd < 0)
		{
			++open_fail_count;

			if(open_fail_count < 2)
			{
				ats_logf(ATSLOG_DEBUG, "%s,%d: Failed to open modem port \"%s\". (%d) %s", __FILE__, __LINE__, port.c_str(), errno, strerror(errno));
			}

			sleep(1);
			continue;
		}

		break;
	}

	// create the threads to send/receive over port
	static pthread_t m_reader_thread;
	static pthread_t m_writer_thread;

	if (pthread_create(&(m_reader_thread),	(pthread_attr_t *)0,h_reader_thread, &md))
	{
		ats_logf(ATSLOG_DEBUG, "%s,%d: Failed to create reader. (%d) %s", __FILE__, __LINE__, errno, strerror(errno));
		return -1;
	}
	
	if (pthread_create(	&(m_writer_thread),	(pthread_attr_t *)0, h_writer_thread, &md))
	{
		ats_logf(ATSLOG_DEBUG, "%s,%d: Failed to create rssi thread. (%d) %s", __FILE__, __LINE__, errno, strerror(errno));
		return -1;
	}

	md.start_server();

	if(ats::su(config.get("user"), config.get("ipc_user")))
	{
		ats_logf(ATSLOG_DEBUG, "Failed to become user/group \"%s,%s\" (%d) %s",
			(config.get("user")).c_str(), (config.get("ipc_user")).c_str(), errno, strerror(errno));
		return 1;
	}

	signal_app_ready(APPNAME);
	ats::infinite_sleep();
	return 0;
}
