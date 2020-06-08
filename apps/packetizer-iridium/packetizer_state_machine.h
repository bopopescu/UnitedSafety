#pragma once

#include "ats-common.h"
#include "state_machine.h"

#include "packetizerDB.h"
#include "packetizerIridiumSender.h"
#include "packetizerIridiumMessage.h"
#include "RedStone_IPC.h"

class PacketizerSm : public StateMachine
{
public:
	PacketizerSm(MyData& );
	virtual~ PacketizerSm();

	virtual void start();

protected:
	bool readfromPacketizerDB(int);

private:
	void (PacketizerSm::*m_state_fn)();
	void state_0();
	void state_1();
	void state_2();
	void state_3();
	void state_4();
	void state_5();
	void state_6();
	void state_7();

	void send_message();

	PacketizerDB * dbreader;
	PacketizerIridiumSender * m_sender;
	PacketizerIridiumMessage *m_msg;
	uint32_t sequence_num;
	REDSTONE_IPC m_RedStone;
	ats::String m_imei;
};
