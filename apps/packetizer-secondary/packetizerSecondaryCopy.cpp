#include "packetizerSecondaryDB.h"
#include "packetizerSecondaryCopy.h"

#define STATE_MACHINE_CLASS PacketizerSecondaryCopy
#define SET_NEXT_STATE(P_STATE) m_state_fn = &STATE_MACHINE_CLASS::state_ ## P_STATE; m_state = P_STATE

COMMON_STATE_MACHINE_START_FUNCTION_DEFINITION(PacketizerSecondaryCopy)

PacketizerSecondaryCopy::PacketizerSecondaryCopy(MyData& mdata): StateMachine(mdata),m_data(&mdata)
{
	MyData& md = dynamic_cast<MyData&>(my_data());
    m_dbreader = new PacketizerSecondaryDB(md, "secondary_db", "/mnt/update/database/secondary.db");
	m_dbreader->start();

	md.set_current_mid(m_dbreader->dbquerylastmid(MESSAGECENTERDB));
	SET_NEXT_STATE(0);
}

PacketizerSecondaryCopy::~PacketizerSecondaryCopy()
{
	delete m_dbreader;
}

void PacketizerSecondaryCopy::state_0()
{
	MyData& md = dynamic_cast<MyData&>(my_data());
	ats_logf(ATSLOG(1), "%s,%d: ENTER STATE 0", __FILE__, __LINE__);
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

void PacketizerSecondaryCopy::state_1()
{
	MyData& md = dynamic_cast<MyData&>(my_data());
	ats_logf(ATSLOG(1), "%s,%d: ENTER STATE 1", __FILE__, __LINE__);
	m_dbreader->dbcopy(md.get_current_mid());
	SET_NEXT_STATE(0);
}
