//-------------------------------------------------------------------------------------------------
//
// NMEA - program to decode NMEA data and fill the shared memory with the processed position data.
//
//	db-config settings:
//		NMEA Logging (On/Off) - allows logging of all raw data to the log file
//

#include <iostream>
#include <sstream>
#include <syslog.h>

#include "NMEA_DATA.h"
#include "NMEA.h"
#include "NMEA_Client.h"
#include "AFS_Timer.h"
#include "ats-common.h"
#include "ats-string.h"
#include "ConfigDB.h"
#include "atslogger.h"
#include "socket_interface.h"
#include "command_line_parser.h"
#include "MyData.h"

#include <RedStone_IPC.h>

using namespace std;

bool g_bDebugging = false;
ATSLogger g_log;
NMEA* pdev;	// pointer to the NMEA class for the input generator thread.
REDSTONE_IPC m_RedStoneData;
int dbg_level;

static const ats::String nmea_returns[] = 
{
	"NMEA_OK",
	"NMEA_NOT_NMEA",
	"NMEA_BAD_CHECKSUM",
	"NMEA_BAD_NMEA",
	"NMEA_GGA_OK", 
	"NMEA_RMC_OK",
	"NMEA_EMPTY"
};

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
static void* local_command_server( void* pClientData)
{
	ClientData* cd = (ClientData*)pClientData;
	MyData &md =* ((MyData*)(cd->m_data->m_hook));

	bool command_too_long = false;
	ats::String cmd;

	const size_t max_command_length = 1024;

	AdminCommandContext adminCC(md, *cd);

	ClientDataCache cdc;
	init_ClientDataCache(&cdc);

	for(;;)
	{
		char ebuf[1024];
		const int c = client_getc_cached(cd, ebuf, sizeof(ebuf), &cdc);

		if(c < 0)
		{
			if(c != -ENODATA)
				ats_logf(ATSLOG_ERROR, "%s,%d: %s", __FILE__, __LINE__, ebuf);

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
			ats_logf(ATSLOG_ERROR, "%s,%d: command is too long", __FILE__, __LINE__);
			cmd.clear();
			command_too_long = false;
			send_cmd(adminCC.get_sockfd(), MSG_NOSIGNAL, "command is too long\n");
			continue;
		}

		CommandBuffer cb;
		init_CommandBuffer(&cb);
		const char* err;

		if((err = gen_arg_list(cmd.c_str(), cmd.length(), &cb)))
		{
			ats_logf(ATSLOG_ERROR, "%s,%d: gen_arg_list failed (%s)", __FILE__, __LINE__, err);
			send_cmd(adminCC.get_sockfd(), MSG_NOSIGNAL, "gen_arg_list failed: %s\n", err);
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
				(i->second).m_fn(adminCC, i->second, cb.m_argc, cb.m_argv);
			}
			else
			{
				send_cmd(adminCC.get_sockfd(), MSG_NOSIGNAL, "Invalid command \"%s\"\n", cmd.c_str());
			}
		}
	}
	return 0;
}

static int g_LEDState = 0;


static void set_LEDState(int p_state)
{
	static bool g_bSentInetNotification = false; //ISCP-327
	if(p_state == g_LEDState)
	{
		return;
	}

	g_LEDState = p_state;

	switch(p_state)
	{
	case 1: 
		send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink kick name=gps led=gps script=\"1,1000000\" \r"); 
		send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink kick name=gpsr led=gps.r script=\"0,1000000\" \r"); 
		break;
	case 2: 
		send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink kick name=gps led=gps script=\"1,2900000;0,100000\" \r"); 
		send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink kick name=gpsr led=gps.r script=\"0,1000000\" \r"); 
		g_bSentInetNotification = false; //ISCP-257
		break;
	case 3: 
		send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink kick name=gps led=gps script=\"1,125000;0,125000\" \r"); 
		send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink kick name=gpsr led=gps.r script=\"0,1000000\" \r"); 
		break;
	case 4: 
		send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink kick name=gps  led=gps   script=\"0,500000\"\r"); 
		send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink kick name=gpsr led=gps.r script=\"1,500000\"\r"); //ISCP-57/ISCP-329
		if (g_bSentInetNotification == false)//ISCP-327
	{
		g_bSentInetNotification = true;
			ats_logf(ATSLOG_DEBUG, "NMEA sending inet_error message\r");		
		AFS_Timer t;
		t.SetTime();
		std::string user_data = "966," + t.GetTimestampWithOS() + ", TGX has lost GPS";
		user_data = ats::to_hex(user_data);
		send_redstone_ud_msg("message-assembler", 0, "msg inet_error msg_priority=9 usr_msg_data=%s\r", user_data.c_str());
	}
		break;
	case 5: 
		send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink kick name=gps led=gps script=\"0,1000000\" \r"); 
		send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink kick name=gpsr led=gps.r script=\"1,1000000\" \r"); 
		break;
	}
	
}

