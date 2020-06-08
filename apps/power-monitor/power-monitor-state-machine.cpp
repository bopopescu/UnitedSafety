// Description: Power Monitor State Machine
//
// State Machine Version:
//		SVN 1300 - http://svn.atsplace.int/software-group/AbsoluteTrac/RedStone/Documents/Design/Power-Monitor-State-Machine.odg
//
// ATS FIXME: State machine does not match documentation. Update documentation.
//
// XXX: This code MUST match with the above state machine diagram. Do NOT modify this code without also
//			modifying the above state machine diagram. If this code and state machine diagram differ, then the
//			diagram shall be taken as correct.
//
//
//  Modifications for TL5000
//   The Low battery value is now using the wakeup voltage.  So we have 3 states depending on voltage
//			Running
//   Wakeup voltage
// 			waiting to run/ going to sleep (keep alive, message output pending, etc)
//   Critical Low Voltage
//		  will wake up once and send critical battery message then not come on again until voltage rises above wakeup voltage.
//
//


#include <fstream>
#include <vector>

#include <errno.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>

#include "atslogger.h"
#include "timer-event.h"
#include "state-machine-data.h"
#include "power-monitor.h"
#include "power-monitor-state-machine.h"
#include "ConfigDB.h"
#include "RedStone_IPC.h"
#include "AFS_Timer.h"
#include <sys/ioctl.h>
#include "linux/i2c-dev.h"


#define STATE_MACHINE_CLASS PowerMonitorStateMachine
#define SET_NEXT_STATE(P_STATE) m_state_fn = &STATE_MACHINE_CLASS::state_ ## P_STATE; m_state = P_STATE

COMMON_STATE_MACHINE_START_FUNCTION_DEFINITION(PowerMonitorStateMachine)

REDSTONE_IPC g_redStoneData;

ats::String strStateDesc;	// description of state for logging and stat command

static bool g_bSentInetNotification_lb = false;
static bool g_bSentInetNotification_cb = false;


//=======================================================================================================================================
PowerMonitorStateMachine::PowerMonitorStateMachine(StateMachineData& p_data) : StateMachine(p_data)
{
	MyData& md = *((MyData*)&(my_data()));
	md.m_command.insert(AdminCommandPair(__FUNCTION__, AdminCommand(ac_PowerMonitorStateMachine, *this)));

	m_immediate_shutdown_flag = false;
	m_force_shutdown_flag = false;

	// get the DisableSleep flag from the database
	db_monitor::ConfigDB db;
	m_bDisableSleep = false;  // modified to eliminate this option
	m_WakeupVoltage = 13.2;
	m_ShutdownVoltage = 12.8;
	m_CriticalVoltage = 11.8;

	m_KeepAwakeMinutes = db.GetInt("RedStone", "KeepAwakeMinutes", 60);
	m_WakeupVoltage = db.GetDouble("wakeup", "WakeupVoltage", m_WakeupVoltage * 1000) / 1000;
	m_ShutdownVoltage = db.GetDouble("wakeup", "ShutdownVoltage", m_ShutdownVoltage * 1000) / 1000;
	m_CriticalVoltage = db.GetDouble("wakeup", "CriticalVoltage", m_CriticalVoltage * 1000) / 1000;
	m_CountLowBattery = 0;
	m_IgnitionDelay = 120;// db.GetInt("power-monitor", "IgnitionDelay", 120); // no lower than 10 seconds
	m_bWokeOnCritBatt = false;
	m_firmwareUpdateTime = 0;
	SET_NEXT_STATE(0);
}

//=======================================================================================================================================
PowerMonitorStateMachine::~PowerMonitorStateMachine()
{
}

