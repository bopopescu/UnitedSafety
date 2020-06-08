#pragma once
#include <map>

#include <pthread.h>

#include "ats-common.h"
#include "state-machine-data.h"
#include "timer-event.h"
#include "GPIOPin.h"

class Command;
class MyData;
class Blinker;

typedef std::map <const ats::String, Blinker*> BlinkerMap;
typedef std::pair <const ats::String, Blinker*> BlinkerPair;

typedef std::map <Blinker*, void*> BlinkerPtrMap;
typedef std::pair <Blinker*, void*> BlinkerPtrPair;

typedef std::map <const ats::String, Command*> CommandMap;
typedef std::pair <const ats::String, Command*> CommandPair;

void* blinker_task(void* p);

class Blinker
{
public:
	EventListener m_listener;
	EventListener m_to_listener;

	static const ats::String m_no_name;

	ats::String m_led;

	std::vector <GPIOPin> m_led_info;
	int m_priority;

	bool m_led_terminate_state;

	MyData* m_md;

	const ats::String& name() const;

	bool led_terminate_state() const;

	int timeout(bool p_lock=true) const;

	bool empty() const;

	Blinker(MyData& p_md, const ats::String& p_script);

	~Blinker();

	std::vector <int> m_script;
	size_t m_index;

	bool led_on() const;

	int usleep_time() const;

	void next_index();

	void set_script(const ats::String& p_script);

	pthread_t m_thread;

	void kick_blink_timeout_thread();

	// Description:
	//
	void stop();

	// Description:
	//
	void detach();

	// Description:
	//
	// XXX: Only --- may call this function.
	//
	// Return: NULL is returned on success, and an error message is returned otherwise.
	const char* set_app(const ats::String&);

	// Description:
	//
	// XXX: Only "MyData.cpp:h_unload_blinker_program" may call this function.
	void unset_app()
	{
		m_running = 0;
	}

	bool running_app(bool p_lock=true) const
	{

		if(p_lock)
		{
			lock();
		}

		const bool b = 'a' == m_running;

		if(p_lock)
		{
			unlock();
		}

		return b;
	}

	bool running_script() const
	{
		lock();
		const bool b = h_running_script();
		unlock();
		return b;
	}

	const ats::String app_name() const
	{
		return m_app;
	}

	void SetRestart(bool val){m_bIsRestart = val;};
	bool GetRestart(){return m_bIsRestart;};
	
private:
	friend class BlinkCommand;
	friend class MyData;

	ats::String m_app;
	pthread_mutex_t* m_mutex;
	pthread_t* m_timeout_thread;
	const ats::String* m_name;
	int m_timeout;
	int m_running;
	bool m_bIsRestart;  // true if we are ordering a script restart

	ats::TimerEvent* m_timer;
	ats::TimerEvent* script_timer() const { return m_timer;}
	ats::TimerEvent* timeout_timer() const { return m_timer + 1;}

	static void* blinker_task(void* p);
	static void* blinker_timeout_thread(void* p);

	bool h_running_script() const
	{
		return ('s' == m_running);
	}

	void lock() const;

	void unlock() const;

	void stop_timeout_thread();

	// Description:
	//
	// XXX: Only MyData::add_blinker may call this function.
	bool initialize_leds(const ats::String& p_led, ats::String& p_emsg);

	// Description:
	//
	// XXX: Only BlinkCommand::Command may call this function.
	void run();

	void clean_up_script_thread();

	void h_stop();
};
