//------------------------------------------------------------------------------------------
//
// PacketizerCopy - state machine to copy data to the iridium db
//
//  Startup: Create the db if it does not exist -> 0
//  0: If new message in messages.db
//  1: Copy message if it has a high enough priority -> 0, update last message looked at.
//
// NOTE: on start up we start from the end of the current messages table.

#include "packetizerIridiumDB.h"
#include "packetizerCopy.h"
#include "midDB.h"


#define STATE_MACHINE_CLASS PacketizerCopy
#define SET_NEXT_STATE(P_STATE) m_state_fn = &STATE_MACHINE_CLASS::state_ ## P_STATE; m_state = P_STATE
extern ATSLogger g_log;

COMMON_STATE_MACHINE_START_FUNCTION_DEFINITION(PacketizerCopy)

PacketizerCopy::PacketizerCopy(MyData& mdata): StateMachine(mdata),m_data(&mdata)
{
	MyData& md = dynamic_cast<MyData&>(my_data());
	dbreader = new PacketizerIridiumDB(md, "iridium_db", "/mnt/update/database/iridium.db");
	dbreader->start();
	md.set_current_mid(dbreader->Query_MostRecentMID(MESSAGECENTERDB));  // current mid will be newest record in message table
	SET_NEXT_STATE(0);
	set_log_level(11);  // turns off state machine logging
}

PacketizerCopy::~PacketizerCopy()
{
	delete dbreader;
}

// watch for new records.
void PacketizerCopy::state_0()
{
//	ats_logf(&g_log, "%s,%d: ENTER STATE 0", __FILE__, __LINE__);
	MyData& md = dynamic_cast<MyData&>(my_data());

	for(;;)
	{
		if( md.get_current_mid() + 1 <= dbreader->Query_MostRecentMID(MESSAGECENTERDB))  // is there a newer record
		{
			md.set_current_mid(md.get_current_mid() + 1);  // set whether message copied or not so that we only look at non-iridium priority messages once
			SET_NEXT_STATE(1);
			break;
		}
		else
		{
			sleep(1);
		}
	}
}

// Copy the message if it is high enough priority
void PacketizerCopy::state_1()
{
	MyData& md = dynamic_cast<MyData&>(my_data());
//	ats_logf(&g_log, "%s,%d: ENTER STATE 1 - copying record %d", __FILE__, __LINE__, md.get_current_mid());
	dbreader->dbcopy(md.get_current_mid());
	SET_NEXT_STATE(0);
}
