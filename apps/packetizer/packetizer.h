#pragma once

#include <deque>
#include <map>
#include <vector>

#include <semaphore.h>

#include "AFS_Timer.h"
#include "ats-common.h"
#include "state-machine-data.h"

extern int g_dbg;
struct send_message
{
	int seq;
	int mid;
	int msg_mid;
	std::vector<char> data;
};

class MyData : public StateMachineData
{
public:

	MyData();
	~MyData();

	void set_current_mid(int newmid) {current_mid = newmid;}
	int get_current_mid(){return current_mid;}
	void testdata_log(const ats::String& p_msg) const;

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
	AFS_Timer* m_timer;
	bool m_testdatadir_existing;
	pthread_mutex_t* m_mutex;
};