//=======================================================================================================================================
int PowerMonitorStateMachine::ac_PowerMonitorStateMachine(AdminCommandContext& p_acc, const AdminCommand& p_cmd, int p_argc, char* p_argv[])
{
	// ATS FIXME: This function (and similar functions) must be thread safe. Make sure to lock all non-function-local
	//	variables to prevent race conditions.
	PowerMonitorStateMachine& sm = *((PowerMonitorStateMachine*)p_cmd.m_state_machine);

	if(p_argc < 2)
	{
		return 0;
	}

	const ats::String cmd(p_argv[1]);

	if(g_dbg)
	{
		ats_logf(ATSLOG_DEBUG, "%s: Got command \"%s\"", __PRETTY_FUNCTION__, cmd.c_str());
	}

	if("shutdown" == cmd)
	{
		ats::StringMap s;
		s.from_args(p_argc - 1, p_argv + 1);

		sm.set_shutdown_flag(true);

		const bool force = s.get_int("force") ? true : false;

		if(force)
		{
			sm.set_force_shutdown_flag(force);
		}

		if(g_dbg)
		{
			ats_logf(ATSLOG_DEBUG, "%s: Shutdown request received, force_request=%s, force_state=%s",
				__PRETTY_FUNCTION__,
				force ? "true" : "false",
				sm.m_force_shutdown_flag ? "true" : "false");
		}

		sm.my_data().post_event("ShutdownEvent");
	}
	else if("stats" == cmd)
	{
		std::vector <ats::String> jobs;
		std::vector <int> expire;
		MyData& md = *((MyData*)&(sm.my_data()));
		md.list_jobs(jobs, expire);
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "debug=%d, state=%s, num_jobs=%lu\n", g_dbg, strStateDesc.c_str(), jobs.size());
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "WakeupVoltage=%.1f, ShutdownVoltage=%.1f, CurVoltage=%.2f\n", sm.m_WakeupVoltage, sm.m_ShutdownVoltage, g_redStoneData.BatteryVoltage());
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "KeepAwake=%d minutes\n", sm.m_KeepAwakeMinutes);

		if(sm.m_bDisableSleep)
			send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "Sleep has been disabled - system will stay ON!\n");
		
		for(size_t i = 0; i < jobs.size(); ++i)
		{

			if(expire[i] < 0)
			{
				send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "\t%zu: %s\n", i, jobs[i].c_str());
			}
			else
			{
				send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "\t%zu: %s, %d second(s) till expired\n", i, jobs[i].c_str(), expire[i]);
			}

		}

		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "\r");
	}
	else if ("start" == cmd)
	{
		ats_logf(ATSLOG_DEBUG, "%s: IgnitionMonitorStarted event sent.", __PRETTY_FUNCTION__);
		sm.my_data().post_event("IgnitionMonitorStartedEvent");
	}
	else
	{
		ats_logf(ATSLOG_ERROR, "%s,%d: Invalid %s command \"%s\"", __FILE__, __LINE__, __FUNCTION__, cmd.c_str());
	}

	return 0;
}

//=======================================================================================================================================
void PowerMonitorStateMachine::state_0()
{

	if(ats::testmode())
	{
		ats_logf(ATSLOG_DEBUG, "Device Test Mode detected, disabling Power-Monitor for this power-up/runtime");
		exit(0);
	}

	strStateDesc = "State 0 - Starting";
	ats_logf(ATSLOG_DEBUG, "%s (%.2f)", strStateDesc.c_str(), g_redStoneData.BatteryVoltage());
	ats_logf(ATSLOG_DEBUG, "	DisableSleep	(%s)", (m_bDisableSleep ? "On" : "Off"));
	ats_logf(ATSLOG_DEBUG, "	KeepAwakeMinutes (%d)", m_KeepAwakeMinutes);	
	ats_logf(ATSLOG_DEBUG, "	WakeupVoltage (%.1f)", m_WakeupVoltage);	

	if (ats::file_exists("/tmp/logprev/reboot.txt") )
	{
		ats::String strWakeup;

		if (ats::get_file_line(strWakeup, "/tmp/logdir/wakeup.txt", 1, 0) == 0)
		{

			if (strWakeup == "crit_batt")
			{
				m_bWokeOnCritBatt = true;
				SET_NEXT_STATE(2);
			}
			else
			{
				SET_NEXT_STATE(1);
			}

		}

	}
	else
	{
		SET_NEXT_STATE(7);
	}

}

