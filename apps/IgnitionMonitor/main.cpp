// IgnitionMonitor
//
// Determines source of ignition information from the db-config database
// and sets the values for Ignition in the shared memory.
//
// Possible sources are:
// RedStone IgnitionSource OBD
// RedStone IgnitionSource GPIO
//	with RedStone IgnitionGPIO <port>
//

#include <stdio.h>
#include <sys/time.h>

#include "atslogger.h"
#include "AFS_Timer.h"
#include "NMEA_Client.h"
#include "RedStone_IPC.h"
#include "ConfigDB.h"
#include "socket_interface.h"

#include "WakeupMask.h"

#define IF_PRESENT(P_name, P_EXP) if(g_has_ ## P_name) {P_EXP;}

ATSLogger g_log;

const int g_power_monitor_port = 41009;

ClientSocket g_cs_powermonitor;


//--------------------------------------------------------------------------------------------------
int main(int argc, char **argv)
{
	WakeupMask wakeupMask;
	
	const ats::String app_name("IgnitionMonitor");
	g_log.open_testdata("IgnitionMonitor");
	ATSLogger::set_global_logger(&g_log);
	ats_logf(&g_log, "IgnitionMonitor startup");

	if(ats::testmode())
	{
		ats_logf(ATSLOG(0), "Device Test Mode detected, disabling Ignition-Monitor for this power-up/runtime");
		return 0;
	}

	ats_logf(&g_log, "WakeupMasks:");
	ats_logf(&g_log, wakeupMask.Dump().c_str());

	struct timespec ts;
	db_monitor::ConfigDB db;
	const int ignitionOBDTimeoutSec = 30; 
	const int keepAwakeSeconds = 60 * db.GetInt("RedStone", "KeepAwakeMinutes", 60); 

	REDSTONE_IPC redStoneData;
	NMEA_Client nmea;
	AFS_Timer waitForGPSTimer;
	ats::String strReason;
	

	//wait up to 30 seconds for GPS to come up - then just run regardless. This should eliminate most of the 0,0 positions.
	// ATS-FIXME
	// This should either use app-monitor and have NMEA signal when NMEA data is ready to be read or 
	// we should also expand this to use a last valid position - (maybe the last non-zero record in the message table?)
	// The Calamps protocol has flags for fix status that can be set to unknown (0,0) or last valid (from message table)
	//
	while (nmea.GetData().isValid() == false && waitForGPSTimer.DiffTime() < 30)
	{
		sleep(1);
	}
	
	if (nmea.GetData().isValid() == false)
		ats_logf(&g_log, "No GPS after %d seconds", waitForGPSTimer.DiffTime());
	else
		ats_logf(&g_log, "GPS came up after %d seconds", waitForGPSTimer.DiffTime());

	init_ClientSocket(&g_cs_powermonitor);

	if(connect_client(&g_cs_powermonitor, "127.0.0.1", g_power_monitor_port))
	{
		ats_logf(&g_log, "%s,%d:g_cs_powermonitor %s", __FILE__, __LINE__, g_cs_powermonitor.m_emsg);
		exit(1);
	}

	send_cmd(g_cs_powermonitor.m_fd, MSG_NOSIGNAL, "PowerMonitorStateMachine start\r");
	bool bIgnitionOn;
	// 292
	bool IgnitionShutdown;
	int nValidVolts = 0, nInvalidVolts = 0;

	bIgnitionOn = false;
	//292	
	IgnitionShutdown = false;
	redStoneData.IgnitionOn(false);
	ats_logf(&g_log, "Current Ignition Status now %s", redStoneData.IgnitionOn() ? "On" : "Off");
	int inp1Accum = 0;
	
	while (1)
	{
		strReason = "";
		bIgnitionOn = false;
		IgnitionShutdown = false;

		if (wakeupMask.Can())
		{
			clock_gettime(CLOCK_MONOTONIC, &ts);
			
			// look for OBD timeout - we have been on and the OBD hasn't updated in the timeout time.
			if ( redStoneData.IgnitionOn() && ( (ts.tv_sec - redStoneData.ObdLastUpdated()) > ignitionOBDTimeoutSec) )
			{
				ats_logf(&g_log, "OBD has timed out.");
				redStoneData.RPM(0);
			}

			if (redStoneData.RPM() > 0)
			{
				strReason += "RPM ";
				bIgnitionOn = true;
			}
		}
		
		// get the io status
		char buf[256];
		int fd, len;
		fd = open("/dev/set-gpio", O_RDONLY);
		len = read(fd, buf, 208);
		close(fd);
		buf[len] = '\0';

		if (wakeupMask.Inp1() && strchr(buf, 'Z'))
		{
			strReason += "IgnitionInput ";
			bIgnitionOn = true;
			// ISCP-292
			IgnitionShutdown = true;
		}

		
		if (wakeupMask.BattVolt())
		{
			if (redStoneData.BatteryVoltage() < wakeupMask.ShutdownVoltage())	// input voltage must go to Shutdown voltage to go 'off'
			{
				if (++nInvalidVolts < 10 * ignitionOBDTimeoutSec &&  redStoneData.IgnitionOn() == true)	// 30 seconds of readings below cutoff before we go to sleep
				{                                       																								// must already be On to be valid
					strReason += "Voltage ";
					bIgnitionOn = true;
				}
			}
			else 
			{
				nInvalidVolts = 0;
				strReason += "Voltage ";
				bIgnitionOn = true;
			}
		}

		if (bIgnitionOn && redStoneData.IgnitionOn() == false)	// turning on
		{
			ats_logf(&g_log, "Current Ignition Status - Turning ON - %s", strReason.c_str());
			send_cmd(g_cs_powermonitor.m_fd, MSG_NOSIGNAL, "unset_work key=KeepAwake\r", app_name.c_str());
			send_cmd(g_cs_powermonitor.m_fd, MSG_NOSIGNAL, "set_work key=%s\r", app_name.c_str());
			send_app_msg("IgnitionMonitor", "avl-monitor", 0, "ignition on \r");
			redStoneData.IgnitionOn(true);
		}
		if (!bIgnitionOn && redStoneData.IgnitionOn() == true)	// turning off
		{
			ats_logf(&g_log, "Current Ignition Status - Turning OFF");
			send_cmd(g_cs_powermonitor.m_fd, MSG_NOSIGNAL, "set_work key=KeepAwake expire=%d\r", keepAwakeSeconds);
			send_cmd(g_cs_powermonitor.m_fd, MSG_NOSIGNAL, "unset_work key=%s\r", app_name.c_str());
			send_app_msg("IgnitionMonitor", "avl-monitor", 0, "ignition off \r");
			redStoneData.IgnitionOn(false);
		}

		// 292 
		if (IgnitionShutdown && redStoneData.IgnitionShutdown() == false)	// turning on
		{
			redStoneData.IgnitionShutdown(true);
		}
		if (!IgnitionShutdown && redStoneData.IgnitionShutdown() == true)	// turning off
		{
			redStoneData.IgnitionShutdown(false);
		}

		usleep(100000);
	}

	return 0;
}

