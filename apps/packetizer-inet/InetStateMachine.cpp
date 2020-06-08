#include <fstream>
#include <iomanip>
#include "atslogger.h"
#include "ConfigDB.h"
//#include "IridiumUtil.h"

#include "packetizer.h"
#include "InetStateMachine.h"
#include <colordef.h>
#include "CThreadWait.h"
#include <INetConfig.h>
#include <INET_IPC.h>
#include "RedStone_IPC.h"
REDSTONE_IPC g_redStoneData;

extern INET_IPC g_INetIPC;

int msgType = 1;
extern CThreadWait g_TWaitForNewRecord;
extern INetConfig * g_pConfig;
int version = 92;

#define MAX_COUNT_FOR_NETWORK_ERROR 5
#define STATE_MACHINE_CLASS InetStateMachine
#define SET_NEXT_STATE(P_STATE) m_state_fn = &STATE_MACHINE_CLASS::state_ ## P_STATE; m_state = P_STATE;
#define ATS_STATE_TRACE {if (m_bStateTrace)ats_logf(ATSLOG_INFO, "%s", __FUNCTION__);}
enum
{
  SM_ATSLOG_ERROR,
  SM_ATSLOG_DEBUG,
  SM_ATSLOG_TRACE = 10
};

COMMON_STATE_MACHINE_START_FUNCTION_DEFINITION(InetStateMachine)
bool g_bIsShuttingDown;  // used to monitor if gateway needs to be updated to shut down due to low battery.
static int failCount = 0;

//---------------------------------------------------------------------------------------
InetStateMachine::InetStateMachine(MyData& p_data) : StateMachine(p_data), sequence_num(0)
{
	ats_logf(ATSLOG_ERROR, "%s,%d: CONSTRUCTOR", __FILE__, __LINE__);
	//pthread_mutex_init(&m_priorityOneMutex, NULL);
	MyData& md = dynamic_cast<MyData&>(my_data());
	m_pInetDB = new InetDB(md, "inet_db", "/mnt/update/database/inet.db");
	m_cell_sender = new CellSender(md);
	m_sequence_num = 0;
	m_pInetDB->start();
	m_pInetDB->CleanupDB();
	m_DBCleanupTimer.SetTime();
	g_bIsShuttingDown = false;
	m_IridiumStartHasBeenSent = false;  // 'start' gets sent every time we start up.
	m_cell_retries = 0;

	SET_NEXT_STATE(0);
}


