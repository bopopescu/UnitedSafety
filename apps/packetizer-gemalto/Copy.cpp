#include "Copy.h"

#define STATE_MACHINE_CLASS Copy
#define SET_NEXT_STATE(P_STATE) m_state_fn = &STATE_MACHINE_CLASS::state_ ## P_STATE; m_state = P_STATE

COMMON_STATE_MACHINE_START_FUNCTION_DEFINITION(Copy)

Copy::Copy(MyData& mdata): StateMachine(mdata),m_data(&mdata)
{
	MyData& md = dynamic_cast<MyData&>(my_data());
	m_dbreader = new DB(md, TRULINK_DB_NAME, TRULINK_DB_FILE);  
	m_dbreader->start();

	md.set_current_mid(m_dbreader->dbquerylastmid(MESSAGECENTERDB));
	SET_NEXT_STATE(0);
}

Copy::~Copy()
{
	delete m_dbreader;
}

void Copy::state_0()
{
	MyData& md = dynamic_cast<MyData&>(my_data());

	for(;;)
	{
		int next_mid = m_dbreader->dbquerynextmid_from_messagedb(md.get_current_mid());
		if( md.get_current_mid() < next_mid )
		{
			md.set_current_mid(next_mid);
			SET_NEXT_STATE(1);
			break;
		}
		else
			sleep(1);
	}

}

void Copy::state_1()
{
	MyData& md = dynamic_cast<MyData&>(my_data());
	m_dbreader->dbcopy(md.get_current_mid());
	SET_NEXT_STATE(0);
}
