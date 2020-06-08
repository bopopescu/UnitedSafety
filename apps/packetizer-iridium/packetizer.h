#pragma once

#include <deque>
#include <map>
#include <vector>

#include <semaphore.h>

#include "ats-common.h"
#include "Iridium.h"
#include "state-machine-data.h"

extern int g_dbg;
struct send_message
{
	int seq;
	int mid;
	int msg_type;  // the type of the message - used to unset_work on the message type
	std::vector<char> data;
};

class MyData : public StateMachineData
{
public:

	MyData();
	~MyData();

	void set_current_mid(int newmid) {current_mid = newmid;}
	int get_current_mid(){return current_mid;}

protected:
	bool testdatadir_existing() const;
	void lock() const
	{
		pthread_mutex_lock(m_mutex);
	}
	void unlock() const
	{
		pthread_mutex_unlock(m_mutex);
	}

private:
	int current_mid;
	bool m_testdatadir_existing;
	pthread_mutex_t* m_mutex;
};