static void update_gps_LED_state(NMEA_Client& p_nmea_client, int& p_LEDState)
{

	if(p_nmea_client.IsValid())
	{

		if (p_nmea_client.GPS_Quality() == 2)
		{
			set_LEDState(1);
			return;
		}
		else if (p_nmea_client.GPS_Quality() == 1)
		{
			set_LEDState(2);
			return;
		}

	}

	if(p_LEDState != 3)
	{
		set_LEDState(3);
	}

}

//-----------------------------------------------------------------------------
//	checks the GPS time and date against the system time and date
//
static void CheckSystemTime(NMEA_DATA & nmea)
{
	J2K sysTime;
	J2K nmeaTime;
	static short lastMinute = -1;
	static time_t prevTime = 0;
	static size_t timesSet = 0;
	sysTime.SetSystemTime();

	if (!nmea.isValid())
		return;
	if ( nmea.year < 2009 ) // sanity check on GPS
		return;

	nmeaTime.set_dmy_hms(nmea.day, nmea.month, nmea.year,
											 nmea.hour, nmea.minute, nmea.seconds );

	// ATS FIXME:
	// Some programs rely on the system time for timing (such as the gettimeofday
	// function). All programs using gettimeofday (or similar) must be fixed.
	// [Amour Hassan - July 5, 2012]
	
	if (fabs(sysTime - nmeaTime) > 1.0)
	{
		struct tm *atm;
		time_t rawtime;
		time (&rawtime);
		atm = localtime(&rawtime);
		atm->tm_year = nmeaTime.GetYear() - 1900;
		atm->tm_mon = nmeaTime.GetMonth() - 1;
		atm->tm_mday = nmeaTime.GetDay();
		atm->tm_hour = nmeaTime.GetHour();
		atm->tm_min = nmeaTime.GetMinute();
		atm->tm_sec = nmeaTime.GetSecond();

		const time_t x = mktime(atm);
		if(x > prevTime)
		{
			stime(&x);	// sets the system time
			++timesSet;
			send_redstone_ud_msg("rtc-monitor", 0, "set-rtc gps=1\r");

			static stringstream prev_date;
			stringstream date;
			ats::system("date|tr -d '\\n'|tr -d '\\r'", &date);
			{
				ats_logf(ATSLOG_ERROR,
				"SYS and RTC set to %zu at %s\n"
				"Prev set time was %zu at %s\n"
				"Times Set: %zu\n",
				x,
				date.str().c_str(),
				prevTime,
				prev_date.str().c_str(),
				timesSet);
			}
			prev_date.str(date.str());
			prevTime = x;
		}
	}

	lastMinute = nmeaTime.GetMinute();
}

static bool apply_custom_gga_rmc(db_monitor::ConfigDB& p_db, NMEA& p_nmea, NMEA_IPC& p_ipc, const ats::String& p_gga, const ats::String& p_rmc, char p_data_type)
{
	const bool use_system_time = true;

	if(NMEA_OK == (p_nmea.Add(p_gga.c_str(), p_gga.length())))
	{

		if(use_system_time)
		{
			p_nmea.SetTimeFromSystem();
		}

		p_nmea.GetData().m_type = p_data_type;
		p_ipc.Data(p_nmea.GetData());	// copy data so the system can see it. Do it here so that invalid records get added in.

		if(NMEA_OK == p_nmea.Add(p_rmc.c_str(), p_rmc.length()))
		{

			if(use_system_time)
			{
				p_nmea.SetTimeFromSystem();
			}

			p_nmea.GetData().m_type = p_data_type;
			p_ipc.Data(p_nmea.GetData());	// copy data so the system can see it. Do it here so that invalid records get added in.
			return true;
		}

	}

	return false;
}

