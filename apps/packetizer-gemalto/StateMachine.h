#pragma once

#include "ats-common.h"
#include "state_machine.h"
#include "midDB.h"
#include "packetizer.h"
#include "RedStone_IPC.h"

#include "DB.h"
#include "Sender.h"

class GemaltoStateMachine : public StateMachine
{
public:
	GemaltoStateMachine(MyData &);
	virtual ~GemaltoStateMachine();
	virtual void start();
private:
	void (GemaltoStateMachine::*m_state_fn)();
	void state_0();
	void state_1();
	void state_2();
	void state_3();
	void state_4();
	void state_5();
	void state_6();

	void sendMessage();
	bool readFromDB(int mid);
	void waitForNextMessage();
	DB *m_dbreader;
	Sender *m_sender;

	struct send_message m_msg;
	uint32_t sequence_num;

	REDSTONE_IPC m_redstone;
	midDB m_mDB;
};
