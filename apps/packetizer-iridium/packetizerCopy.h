#pragma once

#include "ats-common.h"
#include "state_machine.h"

#include "packetizerDB.h"

typedef enum {
    MSG_PRIORITY_IRIDIUM_CELL = 1,
    MSG_PRIORITY_IRIDIUM      = 2,
    MSG_PRIORITY_CELL         = 3,
    MSG_PRIORITY_WIFI         = 4

} MSG_PRIORITY_LEVELS;

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

	PacketizerDB * dbreader;
	MyData* m_data;
};
