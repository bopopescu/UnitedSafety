#pragma once
#include "ats-common.h"
#include "state_machine.h"
#include "packetizer.h"

#include "InetDB.h"

class CopySM: public StateMachine
{
public:
	CopySM(MyData&);
	~CopySM();
	virtual void start();

private:
	void (CopySM::*m_state_fn)();
	void state_0();
	void state_1();

	InetDB* m_pInetDB;
	MyData* m_data;
};

