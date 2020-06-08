#pragma once

#include "ats-common.h"
#include "state_machine.h"

#include "packetizerDB.h"
#include "packetizerSender.h"  // for TRAK_MESSAGE_TYPE definitions.
#include "packetizerMessage.h"
#include "NMEA_DATA.h"
#include "FTDebug.h"
#include "FTData_J1939.h"
#include "FTData_Speeding.h"
#include "midDB.h"

class PacketizerSm : public StateMachine
{
public:
  NMEA_DATA m_nmea;
  FTDebug m_FTDev;
	TRAK_MESSAGE_TYPE m_MsgCode;
	PacketizerDB * dbreader;
  int m_CurMID;
  short m_SeatBeltOn;
  string m_UserData;
  bool m_bUsingRealTime;

	midDB m_mdb;

public:
	PacketizerSm(MyData& );
	virtual~ PacketizerSm();

	virtual void start();

protected:
	void readfromDB(int, const char* table="message_table");

private:
	void (PacketizerSm::*m_state_fn)();
	void state_0();  // get data
	void state_1();  // send to AFF_Email
	void state_2();  // remove the record from the DB
};