// Description: Injects the most recent valid NMEA GGA and RMC logs available in the NMEA position
// database.
static void get_fixed_or_historic_position(db_monitor::ConfigDB& p_db, NMEA& p_nmea, NMEA_IPC& p_ipc, bool p_use_fixed_position)
{
	if(p_use_fixed_position)
	{

		if(apply_custom_gga_rmc(p_db, p_nmea, p_ipc, p_db.GetValue("NMEA", "FixedGGA"), p_db.GetValue("NMEA", "FixedRMC"), NMEA_DATA::FixedDataFlag))
		{
			return;
		}

	}

	const ats::String& emsg = p_db.query(ATS_LOG_KEY, "select v_gga,v_rmc from t_NMEA_pos where v_id = (select max(v_id) from t_NMEA_pos)");

	if(!emsg.empty())
	{
		return;
	}

	const db_monitor::ResultTable& t = p_db.Table();

	if(t.empty())
	{
		return;
	}

	const db_monitor::ResultRow& r = t[0];

	if(r.size() < 2)
	{
		return;
	}

	const ats::String gga(r[0]);
	const ats::String rmc(r[1]);
	apply_custom_gga_rmc(p_db, p_nmea, p_ipc, gga, rmc, NMEA_DATA::HistoricDataFlag);
}

enum POSITION_EVENT
{
	NULL_POSITION_EVENT,
	GOT_LIVE_DATA,
	LOST_LIVE_DATA
};

// Description: Sets the current position event state to "pe" and logs __only__ the change.
static void position_event(enum POSITION_EVENT& p_pe, enum POSITION_EVENT pe)
{
	if(p_pe != pe)
	{
		p_pe = pe;
		const char* s;

		#define TMP_CASE(E) case E: s = #E; break
		switch(pe)
		{
		TMP_CASE(GOT_LIVE_DATA);
		TMP_CASE(LOST_LIVE_DATA);
		default: s = "NULL_POSITION_EVENT"; break;
		}
		#undef TMP_CASE

		ats_logf(ATSLOG_INFO, "%s: [%d] %s", __FUNCTION__, pe, s);
	}
}