//---------------------------------------------------------------------------------------
// state_0 - read config parameters (1)
void InetStateMachine::state_0()
{
printf("%s:%d\n", __FILE__, __LINE__);
	ATS_STATE_TRACE;
	set_log_level(11);  // turns off state machine logging

	db_monitor::ConfigDB db;

	//Initialize state machine variables
	m_retry_limit = 5;
//	m_iridium_timeout_seconds = db.GetInt("packetizer-inet", "iridium_timeout", 60);
//	m_IridiumPriorityLevel = 10;
	m_ForceIridium = db.GetValue("packetizer-inet", "ForceIridium", "Never");  // test iridium without checking cellular
	m_IridiumEnable = db.GetBool("packetizer-inet", "IridiumEnable", "Off"); // Get iridium enable flag from packetizer-inet
	m_bPS19Test = db.GetBool("isc-lens", "PS19Test", false);
	if (m_ForceIridium == "On")
	{
		ats_logf(ATSLOG_ERROR,"%s, %d: WARNING: Iridium feature is Forced ON - check ForceIridium setting.", __FILE__, __LINE__);
		ATS_TRACE;
		m_FakeIridium = true;
		//g_INetIPC.UsingIridium(true);  // always true if forcing iridium.
	}
	else
		m_FakeIridium = false;

	if (m_bPS19Test)
		//g_INetIPC.UsingIridium(true);  // always true if running the PS19 test.

	m_KeepINetAliveTimer.SetTime();
	m_bStateTrace = db.GetBool("packetizer-inet", "StateTrace", false);

	// validate the Iridium - feature-iridium-monitor takes priority over packetizer-inet-IridiumEnable  Both these exist so you can 
	// turn off the inet sending to Iridium but leave it enabled for other packetizers.

	if(m_IridiumEnable)
	{
		if (!db.GetBool("feature","iridium-monitor", "Off"))
		{
			ats_logf(ATSLOG_ERROR,"%s, %d: WARNING: Iridium feature is turned off, and IridiumEnable is turned on in packetizer. Disabling Iridium for packetizer.", __FILE__, __LINE__);
			m_IridiumEnable = false;
		}
		else
		{
			m_iridium_util = new IridiumUtil();
		}
	}
//	m_PriorityIridiumDataLimit = db.GetInt("packetizer-inet", "IridiumDataLimitPriority", 1);
//	m_imei = db.GetValue("RedStone", "IMEI");
//	m_IridiumIMEI = db.GetValue("Iridium", "IMEI");
	//m_CellFailMode = false;
	
	m_ESITimer.SetTime();
	m_AuthRequestTimer.SetTime();
//	m_pInetDB->LoadMessageTypes(m_messagetypes);
	set_log_level(SM_ATSLOG_TRACE);

	SET_NEXT_STATE(1);
printf("%s:%d\n", __FILE__, __LINE__);
}
//---------------------------------------------------------------------------------------
// state_1 - send start via cell (2/7)
void InetStateMachine::state_1()
{
printf("%s:%d\n", __FILE__, __LINE__);

	ATS_STATE_TRACE;

	if (m_FakeIridium)
	{
		SET_NEXT_STATE(7);
		return;
	}
	if (IsInternetAvailable())
	{
		if (m_cell_sender->start())  // this will get the auth token and set up the gateway.
		{
			m_cell_retries = 0; // cell access has worked so reset count.
			SET_NEXT_STATE(2);		
		}
		else
		{
			SET_NEXT_STATE(9);
		}
	}
	else
	{
		SET_NEXT_STATE(7);  // off to the iridium side.
	}
printf("%s:%d\n", __FILE__, __LINE__);
}
//---------------------------------------------------------------------------------------
// state_2 - check for priority 1 (3/4)
void InetStateMachine::state_2()
{
printf("%s:%d\n", __FILE__, __LINE__);
	ATS_STATE_TRACE;
	int mid = 0;
	ats_logf(ATSLOG_INFO, BLUE_ON "Version : %d\r\n" RESET_COLOR,version);// hamza
	ats_logf(ATSLOG_INFO,"Sate 2- mid = %d:\r\n",mid);// hamza
	if(m_msgInfo.mid > 0)
	{
		ats_logf(ATSLOG_INFO, "State 2 - Step 1\r\n");// hamza
		SET_NEXT_STATE(3);
	}	
	else
	{
		mid = m_pInetDB->SelectSinglePriorityOneMessage();
		if(mid > 0)
		{
			m_msgInfo.mid = mid;
			m_msgInfo.pri = 1;
			m_msgInfo.seq = inc_sequence_num();
			ats_logf(ATSLOG_DEBUG, "%s,%d,: found Priority 1 mid %d", __FILE__, __LINE__, mid);
			SET_NEXT_STATE(3);
		}
		else
		{
			SET_NEXT_STATE(4);  // read a regular message
		}
	}
	//298
	if (m_cell_retries > m_retry_limit)
		m_IridiumSender.GetResponseIridium(m_iridium_util);

	
}
//---------------------------------------------------------------------------------------
// state_3 - send message (2/11)
void InetStateMachine::state_3()
{
	int ret = -1;
printf("%s:%d\n", __FILE__, __LINE__);
	ATS_STATE_TRACE;
	// read the message
	if(!m_pInetDB->dbquery_from_packetizerdb(m_msgInfo.mid, m_msgInfo.sm))
	{
		ats_logf(ATSLOG_DEBUG, "%s, %d: Cannot read message from database (mid = %d). Dropping message.", __FILE__,__LINE__, m_msgInfo.mid);
		RemoveRecord();
		SET_NEXT_STATE(5); // check timers for sending other infrequent messages.
	}
	// send the message
//IRIDIUM TEST
	if (m_bPS19Test)
	{
		ats_logf(ATSLOG_INFO, "%s,%d: %s:      Iridium Test: Sending message via iridium", __FILE__, __LINE__, __FUNCTION__);
		m_IridiumSender.SetData(m_msgInfo.sm);
		m_IridiumSender.sendSingleMessage(&m_msgInfo, m_iridium_util);
		RemoveRecord();
		SET_NEXT_STATE(2);
	}

	if (m_FakeIridium)
	{
		SET_NEXT_STATE(14);
		return;
	}
	
	ret = m_cell_sender->sendSingleMessage(&m_msgInfo); // ISCP-339
	if (ret >= 0) 
	{

		if(ret == 0)// ISCP-339
	{
		RegisterNetworkError(false);
		m_cell_retries = 0; // cell access has worked so reset count.
		failCount = 0; // reset for network error log

		}
		RemoveRecord();
		SET_NEXT_STATE(2);
	}
	else
	{
		SET_NEXT_STATE(11);
	}
}
//---------------------------------------------------------------------------------------
// state_4 - check for regular messages (3/5)
void InetStateMachine::state_4()
{
		ATS_STATE_TRACE;
	if(m_pInetDB->dbqueryhighestprimid_from_packetizerdb((size_t &)m_msgInfo.mid, (size_t &)m_msgInfo.pri)) // checks for a record
	{
		m_msgInfo.seq = inc_sequence_num();
		SET_NEXT_STATE(3);
	}
	else
	{
		SET_NEXT_STATE(5);
	}
}
//---------------------------------------------------------------------------------------
// state_5 -  sleep (waiting on messages) (6)
// NOTE: check for new messages every second so we don't delay sending new messages.
void InetStateMachine::state_5()
{
	ATS_STATE_TRACE;
	sleep(1);
	SET_NEXT_STATE(6);
}
//---------------------------------------------------------------------------------------
// state_6 - check timers (2)
void InetStateMachine::state_6()
{
	ATS_STATE_TRACE;
	if (m_DBCleanupTimer.DiffTime() > (1800) )
	{
		clearMsgInfo(m_msgInfo);
		m_pInetDB->CleanupDB();
		m_DBCleanupTimer.SetTime();
	}
	SET_NEXT_STATE(17); // check timers for a whole bunch of stuff
}
//---------------------------------------------------------------------------------------
// state_7 - check internet (8/9)
void InetStateMachine::state_7()
{
	ATS_STATE_TRACE
	if (m_FakeIridium)
	{
		SET_NEXT_STATE(9);
		return;
	}
	if (IsInternetAvailable())
	{
		m_cell_retries = 0;  // if we have internet (ping google) then we don't switch to iridium
		//g_INetIPC.UsingIridium(false);
		SET_NEXT_STATE(8);
	}
	else
	{
		SET_NEXT_STATE(9);
	}
}
//---------------------------------------------------------------------------------------
// state_8 - internet is ok so sleep then try again (1)
// NOTE - sleep is longer because the failure is likely not talking to iNet and we don't
// want to repeatedly pound on the server
void InetStateMachine::state_8()
{
	ATS_STATE_TRACE
	sleep(30);
	SET_NEXT_STATE(1);
}
//---------------------------------------------------------------------------------------
// state_9 - No internet - check failure count > min (10/8)
// NOTE: this is for sending 'start'
void InetStateMachine::state_9()
{
	ATS_STATE_TRACE;
	if (m_FakeIridium)
	{
		SET_NEXT_STATE(10);
		return;
	}

	if (++m_cell_retries > m_retry_limit)
	{
		SET_NEXT_STATE(10);
	}
	else
	{
		SET_NEXT_STATE(8);
	}
}
//---------------------------------------------------------------------------------------
// state_10 - Then send 'start' via Iridium (2/1)
void InetStateMachine::state_10()
{
	ATS_STATE_TRACE;
	if (m_IridiumStartHasBeenSent || SendIridiumStart()) // successfully sent start via iridium
	{
		ATS_TRACE;
		m_IridiumStartHasBeenSent = true;
		//g_INetIPC.UsingIridium(true);
		SET_NEXT_STATE(2);
	}
	else // failed - sleep and try again.  Each retry should take longer if we are failing to push to Iridium
	{
		ATS_TRACE;
		// either 30 seconds time num tries or every 10 minutes if tried 10 times
		//sleep (30 * ((m_cell_retries  < 10) ? m_cell_retries : 20));
		sleep (30 *  ((m_cell_retries < 2) ? m_cell_retries : 2));
		SET_NEXT_STATE(1);
	}
}
//---------------------------------------------------------------------------------------
// state_11 - message failed - check internet (12/13)
void InetStateMachine::state_11()
{
	ATS_STATE_TRACE;
	if (m_FakeIridium)
	{
		SET_NEXT_STATE(13);
		return;
	}

	if (IsInternetAvailable())
	{
		m_cell_retries = 0;  // if we have internet (ping google) then we don't switch to iridium
		SET_NEXT_STATE(12);
	}
	else
	{
		SET_NEXT_STATE(13);
	}
}
//---------------------------------------------------------------------------------------
// state_12 - internet is ok so sleep then try again (2)
// NOTE - sleep is longer because the failure is likely not talking to iNet and we don't
// want to repeatedly pound on the server
void InetStateMachine::state_12()
{
	ATS_STATE_TRACE;
	sleep(30);
	SET_NEXT_STATE(2);
}
//---------------------------------------------------------------------------------------
// state_13 - No internet - check failure count > min (14/2)
void InetStateMachine::state_13()
{
	ATS_STATE_TRACE
	if (++m_cell_retries > m_retry_limit)
	{
		SET_NEXT_STATE(14);
	}
	else 
	{
//		// either 30 seconds time num tries or every 10 minutes if tried 10 times
//		sleep (30 * ((m_cell_retries < 10) ? m_cell_retries : 20));
		sleep(10);//ISCP-339
		SET_NEXT_STATE(2);
	}
}
//---------------------------------------------------------------------------------------
// state_14 - send message using Iridium (15/2)
void InetStateMachine::state_14()
{
	ATS_STATE_TRACE;
	if (!m_IridiumStartHasBeenSent)  // haven't sent the start yet
	{
		SET_NEXT_STATE(16);
		return;
	}

	if (m_IridiumSender.sendSingleMessage(&m_msgInfo, m_iridium_util))
	{
		RegisterNetworkError(false);
		failCount = 0; // reset for network error log
		
		SET_NEXT_STATE(15);  // success - go delete the message
		sleep (5); // ISCP-339
		for (short i =0;i<5;i++)
		{
			m_IridiumSender.GetResponseIridium(m_iridium_util);
			sleep(2);
		}
	}
	else  // failed to send via Iridium so make each retry longer.
	{

		if(++failCount > MAX_COUNT_FOR_NETWORK_ERROR)
		{
			RegisterNetworkError(true);
		}

		// either 30 seconds time num tries or every 10 minutes if tried 10 times
		//int numSleeps = ((m_cell_retries < 10) ? m_cell_retries : 20);
		int numSleeps = ((m_cell_retries < 2) ? m_cell_retries : 2);
		for (short i = 0; i < numSleeps; i++)
		{
			sleep (30);

			if (IsInternetAvailable())  // we are back to cell so send stuff!
			{
				SET_NEXT_STATE(2);
				return;  // leave immediately!
			}
		}
		SET_NEXT_STATE(2);  // try to send it again
	}
}
//---------------------------------------------------------------------------------------
// state_15 -  sent message via Iridium (2) (delete sent record and move on)
void InetStateMachine::state_15()
{
	ATS_STATE_TRACE;
	RemoveRecord();
	SET_NEXT_STATE(2);
}


