#pragma once

#include <map>

#include "ats-common.h"
#include "state_machine.h"

#include "midDB.h"

#include "packetizerDB.h"
#include "packetizerSender.h"
//#include "packetizerMessage.h"
#include "RedStone_IPC.h"


class PacketizerSm : public StateMachine
{
public:
	PacketizerSm(MyData&);
	virtual~ PacketizerSm();

	virtual void start();

protected:
	bool readfromCantelDB(int, struct send_message& p_msg);
	bool ReadFromRealTimeDB(int mid, struct send_message& p_msg);

private:
	void (PacketizerSm::*m_state_fn)();
	void state_0();
	void state_1();
	void state_2();
	void state_3();
	void state_4();
	void state_5();
	void state_6();

	void send_message();

	// Description: Reads (and then removes) a number of backlog messages from the "CALAMPDB" database.
	//	The maximum number of messages read in one call will be "p_limit".
	//
	//	"p_msg" will contain the backlog messages read (if any).
	//
	//	"p_limit" is expected to be the largest number of messages that can be compressed into a
	//	single LMDirect message, where the LMDirect message will still fit into a single UDP packet.
	//	If "p_limit" is less than 1, then it will be treated as 1.
	//
	// Return: "p_msg" will contain a number of backlog messages (if any). Messages that appear in "p_msg"
	//	will have been removed from the "CALAMPDB" database.
	void get_message_backlog(std::vector<struct send_message>& p_msg, int p_limit);

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

	std::map<int,int> m_messagetypes;
	std::vector<struct send_message> m_msg;
	REDSTONE_IPC m_RedStone;
	midDB m_mdb;
	ats::String m_CurTable; // the current table that we are reading from - either realtime_table or message_table
	PacketizerDB* m_dbreader;
	PacketizerSender* m_sender;
	time_t m_keepalive_timeout_seconds;
	int m_max_backlog;
	int m_sequence_num, m_CompressedSeqNum;
	bool m_compressed_was_incremented;
	int m_UseCompression;  // in db-config get packetizer-calamps UseCompression
	int m_timeout;
};
