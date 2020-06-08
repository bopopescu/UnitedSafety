#pragma once

#include "ats-common.h"
#include "state_machine.h"

#include "packetizerDashDB.h"

class PacketizerCopy: public StateMachine
{
public:
	PacketizerCopy(MyData&);
	~PacketizerCopy();

	virtual void start();
private:

	void (PacketizerCopy::*m_state_fn)();
	void state_0();
	void state_1();

	PacketizerDashDB * m_dbreader;
	MyData* m_data;
};