//=======================================================================================================================================
void PowerMonitorStateMachine::state_1()
{
	strStateDesc = "State 1 - Checking for ignition";
	ats_logf(ATSLOG_DEBUG, "%s (%.2f)", strStateDesc.c_str(), g_redStoneData.BatteryVoltage());

	for (short i = 0; i < m_IgnitionDelay; i++)
	{
		ats_logf(ATSLOG_DEBUG, "%s (%.2f)", strStateDesc.c_str(), g_redStoneData.BatteryVoltage());
		
		if (g_redStoneData.BatteryVoltageValid())
		{
			if (g_redStoneData.IgnitionOn() || m_bDisableSleep)
			{
				SET_NEXT_STATE(2);
				return;
			}

			if (g_redStoneData.BatteryVoltageValid())	// battery reading is valid after 30 seconds to allow for settling - see battery-monitor
			{
				if (CheckForLowBattery())
				{
					SET_NEXT_STATE(8);
					return;
				}
			}
		}
		sleep(1);
	}

	SET_NEXT_STATE(2);	// #1982 - fixing so that heartbeat can keep the system alive.
}

//=======================================================================================================================================
void PowerMonitorStateMachine::state_2()
{
	strStateDesc = "State 2 - Waiting";
	ats_logf(ATSLOG_DEBUG, "%s (%.2f)", strStateDesc.c_str(), g_redStoneData.BatteryVoltage());
	MyData& md = *((MyData*)&(my_data()));

	for(;;)
	{
		EventListener listener(md);

		ShutdownEvent* shutdown = new ShutdownEvent();
		ats::TimerEvent* timer = new ats::TimerEvent(10);

		md.add_event_listener(shutdown, __PRETTY_FUNCTION__, &listener);
		md.add_event_listener(timer, __PRETTY_FUNCTION__, &listener);

		AppEventHandler e(listener.wait_event());

		if(e.m_event->m_reason.is_cancelled())
		{
			SET_TO_STATE_MACHINE_FAILURE;
			break;
		}
		else if(e.m_event == shutdown)
		{
			// immediate shutdown flag set
			ats_logf(ATSLOG_DEBUG, "%s,%d: Shutdown Event Recived:\n", __FILE__, __LINE__); 
			SET_NEXT_STATE(6);
			break;
		}
		else if(e.m_event == timer)
		{
			m_firmwareUpdateTime = g_redStoneData.GetFirmwareUpdateTime() - time(NULL);

			ats_logf(ATSLOG_DEBUG, "%s,%d:Firmware will Update After %ld Sec\n", __FILE__, __LINE__,m_firmwareUpdateTime);
			// waited 10 seconds - check for activity
			if( ( true == g_redStoneData.GetFirmwareUpdateStatus() ) && (0 > m_firmwareUpdateTime) )
			{ 
				ats_logf(ATSLOG_DEBUG, "%s,%d: Checking for new Firmware:\n", __FILE__, __LINE__); 
				SET_NEXT_STATE(5);
			}
			else
			{
			SET_NEXT_STATE(3);
			}
			break;
		}
	}
}


