#pragma once

#include <pthread.h>

#include "ats-common.h"

class TimerControlBlock;
class SocketReferenceManager;

class TimerEngine
{
public:
	typedef std::map <const ats::String, TimerControlBlock*> TimerControlBlockMap;
	typedef std::pair <const ats::String, TimerControlBlock*> TimerControlBlockPair;

	TimerEngine();
	virtual~ TimerEngine();

	void lock();
	void unlock();

	ats::String add(const ats::StringMap& p_arg, TimerControlBlock* p_tcb);

	void remove(const ats::String& p_key);

	int set_time(const ats::String& p_time, ats::String* p_emsg);

	static int time_to_period_msec(const ats::String& p_time, ats::String* p_emsg);

	TimerControlBlockMap m_tcb;
	pthread_t m_thread;
	pthread_mutex_t m_lock;
	int m_period_msec;
};
