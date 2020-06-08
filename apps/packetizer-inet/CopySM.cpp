#include <unistd.h>
#include "InetDB.h"
#include "CopySM.h"
#include "CThreadWait.h"

#define STATE_MACHINE_CLASS CopySM
#define SET_NEXT_STATE(P_STATE) m_state_fn = &STATE_MACHINE_CLASS::state_ ## P_STATE; m_state = P_STATE
extern CThreadWait g_TWaitForNewRecord;

COMMON_STATE_MACHINE_START_FUNCTION_DEFINITION(CopySM)

CopySM::CopySM(MyData& mdata): StateMachine(mdata),m_data(&mdata)
{
	set_log_level(11);  // turns off state machine logging

	MyData& md = dynamic_cast<MyData&>(my_data());
	m_pInetDB = new InetDB(md, "inet_db", "/mnt/update/database/inet.db");
	m_pInetDB->start();
	
	md.set_current_mid(m_pInetDB->dbquerylastmid(MESSAGECENTERDB));
	SET_NEXT_STATE(0);
}

CopySM::~CopySM()
{
	delete m_pInetDB;
}

void CopySM::state_0()
{
	MyData& md = dynamic_cast<MyData&>(my_data());
	//ats_logf(ATSLOG_INFO, "%s,%d: ENTER STATE 0", __FILE__, __LINE__);
	for(;;)
	{
		int next_mid = m_pInetDB->dbquerynextmid_from_messagedb(md.get_current_mid());
		if( md.get_current_mid() < next_mid )
		{
			md.set_current_mid(next_mid);
			SET_NEXT_STATE(1);
			break;
		}
		else
			sleep(1);
	}

}

void CopySM::state_1()
{
	MyData& md = dynamic_cast<MyData&>(my_data());
	//ats_logf(ATSLOG_INFO, "%s,%d: ENTER STATE 1 - copying %d", __FILE__, __LINE__, md.get_current_mid());
	m_pInetDB->CopyRecordFromMessageTable(md.get_current_mid());
	g_TWaitForNewRecord.Signal();  // signal main state machine that there is a new record to process.
	SET_NEXT_STATE(0);
}
