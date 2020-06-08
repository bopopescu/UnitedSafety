#pragma once

#include "ats-common.h"
#include "state_machine.h"
#include "midDB.h"
#include "packetizer.h"
#include "RedStone_IPC.h"

#include "packetizerSecondaryDB.h"
#include "packetizerSecondarySender.h"

class PacketizerSecondaryStateMachine : public StateMachine
{
private:
	std::string m_PrimaryIP;
	std::string m_IMEI;
	int m_PrimaryPort;
	int m_RouteIP;
	int m_cell_retries;
	int m_retry_limit;
	PacketizerSecondarySender *m_pSecondarySender;
public:
	PacketizerSecondaryStateMachine(MyData &);
	virtual ~PacketizerSecondaryStateMachine();
	virtual void start();
private:
	void (PacketizerSecondaryStateMachine::*m_state_fn)();
	void state_0();
	void state_1();
	void state_2();
	void state_3();
	void state_4();

	void ConnectToPrimary();

	void sendMessage();
	bool readFromSecondaryDB(int mid);
	void waitForNextMessage();
	PacketizerSecondaryDB *m_dbreader;

	struct send_message m_msg;
	uint32_t sequence_num;

	REDSTONE_IPC m_redstone;
	midDB m_mDB;
};
