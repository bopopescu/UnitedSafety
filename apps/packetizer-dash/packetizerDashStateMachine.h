#pragma once

#include "ats-common.h"
#include "state_machine.h"
#include "midDB.h"
#include "packetizer.h"
#include "RedStone_IPC.h"

#include "packetizerDashDB.h"
#include "packetizerDashSender.h"

class PacketizerDashStateMachine : public StateMachine
{
public:
	PacketizerDashStateMachine(MyData &);
	virtual ~PacketizerDashStateMachine();
	virtual void start();
private:
	void (PacketizerDashStateMachine::*m_state_fn)();
	void state_0();
	void state_1();
	void state_2();
	void state_3();
	void state_4();
	void state_5();
	void state_6();

	void sendMessage();
	bool readFromDashDB(int mid);
	void waitForNextMessage();
	PacketizerDashDB *m_dbreader;
	PacketizerDashSender *m_sender;

	struct send_message m_msg;
	uint32_t sequence_num;

	REDSTONE_IPC m_redstone;
	midDB m_mDB;
};
