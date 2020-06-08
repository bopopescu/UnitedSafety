#pragma once

#include "ats-common.h"
#include "state_machine.h"
#include "RedStone_IPC.h"
#include "midDB.h"

#include "packetizerDB.h"
#include "packetizerSender.h"
#include "packetizerMessage.h"

class PacketizerSm : public StateMachine
{
public:
	PacketizerSm(MyData& );
	virtual~ PacketizerSm();

	virtual void start();

protected:
	bool readfromCantelDB(int);

private:
	void (PacketizerSm::*m_state_fn)();
	void state_0();
	void state_1();
	void state_2();
	void state_3();
	void state_4();
	void state_5();
	void state_6();

	void send_message();

	PacketizerDB * dbreader;
	PacketizerSender * sender;

	struct send_message m_msg;
	uint32_t sequence_num;

	REDSTONE_IPC m_RedStone;
	midDB m_mdb;
};
