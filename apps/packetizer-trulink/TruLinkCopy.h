#pragma once
#include "ats-common.h"
#include "state_machine.h"
#include "packetizer.h"

#include "TruLinkDB.h"

class TruLinkCopy: public StateMachine
{
public:
	TruLinkCopy(MyData&);
	~TruLinkCopy();
	virtual void start();

private:
	void (TruLinkCopy::*m_state_fn)();
	void state_0();
	void state_1();

	TruLinkDB* m_dbreader;
	MyData* m_data;
};

