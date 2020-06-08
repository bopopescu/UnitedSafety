#pragma once
#include <pthread.h>
#include <semaphore.h>
#include <ats-string.h>
#include <AFS_Timer.h>

#include "state_machine.h"
#include "IridiumUtil.h"
#include "InetDB.h"
#include "CellSender.h"
#include "InetIridiumSender.h"

class InetStateMachine : public StateMachine
{
public:
	InetStateMachine(MyData&);
	void start();
	bool readfromPacketizerDB(int p_mid, struct send_message& p_msg);

	void RegisterNetworkError(bool);
	void RegisterNetworkRestored(bool);

private:

	void (InetStateMachine::*m_state_fn)();
	void state_0();
	void state_1();
	void state_2();
	void state_3();
	void state_4();
	void state_5();
	void state_6();
	void state_7();
	void state_8();
	void state_9();
	void state_10();
	void state_11();
	void state_12();
	void state_13();
	void state_14();
	void state_15();
	void state_16();
	void state_17();
	void state_18();
	void state_19();
	//void state_20();
	void state_21();
	void state_22();
	void state_23();
	void state_24();
	void state_25();

	// Increments the sequence number and handles the wrap-around at 65535.
	// Return: The new sequence number is returned.
	int inc_sequence_num()
	{
		if((++m_sequence_num) > 65535)
		{
			m_sequence_num = 1;
		}

		return m_sequence_num;
	}

	int get_sequence_num() const
	{
		return m_sequence_num;
	}

	bool SendIridiumStart();
	void RemoveRecord();

	InetDB *m_pInetDB;
	CellSender *m_cell_sender;
	IridiumUtil *m_iridium_util;
	InetIridiumSender m_IridiumSender;

	std::vector<struct send_message> m_msg;
	struct message_info m_msgInfo;
	uint32_t sequence_num;

	AFS_Timer m_KeepINetAliveTimer;  // how often we send out the Gateway Update message
	AFS_Timer m_AuthRequestTimer;    // how often we get the Auth code
	AFS_Timer m_ESITimer;    // how often we Exchange Status Information.
	AFS_Timer m_SatUpdateTimer;
	AFS_Timer m_DBCleanupTimer;

	//state machine variables
	uint m_retry_limit;
	uint m_cell_retries;  // number of failures to send a message. After m_retry_limit we switch to Iridium
	//uint m_IridiumPriorityLevel;
	bool m_IridiumEnable;
	ats::String m_ForceIridium; // Can be Never, Off, On.  If Never a restart will be needed to switch modes.  If 'Off' or 'On' you can actively change db-config value and the unit will switch while running
	bool m_FakeIridium; // Send Iridium email directly via cell network
	bool m_IridiumStartHasBeenSent;
	
	int m_sequence_num;
	bool m_bStateTrace; // check if we want state Trace messages.
	bool 	m_bPS19Test;
	
#define SM_INVALID_MID -1
	void clearMsgInfo(struct message_info& mi)
	{
		mi.mid = SM_INVALID_MID;
		mi.pri = 0;
		mi.seq = -1;
		mi.sm.clear();
	}
};

