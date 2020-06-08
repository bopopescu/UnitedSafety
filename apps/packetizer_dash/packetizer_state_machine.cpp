#include <vector>
#include <sstream>
#include <iterator>

#include <syslog.h>
#include <stdlib.h>

#include "packetizer.h"
#include "packetizerDB.h"
#include "packetizer_state_machine.h"
#include <geoconst.h>
#include <RedStone_IPC.h>

#define STATE_MACHINE_CLASS PacketizerSm
#define SET_NEXT_STATE(P_STATE) m_state_fn = &STATE_MACHINE_CLASS::state_ ## P_STATE; m_state = P_STATE

#define REALTIME "realtime_table"

extern const ats::String g_DBcolumnname[];
extern ATSLogger g_log;
int g_CurMTID; // current records MTID for sending to mid.db for iridium tracking

REDSTONE_IPC g_RedStone;

COMMON_STATE_MACHINE_START_FUNCTION_DEFINITION(PacketizerSm)

PacketizerSm::PacketizerSm(MyData& p_data) : StateMachine( p_data)
{
	MyData& md = dynamic_cast<MyData&>(my_data());
	dbreader = new PacketizerDB(md);

	dbreader->start();

	md.set_current_mid(dbreader->dbquerylastmid(MESSAGECENTERDB));
	SET_NEXT_STATE(0);
}

PacketizerSm::~PacketizerSm()
{
	delete dbreader;
}

void PacketizerSm::readfromDB(int mid, const char* table)
{
	ats::StringMap sm;
	std::vector<char> data;
	dbreader->dbquery_from_canteldb(mid, sm, table);
	PacketizerMessage msg(sm);

	m_MsgCode = (TRAK_MESSAGE_TYPE)sm.get_int(g_DBcolumnname[14]);

  m_nmea.SetTimeFromDateTimeString( sm.get(g_DBcolumnname[2]).c_str() );
	m_nmea.ddLat = sm.get_double(g_DBcolumnname[4]);
	m_nmea.ddLon = sm.get_double(g_DBcolumnname[5]);
	m_nmea.H = sm.get_double(g_DBcolumnname[6]);

	m_nmea.ddCOG = sm.get_double(g_DBcolumnname[8]);
	m_nmea.sog = (sm.get_double(g_DBcolumnname[7]) / MS_TO_KPH) * MS_TO_KNOTS;
	m_nmea.hdop = sm.get_double(g_DBcolumnname[11]);
	m_nmea.num_svs = sm.get_double(g_DBcolumnname[9]);
  m_nmea.gps_quality = sm.get_double(g_DBcolumnname[10]);
  m_SeatBeltOn = sm.get_int(g_DBcolumnname[12]);
  m_UserData = sm.get(g_DBcolumnname[33]);
}

void PacketizerSm::state_0()
{
	ats_logf(&g_log, "enter state 0");

	for(;;)
	{
    if (g_RedStone.SendingEmail())
      sleep(1);
    else
    {
		 	if((m_CurMID = dbreader->dbquerylastmid(CANTELDB, REALTIME)) > 0)  // check for realtime message
		 	{
			  readfromDB(m_CurMID, REALTIME);
			  m_bUsingRealTime = true;
  			SET_NEXT_STATE(1);
			  break;
		 	}
  		m_CurMID = dbreader->dbqueryoldestmid(CANTELDB);

	  	if(m_CurMID)
		  {
			  readfromDB(m_CurMID);
  			SET_NEXT_STATE(1);
			  break;
		  }
		  else
		  {
			  sleep(1);
		  }
    }
	}
}