//=======================================================================================================================================
void PowerMonitorStateMachine::state_3()
{
	int version = 4;
	strStateDesc = "State 3 - Checking jobs";
	ats_logf(ATSLOG_DEBUG, "%s (%.2f) (%d)", strStateDesc.c_str(), g_redStoneData.BatteryVoltage(), m_CountLowBattery);
	ats_logf(ATSLOG_DEBUG, "\r\nPower-monitor - versrion = %d \r\n",version); 

	MyData& md = *((MyData*)&(my_data()));

	if (g_redStoneData.BatteryVoltage() < m_ShutdownVoltage)	//check for low battery
	{
 		if (++m_CountLowBattery >= 3)
		{
			if (!g_bSentInetNotification_lb) //<ISCP-164>
			{
				ats_logf(ATSLOG_DEBUG, "Power-monitor sending low battery warning\r");
				g_bSentInetNotification_lb = true;
				AFS_Timer t;
				t.SetTime();
				std::string user_data = "1000," + t.GetTimestampWithOS() + ", Low Battery Warning";
				user_data = ats::to_hex(user_data);

				send_redstone_ud_msg("message-assembler", 0, "msg inet_error msg_priority=9 usr_msg_data=%s\r", user_data.c_str());
			}
			if (CheckForLowBattery())
			{
				SET_NEXT_STATE(8);
				return;
			}
			else
				m_CountLowBattery = 0;
		}
	}
	else
	{
		m_CountLowBattery = 0;
		m_bWokeOnCritBatt = false;  // if we have voltage we should be fine
	}
	
	ats_logf(ATSLOG_DEBUG, "\r\ng_redStoneData.IgnitionOn =  %d \r\n",g_redStoneData.IgnitionOn()); 

	


	// Initiating shutdown if ignition is off.
	// 292
	if(!g_redStoneData.IgnitionShutdown())
	{
		// ISCP-292 
		// We will first read the wakeup mask and disable waking up from battery voltage

		// get device file name 
		ats_logf(ATSLOG_DEBUG, "Ignition called\n"); 
		const char* dev_fname = "/dev/i2c-0";
		const int dev_addr = 0x30;
		const int fd = open(dev_fname, O_RDWR);
		// check if opened successfully 
		if(fd < 0)
		{
			ats_logf(ATSLOG_DEBUG, "\r\n Failed to open device\r\n"); 
			return;
		}
		if(ioctl(fd, I2C_SLAVE, dev_addr))
		{
			ats_logf(ATSLOG_DEBUG, "\r\n Failed to open  salve\r\n"); 			
		}
		if(ioctl(fd, I2C_SLAVE, dev_addr))
		{
			ats_logf(ATSLOG_DEBUG, "\r\n Failed to open  salve\r\n"); 			
		}

		// Read current wakup make
		const int current_wakeup = i2c_smbus_read_byte_data(fd, 0x15) & 0xff;		
		static const int g_batt_volt_bit = 0x40;
		static const int g_low_batt_bit = 0x80;
		// assigne wake up 
		int wakeup = current_wakeup;
		// clear battery voltage wake up
		wakeup = wakeup & (~g_batt_volt_bit);
		// clear low battery wake up 
		wakeup = wakeup & (~g_low_batt_bit);
		// Set wakeup mask		
		i2c_smbus_write_byte_data(fd, 0x20, wakeup);
		// Proceed to Shutdown
		md.set_shutdown_flag();
		SET_NEXT_STATE(4);
	}
	else if((m_force_shutdown_flag) )
	{
		ats_logf(ATSLOG_DEBUG, "%s,%d: Force Shutdown Called:", __FILE__, __LINE__); 
		SET_NEXT_STATE(6);
	}
	else if (md.number_of_jobs_remaining() || m_bDisableSleep || 	m_bWokeOnCritBatt)
	{
		SET_NEXT_STATE(2);
	}
	else
	{
		ats_logf(ATSLOG_DEBUG, "%s,%d: Else statment executed:", __FILE__, __LINE__); 
		md.set_shutdown_flag();
		SET_NEXT_STATE(6);
	}
}

//=======================================================================================================================================
void PowerMonitorStateMachine::state_4()
{
	strStateDesc = "State 4 - KeepAwake";
	ats_logf(ATSLOG_DEBUG, "%s (%.2f)", strStateDesc.c_str(), g_redStoneData.BatteryVoltage());
	
	// Handle KeepAwakeMinutes here. ISCP-292
	AFS_Timer timer, timer20seconds;
	timer.SetTime();
	timer20seconds.SetTime();
	int secs = m_KeepAwakeMinutes * 60;
	int timeOut = 0;
	bool isOneMinuteTimeOutTrue = false; //ISCP-292
	MyData& md = *((MyData*)&(my_data()));
	static  bool isShutdownMessageSent = false;

	for(;;)
	{

		
		if (timer20seconds.DiffTime() >= 20)
		{
			timer20seconds.SetTime();
			
			if (g_redStoneData.BatteryVoltage() < m_ShutdownVoltage)	//check for low battery
			{
				if (++m_CountLowBattery == 3)
				{
					if (!g_bSentInetNotification_lb) //<ISCP-164>
					{
						ats_logf(ATSLOG_DEBUG, "Power-monitor sending low battery warning 2\r");
						g_bSentInetNotification_lb = true;
						AFS_Timer t;
						t.SetTime();
						std::string user_data = "1000," + t.GetTimestampWithOS() + ", Low Battery Warning";
						user_data = ats::to_hex(user_data);

						send_redstone_ud_msg("message-assembler", 0, "msg inet_error msg_priority=9 usr_msg_data=%s\r", user_data.c_str());
					}
					if (CheckForLowBattery())
					{
						SET_NEXT_STATE(8);
						return;
					}
				}
			}
			else
				m_CountLowBattery = 0;
			// Add delay of one minute before testing the Ignitoin input //ISCP-292
           if ((isOneMinuteTimeOutTrue == false) && (++timeOut == 3) )
           {
           		isOneMinuteTimeOutTrue = true;
           }

		}

//isc-162		
		if (timer.DiffTime() > secs  && (isShutdownMessageSent == false))
		{
			g_redStoneData.SetInstrumentShutdownStatus(true);
                        g_redStoneData.SetInstrumentShutdownMessageStatus(false);
			isShutdownMessageSent = true;
		}
		if (timer.DiffTime() > (secs + 300*6) || (g_redStoneData.GetInstrumentShutdownMessageStatus() == true))//instrument will shutdown after getting the signal from Cell or Satellite OR if wait of 30 min ends
		{	
			SET_NEXT_STATE(6) ;
			return;
		}
		//ISCP-292
		if ((g_redStoneData.IgnitionShutdown() == true) && (isOneMinuteTimeOutTrue == true))
		{
			ats_logf(ATSLOG_DEBUG, "Moving to state 2");
			SET_NEXT_STATE(2);
			//reset flag
			isShutdownMessageSent = false;
			break;
		}

		sleep(1);
	}

}

