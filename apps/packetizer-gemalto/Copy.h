#pragma once

#include "ats-common.h"
#include "state_machine.h"

#include "DB.h"

class Copy: public StateMachine
{
public:
	Copy(MyData&);
	~Copy();

	virtual void start();
private:

	void (Copy::*m_state_fn)();
	void state_0();
	void state_1();

	DB * m_dbreader;
	MyData* m_data;
};
