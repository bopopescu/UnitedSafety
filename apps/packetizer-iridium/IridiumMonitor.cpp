#include "packetizerIridiumDB.h"
#include "IridiumMonitor.h"
#include "ConfigDB.h"

#define STATE_MACHINE_CLASS IridiumMonitor
#define SET_NEXT_STATE(P_STATE) m_state_fn = &STATE_MACHINE_CLASS::state_ ## P_STATE; m_state = P_STATE
extern ATSLogger g_log;

COMMON_STATE_MACHINE_START_FUNCTION_DEFINITION(IridiumMonitor)

IridiumMonitor::IridiumMonitor(MyData& mdata): StateMachine(mdata),m_data(&mdata)
{
	m_RedStoneData.IridiumEnabled(false);
	db_monitor::ConfigDB dbconf();
	
 	db_monitor::ConfigDB db;
	m_IridiumTimeout = db.GetInt("packetizer-iridium", "IridiumTimeout", 300); // defaults to 5 minutes

	ats_logf(&g_log, "Config:	Timeout %d\n", m_IridiumTimeout);

	ats::String value;
	m_LastMID = db.GetInt("packetizer-iridium", "LastMID", 0);
	ats_logf(&g_log, "         LastMID %d\n", m_LastMID);
	
	m_RedStoneData.IridiumEnabled(false);
	m_RedStoneData.LastSendFailed(true); // Iridum should come up enabled - this won't get set if no packetizer is running

	SET_NEXT_STATE(0);
}

IridiumMonitor::~IridiumMonitor()
{
}

void IridiumMonitor::state_0()	// disabled - looking to enable
{
	ats_logf(&g_log, "%s,%d: ENTER STATE 0", __FILE__, __LINE__);
	
	for (;;)
	{

		if (m_RedStoneData.LastSendFailed() &&
			m_RedStoneData.GetLastFailTimer()->DiffTime() > m_IridiumTimeout)
		{
			m_RedStoneData.IridiumEnabled(true);
			SET_NEXT_STATE(1);
			return;
		}
		sleep(1);
	}
}

void IridiumMonitor::state_1() // enabled - looking to disable
{
	ats_logf(&g_log, "%s,%d: ENTER STATE 1", __FILE__, __LINE__);

	for (;;)
	{
		if (!m_RedStoneData.LastSendFailed())
		{
			m_RedStoneData.IridiumEnabled(false);
			SET_NEXT_STATE(0);
			return;
		}
		sleep(1);
	}
}