int main(int argc, char **argv)
{
	wait_for_app_ready("message-assembler");

	bool& bDebugging = g_bDebugging;

	ats::String user;

	if (argc != 1)
	{

		ats::StringMap arg;
		arg.from_args(argc - 1, argv + 1);

		user = arg.get("user");

		const ats::String& show = arg.get("show");

		if(!show.empty())
		{

			if("NMEA_Client" == show)
			{
				cout << NMEA_Client() << "\n";
			}
			else
			{
				cerr << "Cannot show \"" << show << "\"\n";
				return 1;
			}

			return 0;
		}

		bDebugging = arg.get_bool("debug");
	}
	
	{  // scoping is required to force closure of db-config database
		db_monitor::ConfigDB db;
		dbg_level = db.GetInt(argv[0], "LogLevel", 0);  // 0:Error 1:Debug 2:Info
	}
	
	g_log.set_global_logger(&g_log);
	g_log.set_level(dbg_level);

	g_log.open_testdata("NMEA");
	ATSLogger::set_global_logger(&g_log);
	ats_logf(ATSLOG_ERROR, "---------- NMEA startup ----------");
	sleep(10);//ISCP-327

	NMEA_DATA_SOURCE curSource = DS_ANY;	// we take anything to start with.
	int LEDState = 0;

	int ret;
	bool done = false;
	AFS_Timer theTimer;
	AFS_Timer noGPSTimer;
	AFS_Timer LEDTimer;
	int nGPSRestarts = 0;
	int nEmptyCount = 0;

	// now get the speed source - either OBD or GPS
	db_monitor::ConfigDB db;
	const ats::String speed_source("GPS");
	const bool bUseOBDSpeed = false;
	const bool use_fixed_position = ("Fixed" == db.GetValue("NMEA", "Source"));
	
	if (use_fixed_position)
		ats_logf(ATSLOG_ERROR, "WARNING!!!!    Using a fixed position.");
	
	MyData& md = MyData::getInstance();
	md.open_gps_log(db);

	ServerData sd;
	init_ServerData(&sd, 64);
	sd.m_hook = &md;
	sd.m_cs = local_command_server;
	set_unix_domain_socket_user_group(&sd, "applet", "applet");
	::start_redstone_ud_server(&sd, "SER_GPS", 1);

	NMEA& nmea = *(md.m_nmea);
	pdev = &nmea;
	NMEA_IPC ipc;
	ipc.Type(NMEA_UPDATE_NONE);	// do this here so that the initial setup is the way we want.	If it is
	ipc.Source(DS_ANY);					// done in the constructor any new accesses to the IPC data would overwrite these values.

	NMEA_Client nmea_client;

	// ATS FIXME: Cannot run as user "applet" due to bug #629.
	//ats::su(user.empty() ? "applet" : user);

	enum POSITION_EVENT pe = NULL_POSITION_EVENT;

	while (!done)
	{

		switch (ipc.Type())
		{
			case	 NMEA_UPDATE_NONE:	// set to this after the NMEA program uses the data
				break;
			case	 NMEA_UPDATE_UPDATING: // set when text is going to be added to block other units from adding
				break;
			case	 NMEA_UPDATE_TEXT:	// text data coming in from a GPS device
				{
					ipc.Type(NMEA_UPDATE_UPDATING);	// prevent any further changes while we read.
					theTimer.SetTime();	// kick the timer (watching for no chars on input to invalidate the position

					ret = nmea.Add(ipc.Text(), strlen(ipc.Text()));
					ipc.Data(nmea.GetData());	// copy data so the system can see it.	Do it here so that invalid records get added in.

					if (bDebugging)
						cout << "NMEA:: decode returned " << nmea_returns[ret] << endl;

					if ((!use_fixed_position) && (nmea_client.GPS_Quality()))
					{
						position_event(pe, GOT_LIVE_DATA);
						md.save_gps_gga_rmc(db);
						nmea.GetData().m_type = NMEA_DATA::LiveDataFlag;

						if (bDebugging)
							cout << "NMEA:: " << nmea_returns[ret] << endl;

						ipc.IsValid(true);
						noGPSTimer.SetTime();	// got a valid GPS so reset the timer

						update_gps_LED_state(nmea_client, LEDState);

						int sog = (int)((nmea.GetData().sog / MS_TO_KNOTS) * MS_TO_KPH);

						send_redstone_ud_msg("avl-monitor", 0, "velocity %d %s\r", sog, speed_source.c_str());

						CheckSystemTime(nmea.GetData());

						if (bDebugging)
						{
							nmea.GetFullPosStr(stderr);
							cout << endl;
						}

						nEmptyCount = 0;
					}
					else
					{
						position_event(pe, LOST_LIVE_DATA);
						set_LEDState(4);
						get_fixed_or_historic_position(db, nmea, ipc, use_fixed_position);
					}

					if(bUseOBDSpeed)//Only handle OBD source
					{
						send_redstone_ud_msg("avl-monitor", 0, "velocity %d %s\r", m_RedStoneData.Speed(), speed_source.c_str());
					}

				}
				break;
			case	 NMEA_UPDATE_VALID_SOURCE:	// if there is more than one source this will tell which source to use - other sources are ignored.
				curSource = ipc.Source();
				break;
			case NMEA_UPDATE_POSN:	// data coming in from	GX55 etc.
				if (curSource == DS_ANY || curSource == ipc.Source()) // only use data from the current source
				{
					ipc.Data(ipc.InputData());
				}
				break;
			case	NMEA_EXIT:
				done = true;
				break;
			default:
				break;
		}
		ipc.Type(NMEA_UPDATE_NONE);	 // done with it

		if (theTimer.DiffTime() > 10)  // more than 10 seconds since any data on port
		{
			set_LEDState(5);
			ipc.IsValid(false);
			theTimer.SetTime();
		}
			
		if (LEDTimer.DiffTime() > 5)  // force regular update of time since GPS light can be turned off elsewhere
		{
			g_LEDState = 0;
			LEDTimer.SetTime();
		}

		if (noGPSTimer.DiffTime() > 300)  // no valid gps for 5 minutes.
		{
			switch (nGPSRestarts)
			{
				case 0:
					ats_logf(ATSLOG_DEBUG, "GPS Input failure- Issuing warm reset");
					system("/usr/bin/restart_gps");
					break;
				case 1:
					ats_logf(ATSLOG_DEBUG, "Restarting GPS");
					system("/usr/bin/restart_gps");
					break;
				default:
					break;
			}
			nGPSRestarts++;
			noGPSTimer.SetTime();
		}

		usleep(250000);
	}

	ipc.Remove();
}