//---------------------------------------------------------------------------------------
// state_16 -  Send the iridium 'start' (after initial cell connection fails - 'start' has
// already been sent via the cell and we don't want to resend it that way.
void InetStateMachine::state_16()
{
	ATS_STATE_TRACE;
	if (m_IridiumStartHasBeenSent || SendIridiumStart()) // successfully sent start via iridium
	{
		ATS_TRACE;
		m_IridiumStartHasBeenSent = true;
		SET_NEXT_STATE(2);
	}
	else // failed - sleep and try again.  Each retry should take longer if we are failing to push to Iridium
	{
		ATS_TRACE;
		// either 30 seconds time num tries or every 10 minutes if tried 10 times		
		sleep (30 *  ((m_cell_retries < 4) ? m_cell_retries : 4));//ISCP-339
		SET_NEXT_STATE(2);
	}
}

// check timers for sending other infrequent messages.
//
void InetStateMachine::state_17()
{
	ATS_STATE_TRACE
	int cellGatewayupdate = -1;

	if (m_cell_retries > m_retry_limit)
	{
		// check if it is time to send the UpdateGateway message
		if (m_SatUpdateTimer.DiffTime() > (g_pConfig->SatKeepAliveMinutes() * 60))
		{
			if (m_IridiumSender.SendiNetUpdateGateway(m_iridium_util))
			{
				m_SatUpdateTimer.SetTime();
			}
			else 
			{
				SET_NEXT_STATE(11); // Check internet status.
				return;
			}
			
			g_INetIPC.SetLensLED();  // update the LEDS to keep them in sync.
		}
		if(g_redStoneData.GetInstrumentShutdownStatus() == true)
		{
			g_INetIPC.RunState(SHUTDOWN);
		}
		// check if a shutdown is occuring due to low battery 
		if (g_bIsShuttingDown && g_INetIPC.RunState() != SHUTDOWN)//isc-162
		{
			g_bIsShuttingDown = false;
		}
		else if (g_INetIPC.RunState() == SHUTDOWN &&  !g_bIsShuttingDown)
		{
			static int errorCount = 0;
			if (m_IridiumSender.SendiNetUpdateGateway(m_iridium_util) == true)
			{
				g_bIsShuttingDown = true;
				//Check if Instrument Shutdown status is being sent. 
				if ((g_redStoneData.GetInstrumentShutdownMessageStatus() == false) && (g_redStoneData.GetInstrumentShutdownStatus() == true))
				{
					g_redStoneData.SetInstrumentShutdownMessageStatus(true);
				}
			} 
			else 
			{
				if (++errorCount > 3)
				{
					if ((g_redStoneData.GetInstrumentShutdownMessageStatus() == false) && (g_redStoneData.GetInstrumentShutdownStatus() == true))
					{
						g_redStoneData.SetInstrumentShutdownMessageStatus(true);
					}
				}
			}

		}
		SET_NEXT_STATE(2); // start over.
		return;
	}
	if (m_KeepINetAliveTimer.DiffTime() > (g_pConfig->InetKeepAliveMinutes() * 60))
	{
		cellGatewayupdate = m_cell_sender->inetUpdateGateway();
		if (cellGatewayupdate == 0)
		{
			m_KeepINetAliveTimer.SetTime();
		}
//243
		else if(cellGatewayupdate == -2)
		{
			SET_NEXT_STATE(13); // Check internet status.
			return;
		}
		else 
		{
			SET_NEXT_STATE(11); // Check internet status.
			return;
		}
			
		g_INetIPC.SetLensLED();  // update the LEDS to keep them in sync.
	}
	
	
	if(IsInternetAvailable()) // ISCP-339
	{
	// Get a new Auth token before the last one expires (they last 1 hour)
	if (m_AuthRequestTimer.DiffTime() > 3500)
	{
		if (m_cell_sender->inetGetAuthentication() == 0) // get new authentication
			m_AuthRequestTimer.SetTime();
		else
		{
			SET_NEXT_STATE(11); // Check internet status.
			return;
		}
	}
	
	// Send the Exchange Status Information message every 15 minutes. 
	// this may trigger the Download Settings call if the time value for settings has changed.
	if (m_ESITimer.DiffTime() > 15 * 60) // every 15 minutes
	{
		m_ESITimer.SetTime();
		m_cell_sender->inetExchangeStatusInformation();
		m_cell_sender->inetDownloadEventSchedules();
	}
	
	}
	else // ISCP-339
	{
		SET_NEXT_STATE(11); // Check internet status.
		return;

	}
    if(g_redStoneData.GetInstrumentShutdownStatus() == true)//ISC-162
    {
    	g_INetIPC.RunState(SHUTDOWN);
    }
	
	// check if a shutdown is occuring due to low battery 
	if (g_bIsShuttingDown && g_INetIPC.RunState() != SHUTDOWN)
	{
		g_bIsShuttingDown = false;
	}
	else if (g_INetIPC.RunState() == SHUTDOWN &&  !g_bIsShuttingDown)
	{
		g_bIsShuttingDown = true;
		//m_cell_sender->inetUpdateGateway();  // should send out a SHUTDOWN message
		if (m_cell_sender->inetUpdateGateway() == 0)// should send out a SHUTDOWN message
		{
			//Check if Instrument Shutdown status is being sent. 
			if ((g_redStoneData.GetInstrumentShutdownMessageStatus() == false) && (g_redStoneData.GetInstrumentShutdownStatus() == true))
			{
				g_redStoneData.SetInstrumentShutdownMessageStatus(true);
			}
		}
	}
	SET_NEXT_STATE(2); // start over.
}
//---------------------------------------------------------------------------------------
// Send the 'start' for Iridium - which is just the UpdateGateway
// Returns: true if message sent. false if message fails to send.
bool InetStateMachine::SendIridiumStart()
{
	ats_logf(ATSLOG_DEBUG, "%s,%d: SendIridiumStart - switching to Iridium!", __FILE__, __LINE__);
	bool ret;

	ret = m_IridiumSender.SendiNetCreateGateway(m_iridium_util);

	if (ret)
		m_SatUpdateTimer.SetTime();
	
	g_INetIPC.SwitchedToIridium(true);// force isc-lens to regenerate CreateInstrument message for all attached VPRO.

	return ret;
}



