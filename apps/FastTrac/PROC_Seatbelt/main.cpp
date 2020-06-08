//
// Process for creating a "STS_SEAT_BELT" event in the FASTTrac.
//
//	A proc event does not access any serial ports.
//	This Tracker proc will output a Tracker event to the
//	AFF at the whenever a seat belt event occurs.
//
//	IPC with FASTTrac is handled via the assigned AFS_PROC slot (either AFS_PROC1, AFS_PROC2, or AFS_PROC3)
//	IPC with the AFF is handled in AFS_AFF.
//
// Task description
//	Startup
//	Initiate IPC based on PROC slot argument (the MCU will start the proc with an argument for which PROC it is to use)
//	Read the parms
//	monitor IPC for setup
//	handle setup
//	Output Tracker events to AFS_AFF whenever a seat belt event occurs
//

// PROC_Seatbelt - Sends "STS_SEAT_BELT" event to the AFF when a seat belt event occurs.
//
// Usage: PROC_Seatbelt("PROC1") or PROC_Seatbelt("PROC2")
// This determines what IPC memory to look at.
//

#include <iostream>
#include <string>

#include <stdio.h>
#include <sys/time.h>
#include <errno.h>

#include "Proc_Seatbelt.h"
#include "Proc_SeatbeltStateMachine.h"

#include "ats-common.h"
#include "atslogger.h"
#include "socket_interface.h"
#include "command_line_parser.h"
#include "SocketQuery.h"
#include "ConfigDB.h"

using namespace std;

ATSLogger g_log;
int g_dbg = 0;
//Proc_Tracker* g_track_ptr = 0;
int g_seatbeltBit = 1;
REDSTONE_IPC* g_shmem = 0;

static void *serv_client(void *p);

//-------------------------------------------------------------------------------------------------
int ac_debug(AdminCommandContext &p_acc, const AdminCommand&, int p_argc, char *p_argv[])
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

//-------------------------------------------------------------------------------------------------
int ac_help(AdminCommandContext &p_acc, const AdminCommand&, int p_argc, char *p_argv[])
{
	send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "This is the help system\n\r");
	return 0;
}

// ************************************************************************
// Description: A local command server so that other applications or
//			developers can query this application.
//
//			One instance of this function is created for every local connection
//			made.
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

	ClientDataCache cache;
	init_ClientDataCache(&cache);

	for(;;)
	{
		char ebuf[1024];
		const int c = client_getc_cached(cd, ebuf, sizeof(ebuf), &cache);

		if(c < 0)
		{
			if(c != -ENODATA) ats_logf(ATSLOG(0), "%s,%d: %s", __FILE__, __LINE__, ebuf);
			break;
		}

		if(c != '\r' && c != '\n')
		{
			if(cmd.length() >= max_command_length) 
				command_too_long = true;
			else 
				cmd += c;

			continue;
		}

		if(command_too_long)
		{
			ats_logf(&g_log, "%s,%d: command is too long", __FILE__, __LINE__);
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
			ats_logf(&g_log, "%s,%d: gen_arg_list failed (%s)", __FILE__, __LINE__, err);
			send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "gen_arg_list failed: %s\n", err);
			cmd.clear();
			continue;
		}

		ats_logf(ATSLOG(2), "%s,%d:%s: Command received: %s", __FILE__, __LINE__, __FUNCTION__, cmd.c_str());

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
				(i->second).m_fn(acc, i->second, cb.m_argc, cb.m_argv);
			}
			else
			{
				send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "Invalid command \"%s\"\n", cmd.c_str());
			}
		}
	}
	return 0;
}

//-------------------------------------------------------------------------------------------------
void MyData::start_server()
{
	ServerData& sd = m_command_server;
	init_ServerData(&sd, 32);
	sd.m_hook = this;
	sd.m_cs = local_command_server;
	const char* user = "applet";
	set_unix_domain_socket_user_group(&sd, user, user);
	::start_redstone_ud_server(&sd, "PROC_Seatbelt", 1);
	signal_app_unix_socket_ready("PROC_Seatbelt", "PROC_Seatbelt");
	signal_app_ready("PROC_Seatbelt");
	wait_for_app_ready("message-assembler");
	wait_for_app_ready("i2c-gpio-monitor");
	send_trulink_ud_msg("i2c-gpio-monitor", 0, "uds %s\r", ATS_APP_NAME);
}

//-------------------------------------------------------------------------------------------------
MyData::MyData():m_seatbeltBit(0)
{
	m_exit_sem = new sem_t;
	sem_init(m_exit_sem, 0, 0);

	m_mutex = new pthread_mutex_t;
	pthread_mutex_init(m_mutex, 0);

	loadConfig();

	m_command.insert(AdminCommandPair("help", AdminCommand(ac_help, "Displays help information")));
	m_command.insert(AdminCommandPair("h", AdminCommand(ac_help, "Displays help information")));
	m_command.insert(AdminCommandPair("?", AdminCommand(ac_help, "Displays help information")));

	m_command.insert(AdminCommandPair("debug", AdminCommand(ac_debug, "Displays debug information")));
}

//-------------------------------------------------------------------------------------------------
void MyData::loadConfig()
{
	db_monitor::ConfigDB db;
	const ats::String app_name(ATS_APP_NAME);
	m_low_speed = db.GetInt(app_name, "CutoffSpeed", 15);
	m_speed_exceedence_duration_seconds = db.GetInt(app_name, "SpeedCheckDuration", 2);
	m_buzzer_delay_time_limit = db.GetInt(app_name, "BuzzerDelay", 3);
	m_unbuckle_time_limit = db.GetInt(app_name, "UnbuckleTimeLimit", 30);
	m_buzzer_timeout = db.GetInt(app_name, "BuzzerTimeout", 60);
	m_buzzer_cmd = db.GetValue(app_name, "BuzzerCmd", "550000 150000");
	m_buzzer_port = db.GetInt(app_name, "BuzzerPort", 41008);

	// Inputs are labelled as 1-4 or 1-6 - so the user should see that value - not the actual bit value 
	// (which is 0-3 or 0-5)
	m_seatbeltBit = db.GetInt(app_name, "SensorInput", 1) - 1; //Default: input 1 as seatbelt bit. 
	
	if (m_seatbeltBit < 0 || m_seatbeltBit > 5) // sanity check for proper bit value.
	  m_seatbeltBit = 0;
	g_seatbeltBit = m_seatbeltBit;
}

EVENTDECLARE(Speed);
EVENTDECLARE(Ignition);
EVENTDECLARE(Seatbelt);
EVENTDECLARE(Restart);

//-------------------------------------------------------------------------------------------------
int main(int argc, char **argv)
{
	g_log.set_global_logger(&g_log);
	g_log.set_level(g_dbg);
	g_log.open_testdata(ATS_APP_NAME);

	ats::system("echo -n " + ats::toStr(getpid()) + " > /var/run/PROC_Seatbelt.pid", NULL);

	static MyData md;
	g_shmem = new REDSTONE_IPC;

	ats_logf(ATSLOG(0), "%s,%d:%s: start state machine", __FILE__, __LINE__, __FUNCTION__);
	SeatbeltStateMachine* w = new SeatbeltStateMachine(md);
	w->run_state_machine(*w);

	ats_logf(ATSLOG(0), "%s,%d:%s: start server", __FILE__, __LINE__, __FUNCTION__);
	md.start_server();
	ats::infinite_sleep();
	return 0;

}

