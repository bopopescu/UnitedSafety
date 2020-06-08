#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <semaphore.h>
#include <unistd.h>

#include "atslogger.h"
#include "state-machine-data.h"
#include "timer-event.h"

using namespace ats;

COMMON_EVENT_DEFINITION(ats::, TimerEvent, AppEvent)

TimerEvent::TimerEvent()
{
	init();
	m_sec = 0;
	m_usec = 0;
}

TimerEvent::TimerEvent(int p_sec, int p_usec)
{
	init();
	m_sec = p_sec;
	m_usec = p_usec;
}

void TimerEvent::init()
{
	m_stop_pipe_written = false;
	m_stopping = false;
	m_disabled = false;
	m_error = 0;
	m_thread = 0;
	m_default_event_name = "TimerEvent" + ats::toStr(this);
	m_pipe[0] = -1;
	m_pipe[1] = -1;

	m_mutex = new pthread_mutex_t[2];
	pthread_mutex_init(m_mutex, 0);
}

TimerEvent::~TimerEvent()
{
	stop_monitor();
	pthread_mutex_destroy(m_mutex);
	delete [] m_mutex;
}

void TimerEvent::enable_timer(bool p_enable)
{
	lock();
	m_disabled = !p_enable;
	unlock();
}

void TimerEvent::lock() const
{
	pthread_mutex_lock(m_mutex);
}

void TimerEvent::unlock() const
{
	pthread_mutex_unlock(m_mutex);
}

ats::String TimerEvent::set_default_event_name(const ats::String& p_name)
{
	lock();
	m_default_event_name = p_name;
	unlock();
	return p_name;
}

void TimerEvent::set_timeout(int p_sec, int p_usec)
{
	lock();
	m_sec = p_sec;
	m_usec = p_usec;
	unlock();
}

int TimerEvent::h_open_pipe()
{

	if((m_pipe[0] >= 0) || (m_pipe[1] >= 0))
	{
		return -EBUSY;
	}

	return pipe(m_pipe);
}

void TimerEvent::h_close_pipe()
{

	if(m_pipe[0] >= 0)
	{
		close(m_pipe[0]);
		m_pipe[0] = -1;
	}

	if(m_pipe[1] >= 0)
	{
		close(m_pipe[1]);
		m_pipe[1] = -1;
	}

	m_stop_pipe_written = false;
}

void TimerEvent::start_monitor()
{
	lock();

	if(m_thread || m_disabled || ((m_sec <= 0) && (m_usec <= 0)))
	{
		unlock();
		return;
	}

	if(h_open_pipe() < 0)
	{
		unlock();
		return;
	}

	m_error = 0;
	m_reason.m_cancel = false;
	m_thread = new pthread_t;
	pthread_create(
		m_thread,
		0,
		run_timer_thread,
		this);
	unlock();
}

int TimerEvent::start_timer_and_wait(int p_sec, int p_usec)
{
	lock();

	if(m_thread)
	{
		unlock();
		return err_thread_in_use;
	}

	if(m_disabled)
	{
		unlock();
		return err_timer_disabled;
	}

	if((p_sec < 0) || (p_usec < 0))
	{
		unlock();
		return err_invalid_param;
	}

	if(!(p_sec || p_usec))
	{
		unlock();
		return err_invalid_param;
	}

	m_sec = p_sec;
	m_usec = p_usec;

	if(h_open_pipe() < 0)
	{
		unlock();
		return err_open_pipe_failed;
	}

	m_error = 0;
	m_reason.m_cancel = false;
	unlock();
	run_timer_thread(this);
	lock();
	h_close_pipe();
	unlock();
	return 0;
}

void TimerEvent::stop_monitor()
{
	lock();

	if((!m_thread) || m_stopping)
	{
		unlock();
		return;
	}

	m_stopping = true;
	unlock();

	stop_timer(true);
	pthread_join(*m_thread, 0);
	lock();
	h_close_pipe();
	delete m_thread;
	m_thread = 0;
	m_stopping = false;
	unlock();
}

void TimerEvent::stop_timer(bool p_cancel)
{
	stop_timer(p_cancel, m_disabled);
}

void TimerEvent::stop_timer(bool p_cancel, bool p_disable)
{
	lock();

	if(p_disable)
	{
		m_disabled = p_disable;
	}

	if(!m_error)
	{
		m_reason.m_cancel = p_cancel;
	}

	if((m_pipe[1] >= 0) && (!m_stop_pipe_written))
	{

		if(1 == write(m_pipe[1], "\0", 1))
		{
			m_stop_pipe_written = true;
		}

	}

	unlock();
}

void* TimerEvent::run_timer_thread(void* p_timer)
{
	TimerEvent& timer = *((TimerEvent*)p_timer);

	if((timer.m_sec > 0) || (timer.m_usec > 0))
	{
		fd_set fds;
		FD_ZERO(&fds);
    		FD_SET(timer.m_pipe[0], &fds);

		struct timeval tv;

		if(timer.m_usec > 1000000)
		{
			tv.tv_sec = timer.m_sec + (timer.m_usec / 1000000);
			tv.tv_usec = timer.m_usec - (tv.tv_sec * 1000000);
		}
		else
		{
			tv.tv_sec = timer.m_sec;
			tv.tv_usec = timer.m_usec;
		}

		const int retval = select(timer.m_pipe[0] + 1, &fds, NULL, NULL, &tv);

		if(-1 == retval)
		{
			timer.lock();
			timer.m_error = errno;
			timer.m_reason.m_cancel = true;
			timer.unlock();
		}

	}

	if(!(timer.is_cancelled()))
	{
		timer.on_timeout();

		if(timer.m_listener)
		{
			timer.m_listener->my_data().post_event(timer.default_event_name());
		}

	}

	return 0;
}

ats::String TimerEvent::default_event_name()
{
	lock();
	const ats::String s(m_default_event_name);
	unlock();
	return s;
}

void TimerEvent::on_timeout()
{
}
