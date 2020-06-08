#pragma once

#include <deque>
#include <map>
#include <vector>

#include <semaphore.h>

#include "ats-common.h"
#include "state-machine-data.h"

extern int g_dbg;
struct send_message
{
	size_t seq;
	int mid;
	size_t pri;
	int msg_db_id;  //message_db mtid - used for clearing message blocks in power monitor
	std::vector<char> data;
};

struct send_umessage
{
	size_t seq;
	int mid;
	size_t pri;
	int msg_db_id;  //message_db mtid - used for clearing message blocks in power monitor
	std::vector<unsigned char> data;
};

struct message_info
{
	int mid;
	size_t seq;
	size_t pri;
	ats::StringMap sm;
};

// AWARE360 FIXME: TFD-860: "MyData" is supposed to be a local class for the application. It
//	must not be included in any library. Delete all of "MyData" and move it to the target
//	applications.
class MyData : public StateMachineData
{
public:

	MyData(){
		current_mid = 0;
		m_mutex = new pthread_mutex_t;
		pthread_mutex_init(m_mutex, 0);
	}

	~MyData(){
		pthread_mutex_destroy(m_mutex);
		delete m_mutex;
	}

	void set_current_mid(int newmid) {current_mid = newmid;}
	int get_current_mid(){return current_mid;}

protected:
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
	pthread_mutex_t* m_mutex;
};
