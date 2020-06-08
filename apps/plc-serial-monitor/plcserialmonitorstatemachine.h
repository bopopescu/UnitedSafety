#pragma once

#include "ats-common.h"
#include "state-machine-data.h"
#include "state_machine.h"
#include "serialportmanager.h"
#include "socket_interface.h"

class PlcSerialMonitorSm : public StateMachine
{
public:
	PlcSerialMonitorSm(StateMachineData& );
	virtual~ PlcSerialMonitorSm();
	virtual void start();

private:
	void (PlcSerialMonitorSm::*m_state_fn)();
	void state_0();
	void state_1();
	void state_2();
	void state_3();
	void state_4();

	void send_message();
	bool connectToMsgAssembler();

	SerialPortManager m_spm;
	QByteArray m_data;
	bool m_connected;
};