// State 1 - we have data - now put it into NMEA and use the type to determine which message to send.
void PacketizerSm::state_1()
{
	ats_logf(&g_log, "enter state 1");
  ats::String strType;

  switch (m_MsgCode)
  {
    case TRAK_SCHEDULED_MSG:    //        1,
      m_FTDev.SendDebug(m_nmea, FTData_Debug::ID_PositionUpdate, FTData_Debug::PU_TIME, 0);
      g_RedStone.SendingEmail(true);
      strType = "Scheduled msg";
			break;
		case TRAK_HEARTBEAT_MSG:    //        8,
      m_FTDev.SendHeartbeat(m_nmea);
      g_RedStone.SendingEmail(true);
      strType = "Heartbeat";
			break;

		case TRAK_SPEED_EXCEEDED_MSG:    //   2,
      strType = "Speed exceeded";
      m_FTDev.SendSpeeding(m_nmea, m_UserData);
      g_RedStone.SendingEmail(true);
			break;
		case TRAK_PING_MSG:    //             3,
		case TRAK_STOP_COND_MSG:    //        4,
      strType = "Stopped";
      m_FTDev.SendStopped(m_nmea);
      g_RedStone.SendingEmail(true);
			break;
		case TRAK_START_COND_MSG:    //       5,
      strType = "Start";
      m_FTDev.SendStarted(m_nmea);
      g_RedStone.SendingEmail(true);
			break;
		case TRAK_IGNITION_ON_MSG:    //      6,
      strType = "Ignition On";
      m_FTDev.SendIgnitionOn(m_nmea);
      g_RedStone.SendingEmail(true);
			break;
		case TRAK_IGNITION_OFF_MSG:    //     7,
      strType = "Ignition Off";
      m_FTDev.SendIgnitionOff(m_nmea);
      g_RedStone.SendingEmail(true);
			break;
		case TRAK_SENSOR_MSG:    //           10,
      strType = "SeatBelt";
      
      if (m_SeatBeltOn)
        m_FTDev.SendSeatbeltOn(m_nmea);
      else
        m_FTDev.SendSeatbeltOff(m_nmea);
      g_RedStone.SendingEmail(true);
      break;
    case TRAK_LOW_BATTERY_MSG:
    case TRAK_CRITICAL_BATTERY_MSG:
      strType = "Low Battery";
      m_FTDev.SendLowBattery(m_nmea);
      g_RedStone.SendingEmail(true);
      break;
    case 	TRAK_J1939_MSG:
      strType = "J1939 Status";
      m_FTDev.SendJ1939Status(m_nmea, m_UserData);
      g_RedStone.SendingEmail(true);
      break;
    case 	TRAK_J1939_FAULT_MSG:
      strType = "J1939 Fault";
      m_FTDev.SendJ1939Fault(m_nmea, m_UserData);
      g_RedStone.SendingEmail(true);
      break;
    case TRAK_J1939_STATUS2_MSG:
      strType = "J1939 Status";
      m_FTDev.SendJ1939Status2(m_nmea, m_UserData);
      g_RedStone.SendingEmail(true);
      break;
		case TRAK_ACCEPTABLE_SPEED_MSG:    // 12,
      strType = "Speed_OK";
      m_FTDev.SendSpeedOK(m_nmea);
      g_RedStone.SendingEmail(true);
      break;

		case TRAK_POWER_ON_MSG:    //         11,
		case TRAK_TEXT_MSG:    //             13,
		case TRAK_DIRECTION_CHANGE_MSG:    // 14,
		case TRAK_ACCELERATION_MSG:    //     15,
		case TRAK_HARD_BRAKE_MSG:    //       16,
		case TRAK_SOS_MSG:    //              19,
		case TRAK_HELP_MSG:    //             20,
		case TRAK_OK_MSG:    //               21,
		case TRAK_POWER_OFF_MSG:    //        23,
		case TRAK_CHECK_IN_MSG:    //         24,
		case TRAK_FALL_DETECTED_MSG:    //    25,
		case TRAK_CHECK_OUT_MSG:    //        26,
		case TRAK_NOT_CHECK_IN_MSG:    //     27,
		case TRAK_GPSFIX_INVALID_MSG:    //   30,
		case TRAK_FUEL_LOG_MSG:    //         31,
		case TRAK_DRIVER_STATUS_MSG:    //    32,
		case TRAK_ENGINE_ON_MSG:    //            33,
		case TRAK_ENGINE_OFF_MSG:    //           34,
		case TRAK_ENGINE_TROUBLE_CODE_MSG:     // 35,
		case TRAK_ENGINE_PARAM_EXCEED_MSG:     // 36,
		case TRAK_ENGINE_PERIOD_REPORT_MSG:    // 37,
		case TRAK_OTHER_MSG:                   // 38,
		case TRAK_SWITCH_INT_POWER_MSG:        // 39,
		case TRAK_SWITCH_WIRED_POWER_MSG:      // 40,
		case TRAK_ODOMETER_UPDATE_MSG:         // 41,
		case TRAK_ACCEPT_ACCEL_RESUMED_MSG:    // 42,
		case TRAK_ACCEPT_DECCEL_RESUMED_MSG:   // 43,
		case TRAK_ENGINE_PARAM_NORMAL_MSG:     // 44
		case TRAK_CALAMP_USER_MSG:
      strType = "Ignoring " + m_MsgCode ;
      m_FTDev.SendUnhandledMessage(m_nmea, m_MsgCode);
      g_RedStone.SendingEmail(true);
			break;
  }
	ats_logf(&g_log, "State 2- msg: %s",strType.c_str());

	SET_NEXT_STATE(2);
}

void PacketizerSm::state_2()
{
	ats_logf(&g_log, "enter state 2");
	g_RedStone.FailedToSend();

	for(;;)
	{
    if (g_RedStone.SendingEmail())
      sleep(1);
    else
      break;
  }

	if (m_bUsingRealTime)
	{
		dbreader->DeleteAllRecords(REALTIME);
		ats_logf(&g_log, "Sent Real Time Message");
		m_bUsingRealTime = false;
	}
	else
	{
		dbreader->dbrecordremove(m_CurMID);
		ats_logf(&g_log, "Remove message %d from dashboard_db", m_CurMID);
		m_mdb.SetLatestPacketizerMID(g_CurMTID);
	}

	g_RedStone.LastSendFailed(false);  // notify iridium side that we are still sending OK
	
	SET_NEXT_STATE(0);
}