//=======================================================================================================================================
void PowerMonitorStateMachine::state_5()
{
	strStateDesc = "State 5 - Checking for update";
	ats_logf(ATSLOG_DEBUG, "%s (%.2f)", strStateDesc.c_str(), g_redStoneData.BatteryVoltage());
	
	const int pid = fork();

	if(!pid)
	{
		const char* app = "/usr/bin/check-update";
		execl(app, app, NULL);
		ats_logf(ATSLOG_DEBUG, "%s,%d: execl failed: (%d) %s", __FILE__, __LINE__, errno, strerror(errno));
		exit(1);
	}
	else if(pid < 0)
	{
		ats_logf(ATSLOG_DEBUG, "%s,%d: fork() failed: (%d) %s: Not performing autoupdate at shutdown", __FILE__, __LINE__, errno, strerror(errno));
	}

	int status;
	const int ret = waitpid(pid, &status, 0);

	if(ret < 0)
	{
		ats_logf(ATSLOG_DEBUG, "%s,%d: waitpid(%d) failed: (%d) %s", __FILE__, __LINE__, pid, errno, strerror(errno));
	}
	else
	{

		if(WIFSIGNALED(ret))
		{
			ats_logf(ATSLOG_DEBUG, "%s,%d: autoupdate received abnormal signal (%d)", __FILE__, __LINE__, WTERMSIG(ret));
		}
		else if(WIFSTOPPED(ret))
		{
			ats_logf(ATSLOG_DEBUG, "%s,%d: autoupdate received abnormal stop signal (%d)", __FILE__, __LINE__, WSTOPSIG(ret));
		}
		else if(WIFEXITED(ret))
		{
			ats_logf(ATSLOG_DEBUG, "%s,%d: autoupdate child exited with a problem (check autoupdate logs)", __FILE__, __LINE__);
		}

	}

	SET_NEXT_STATE(6);
}

//=======================================================================================================================================
// state 6 : Shutting down
void PowerMonitorStateMachine::state_6()
{
	ats_logf(ATSLOG_DEBUG, "%s,%d: Shutdown state active\n", __FILE__, __LINE__);
	//ISCP-209 Changes
	// do a reboot if there is an update pending.  Otherwise just shut down.
	if( ( true == g_redStoneData.GetFirmwareUpdateStatus() ) && (0 > m_firmwareUpdateTime) )
	{
		//No other option reboot system
		if(ats::file_exists("/mnt/update/update-ready-to-run") )
		{
			MyData& md = *((MyData*)&(my_data()));
			md.set_shutdown_flag();
			sleep(10);
			ats::system(ats::String("sh /home/root/on_shutdown.sh"));
			send_redstone_ud_msg("isc-lens", 0, "shutdown\r");
			g_redStoneData.SetFirmwareUpdateStatus(false);
			ats_logf(ATSLOG_DEBUG, "%s,%d: ISCP- 209 Reboot For Firmware Update\n", __FILE__, __LINE__);
			sleep(30);  // allow isc-lens time to send out its shutdown.
			ats::system("reboot");
			ats::infinite_sleep();
		}
		else
		{
			ats_logf(ATSLOG_DEBUG, "%s,%d: ISCP- 209 Firmware Update started but Firm File Downloadded Failed\n", __FILE__, __LINE__);
			//it seems firmware update is not schedule from Aware Check after 10 minutes
			g_redStoneData.SetFirmwareUpdateTime( time(NULL) + 1800 );
			SET_NEXT_STATE(2);
		}
		//ats::system( ats::String(ats::file_exists("/mnt/update/update-ready-to-run") ? "reboot" : "go-to-sleep;"));
	}
	else
	{
	strStateDesc = "State 6 - Shutting Down";
		ats_logf(ATSLOG_DEBUG, "%s (%.2f)", strStateDesc.c_str(), g_redStoneData.BatteryVoltage());

	ats::system(ats::String("sh /home/root/on_shutdown.sh"));
	send_redstone_ud_msg("isc-lens", 0, "shutdown\r");
	sleep(30);  // allow isc-lens time to send out its shutdown.
		ats::system("go-to-sleep");
		ats::infinite_sleep();
	}

	//ats::system( ats::String(ats::file_exists("/mnt/update/update-ready-to-run") ? "reboot" : "go-to-sleep;"));	
}

