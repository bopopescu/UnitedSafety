#include "plcserialmonitorstatemachine.h"

#define STATE_MACHINE_CLASS PlcSerialMonitorSm
#define SET_NEXT_STATE(P_STATE) m_state_fn = &STATE_MACHINE_CLASS::state_ ## P_STATE; m_state = P_STATE

extern ATSLogger g_log;

COMMON_STATE_MACHINE_START_FUNCTION_DEFINITION(PlcSerialMonitorSm)
PlcSerialMonitorSm::PlcSerialMonitorSm(StateMachineData& pdata) : StateMachine(pdata)
{
	m_connected = false;
	m_spm.start();
}

PlcSerialMonitorSm::~PlcSerialMonitorSm()
{
}

bool PlcSerialMonitorSm::connectToMsgAssembler()
{
	return true;
}

void PlcSerialMonitorSm::state_0()
{
	ats_logf(&g_log,"PlcSerialMonitorSm enter state 0");
	m_data.clear();
	SET_NEXT_STATE(1);
}

void PlcSerialMonitorSm::state_1()
{
	ats_logf(&g_log,"PlcSerialMonitorSm enter state 1");
	while (1)
	{
		int res = m_spm.readMessage(m_data);
		if (res == 0)
		{
			ats_logf(&g_log, "Message read.\nData:%s", m_data.toHex().constData());
			break;
		}
		else
		{
			ats_logf(&g_log, "Error reading message %d", res);
			m_data.clear();
		}
		usleep(50000);
	}
	SET_NEXT_STATE(2);
}

void PlcSerialMonitorSm::state_2()
{
	ats_logf(&g_log,"PlcSerialMonitorSm enter state 2");
	if(!m_connected)
	{
		m_connected = connectToMsgAssembler();
		if(!m_connected)
		{
			ats_logf(&g_log,"%s,%d: Cannot connect to message assembler.", __FILE__, __LINE__);
			exit(1);
		}
	}
	send_redstone_ud_msg("message-assembler", 0, "msg calamp_user_msg usr_msg_data=%s\r", m_data.toHex().constData());
	SET_NEXT_STATE(3);
}

void PlcSerialMonitorSm::state_3()
{
	ats_logf(&g_log,"PlcSerialMonitorSm enter state 3");
	m_spm.sendAck();
	m_data.clear();
	SET_NEXT_STATE(1);
}

void PlcSerialMonitorSm::state_4()
{
	ats_logf(&g_log,"PlcSerialMonitorSm enter state 4");
}
