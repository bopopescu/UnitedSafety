#pragma once
#include <pthread.h>
#include <semaphore.h>

#include "state_machine.h"
#include "IridiumUtil.h"
#include "calampscompressedmessage.h"
#include "calampseventmessage.h"
#include "calampsusermessage.h"
#include "calampsmessage.h"

#include "packetizerCamsDB.h"
#include "packetizerCellSender.h"
#include "packetizerIridiumMessage.h"

class PacketizerCamsStateMachine : public StateMachine
{
public:
	PacketizerCamsStateMachine(MyData&);
	void start();
	bool readfromPacketizerDB(int p_mid, struct send_message& p_msg);
	void get_message_backlog(std::vector<struct send_message>& p_msg, int p_limit);

private:

	void (PacketizerCamsStateMachine::*m_state_fn)();
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
	void state_20();
	void state_21();
	void state_22();
	void state_23();
	void state_24();
	void state_25();

	// Description: Compresses all messages in "p_msg" into a single message "p_des".
	//
	//	"p_des" must be a newly created "struct send_message".
	//
	//	"p_msg" must be non-empty.
	//
	//	"p_sequence_num" is the sequence number that the new message "p_des" will contain. It is common for
	//	"p_sequence_num" to be the sequence number of the last message in "p_msg" plus one.
	//
	// Return: True is returned on success and false is returned on error. "p_des" is undefined
	//	on error.
	static bool compress(struct send_message& p_des, const std::vector<struct send_message>& p_msg, int p_sequence_num);

	// Description: Increments the sequence number and handles the wrap-around at 65535.
	//
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


	void sendMessage();
	bool readFromDashDB(int mid);
	void waitForNextMessage();
	static void* priorityOneCellSend(void*);
	static void* priorityOneIridiumSend(void*);
	void SendFakeIridium(std::vector<char> data);

	PacketizerCamsDB *m_dbreader;
	PacketizerCellSender *m_cell_sender;
	IridiumUtil *m_iridium_util;

	std::vector<struct send_message> m_msg;
	struct message_info m_msgInfo;
	uint32_t sequence_num;
	std::map<int,int> m_messagetypes;
	AFS_Timer m_keepAliveTimer;
	uint32_t m_CompressedSeqNum;

	//state machine variables
	int m_keepalive_timeout_seconds;
	uint m_timeout;
	uint m_max_backlog;
	uint m_retry_limit;
	uint m_iridium_timeout_seconds;
	int m_UseCompression;
	bool m_CellFailMode;
	bool m_CellFailModeEnable;
	uint m_cell_retries;
	uint m_IridiumPriorityLevel;
	uint m_PriorityIridiumDataLimit;
	bool m_IridiumEnable;
	ats::String m_imei;
	ats::String m_IridiumIMEI;
	bool m_state16_message_sent;
	std::vector<char> m_state15_iridium_data;
	ats::String m_ForceIridium; // Can be Never, Off, On.  If Never a restart will be needed to switch modes.  If 'Off' or 'On' you can actively change db-config value and the unit will switch while running
	bool m_FakeIridium; // Send Iridium email directly via cell network

	REDSTONE_IPC m_redstone;
	int m_sequence_num;

	//Priority1thread variables
	static pthread_mutex_t m_priorityOneMutex;
	static bool m_thread_finished;

	static bool isThreadFinished()
	{
		pthread_mutex_lock(&m_priorityOneMutex);
		bool m = m_thread_finished;
		pthread_mutex_unlock(&m_priorityOneMutex);
		return m;
	}
#define SM_INVALID_MID -1
	void clearMsgInfo(struct message_info& mi)
	{
		mi.mid = SM_INVALID_MID;
		mi.pri = 0;
		mi.seq = -1;
		mi.sm.clear();
	}
};