//=======================================================================================================================================
// state 7 - New power keep awake - stays here for 5 minutes.
void PowerMonitorStateMachine::state_7()
{
	strStateDesc = "State 7 - New Power Keep Awake";
	ats_logf(ATSLOG_DEBUG, "%s (%.2f)", strStateDesc.c_str(), g_redStoneData.BatteryVoltage());

	sleep(300);
	SET_NEXT_STATE(1);
}

// ----------------------------------------------------------------------------------------------
// state 8: low battery - leave after message clears or forced shutdown.
//
void PowerMonitorStateMachine::state_8()
{
	strStateDesc = "State 8 - Low Battery";
	ats_logf(ATSLOG_ERROR, "%s (%.2f)", strStateDesc.c_str(), g_redStoneData.BatteryVoltage());
	ats::system(ats::String("sh /home/root/on_shutdown.sh"));
	MyData& md = *((MyData*)&(my_data()));

	if (!g_bSentInetNotification_cb) //<ISCP-164>
	{
		ats_logf(ATSLOG_DEBUG, "Power-monitor sending critical battery warning\r");
		g_bSentInetNotification_cb = true;
		AFS_Timer t;
		t.SetTime();
		std::string user_data = "1016," + t.GetTimestampWithOS() + ", Critical Battery Warning";
		user_data = ats::to_hex(user_data);

		send_redstone_ud_msg("message-assembler", 0, "msg inet_error msg_priority=9 usr_msg_data=%s\r", user_data.c_str());
	}

	while (1)
	{
		if(m_force_shutdown_flag)
		{
			SET_NEXT_STATE(6);
			return;
		}
		else if (md.number_of_priority_jobs_remaining())
		{
			sleep(10);
		}
		else
		{
			// ATS::FIXME - should actually command rtc-monitor to reset its timers via socket
			ats::system("killall rtc-monitor");
			ats::system("db-config unset rtc-monitor"); 

			SET_NEXT_STATE(6);
			return;
		}
	}
}


//=======================================================================================================================================
bool PowerMonitorStateMachine::CheckForLowBattery()
{

	

	if (g_redStoneData.BatteryVoltage() > m_CriticalVoltage )	//check for low battery
		return false;
		
	if (g_redStoneData.BatteryVoltage() < m_CriticalVoltage)	//check for critical battery
	{
		send_redstone_ud_msg("message-assembler", 0, "msg crit_batt set_work_priority=2\r");
		ats_logf(ATSLOG_ERROR,"Power-monitor - sent critical battery. Battery voltage: %f", g_redStoneData.BatteryVoltage());
		if (!g_bSentInetNotification_cb) //<ISCP-164>
		{
		
				ats_logf(ATSLOG_DEBUG, "Power-monitor sending critical battery warning\r");
				g_bSentInetNotification_cb = true;
				AFS_Timer t;
				t.SetTime();
				std::string user_data = "1016," + t.GetTimestampWithOS() + ", Critical Battery Warning";
				user_data = ats::to_hex(user_data);

				send_redstone_ud_msg("message-assembler", 0, "msg inet_error msg_priority=9 usr_msg_data=%s\r", user_data.c_str());
		}		
		system("i2cset -y 0 0x30 0x29 0xD1");  // tell the micro to turn off all the rails
		system("i2cset -y 0 0x30 0x26 0x01");  // tell the micro not to wake up on crit batt check because message was sent.
		sleep(5);
	}

	return true;
}

