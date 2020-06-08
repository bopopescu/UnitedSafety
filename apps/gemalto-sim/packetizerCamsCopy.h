#pragma once
#include "ats-common.h"
#include "state_machine.h"
#include "packetizer.h"

#include "packetizerCamsDB.h"

class PacketizerCamsCopy: public StateMachine
{
public:
	PacketizerCamsCopy(MyData&);
	~PacketizerCamsCopy();
	virtual void start();

private:
	void (PacketizerCamsCopy::*m_state_fn)();
	void state_0();
	void state_1();

	PacketizerCamsDB* m_dbreader;
	MyData* m_data;
};

