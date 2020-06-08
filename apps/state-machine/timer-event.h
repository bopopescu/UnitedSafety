#pragma once

#include "event_listener.h"

namespace ats
{

class TimerEvent : public AppEvent
{
public:
	COMMON_EVENT_DECLARATION(TimerEvent)

	TimerEvent(int p_sec, int p_usec=0);

	virtual ats::String set_default_event_name(const ats::String& p_name);

	virtual ats::String default_event_name();

	// Description: Changes the timeout period for the timer.
	//
	// XXX: This function may only be called before "start_monitor", or after "stop_monitor"
	//	has been called. Behaviour is undefined when this function is called between
	//	"start_monitor" and "stop_monitor" pairs.
	void set_timeout(int p_sec, int p_usec=0);

	static const int err_thread_in_use = -1;
	static const int err_open_pipe_failed = -2;
	static const int err_timer_disabled = -3;
	static const int err_invalid_param = -4;

	// Description: Starts the timer and waits. This function behaves exactly like "start_monitor"
	//	except that this is a blocking call (no background timer thread is created). This
	//	function is useful when the caller just wants to wait/sleep for a specified time
	//	period (which supports interruption through "stop_timer"), but without the overhead
	//	of creating a new timing/monitoring thread.
	//
	//	The timeout period will be set to "p_sec" seconds and "p_usec" micro seconds.
	//
	//	It is an error to call this function after calling "start_monitor". In that event,
	//	one must call "stop_monitor" first before being able to call this function (the
	//	must decide on threaded or non-threaded timing, but not both at the same time).
	//
	// Return: 0 is returned on success and a negative number (one of TimerEvent::err_*) is
	//	returned on error.
	int start_timer_and_wait(int p_sec, int p_usec);

	// Description: Stops the timer and sets the reason to cancelled if "p_cancel" is true, otherwise
	//	the reason is timeout.
	void stop_timer(bool p_cancel);

	// Description: Stops the timer and sets the reason to cancelled if "p_cancel" is true, otherwise
	//	the reason is timeout.
	//
	//	If "p_disable" is true, then the timer is stopped and disabled so that it cannot
	//	be started again.
	void stop_timer(bool p_cancel, bool p_disable);

	// Description: Enables the timer if "p_enable" is true, otherwise the timer is disabled.
	//	Calling this function with "p_enable" set to false will not stop or cancel a running
	//	timer.
	//
	//	By default, the timer is enabled.
	void enable_timer(bool p_enable);

	bool is_enabled() const { return (!m_disabled);}

	int get_error() const;

protected:
	// Description: Called only when the timer expires. If the timer expires, this function
	//	will be called before the TimerEvent is posted to registered listeners.
	//
	//	Derived classes may override this function to provide custom behaviour. The default
	//	implementation is a NOP (No OPeration).
	//
	//	No timer locks are held when this function is called.
	virtual void on_timeout();

private:
	ats::String m_default_event_name;

	static void* run_timer_thread(void* p_timer);

        pthread_t* m_thread;
	pthread_mutex_t* m_mutex;

	int m_sec;
	int m_usec;

	int m_pipe[2];

	int m_error;
	bool m_disabled;
	bool m_stopping;
	bool m_stop_pipe_written;

	void init();

	void lock() const;
	void unlock() const;

	int h_open_pipe();
	void h_close_pipe();
};

}