void InetStateMachine::RemoveRecord()
{	
	SocketError err;
	ats_logf(ATSLOG_ERROR, "%s,%d: unset_work key=Message_%d", __FILE__, __LINE__,m_msgInfo.sm.get_int("mtid"));
	send_msg("localhost", 41009, &err, "unset_work key=Message_%d\r",m_msgInfo.sm.get_int("mtid"));	

	m_pInetDB->dbrecordremove(m_msgInfo.mid);
	clearMsgInfo(m_msgInfo);
}

 void InetStateMachine::RegisterNetworkError(bool setError)
 {
 	AFS_Timer t;
	std::string user_data;
	static bool NetworkError = false;
	
	if(setError)
	{
		if(!NetworkError)
		{
			t.SetTime();
			user_data = "977," + t.GetTimestampWithOS() + ", TGX Network Error";
			ats_logf( ATSLOG_DEBUG, RED_ON "%s,%d: : TGX Network Error Logged:%s\n" RESET_COLOR,__FILE__, __LINE__, user_data.c_str() );
			user_data = ats::to_hex(user_data);
			send_redstone_ud_msg("message-assembler", 0, "msg inet_error msg_priority=9 usr_msg_data=%s\r", user_data.c_str());
			NetworkError = true;
		}
	}
	else
	{
		if(NetworkError)
		{
			t.SetTime();
			user_data = "976," + t.GetTimestampWithOS() + ", TGX Network Restored";
			ats_logf( ATSLOG_DEBUG, RED_ON "%s,%d: : TGX Network Restored Info Logged:%s\n" RESET_COLOR,__FILE__, __LINE__, user_data.c_str() );
			user_data = ats::to_hex(user_data);
			send_redstone_ud_msg("message-assembler", 0, "msg inet_error msg_priority=9 usr_msg_data=%s\r", user_data.c_str());
			NetworkError = false;
		}
	}
 }
     