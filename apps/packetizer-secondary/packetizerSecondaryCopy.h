#pragma once
#include "ats-common.h"
#include "state_machine.h"
#include "packetizer.h"

#include "packetizerSecondaryDB.h"

class PacketizerSecondaryCopy: public StateMachine
{
public:
    PacketizerSecondaryCopy(MyData&);
    ~PacketizerSecondaryCopy();
	virtual void start();

private:
    void (PacketizerSecondaryCopy::*m_state_fn)();
	void state_0();
	void state_1();

    PacketizerSecondaryDB* m_dbreader;
	MyData* m_data;
};

