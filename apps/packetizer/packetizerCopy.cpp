#include "packetizerDB.h"
#include "packetizerCopy.h"

#define STATE_MACHINE_CLASS PacketizerCopy
#define SET_NEXT_STATE(P_STATE) m_state_fn = &STATE_MACHINE_CLASS::state_ ## P_STATE; m_state = P_STATE

COMMON_STATE_MACHINE_START_FUNCTION_DEFINITION(PacketizerCopy)

PacketizerCopy::PacketizerCopy(MyData& mdata): StateMachine(mdata),m_data(&mdata)
{
	MyData& md = dynamic_cast<MyData&>(my_data());
	dbreader = new PacketizerDB(md);
	dbreader->start();

	md.set_current_mid(dbreader->dbquerylastmid(MESSAGECENTERDB));
	SET_NEXT_STATE(0);
}

PacketizerCopy::~PacketizerCopy()
{
	delete dbreader;
}

void PacketizerCopy::state_0()
{
	MyData& md = dynamic_cast<MyData&>(my_data());
	md.testdata_log("PacketizerCopy enter state 0");
	for(;;)
	{
		if( md.get_current_mid() + 1 <=  dbreader->dbquerylastmid(MESSAGECENTERDB))
		{
			SET_NEXT_STATE(1);
			break;
		}
		else
			sleep(1);
	}

}

void PacketizerCopy::state_1()
{
	MyData& md = dynamic_cast<MyData&>(my_data());
	md.testdata_log("PacketizerCopy enter state 0");
	int mid = md.get_current_mid() + 1;
	dbreader->dbcopy(mid);
	md.set_current_mid(mid);
	SET_NEXT_STATE(0);
}
