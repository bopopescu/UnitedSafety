#include "TimerEngine.h"
#include "TimerControlBlock.h"
#include "SocketReferenceManager.h"
#include "MyData.h"

static const int g_time_slice_usec = 10 * 1000;

static void wait_for_next_period(const struct timespec& p_t)
{
	const double usec_future = (double(p_t.tv_sec) * 1000000.0f) + double(p_t.tv_nsec / 1000);

	for(;;)
	{
		struct timespec t;
		clock_gettime(CLOCK_MONOTONIC, &t);
		const double usec_current = (double(t.tv_sec) * 1000000.0f) + double(t.tv_nsec / 1000);
		const int diff_usec = int(usec_future - usec_current);

		if(diff_usec <= 0)
		{
			break;
		}

		if(diff_usec >= 1000000)
		{
			sleep(1);
		}
		else if(diff_usec >= g_time_slice_usec)
		{
			usleep(diff_usec);
		}

	}

}

static void wait_for_next_period_sec(const struct timespec& p_t)
{
	const double msec_future = (double(p_t.tv_sec) * 1000.0f) + double(p_t.tv_nsec / 1000000);

	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	const double msec_current = (double(t.tv_sec) * 1000.0f) + double(t.tv_nsec / 1000000);
	const int diff_msec = int(msec_future - (msec_current + 63));

	if(diff_msec > 1000)
	{
		sleep(diff_msec / 1000);
	}
	else if(diff_msec > 0)
	{
		usleep(diff_msec * 1000);
	}

}

// Description: Thread managing running "on-time" processes and events.
//
// XXX: This thread uses CLOCK_MONOTONIC. The timing of this thread will not be exact,
//	but will not drift relative to CLOCK_MONOTONIC. This thread will drift from real
//	time (such as GPS or Atomic time) as CLOCK_MONOTONIC (CPU clock/oscillator) drifts.
static void* timer_thread(void* p)
{
	TimerEngine& te = *((TimerEngine*)p);

	if(te.m_period_msec < 1000)
	{
		const int period_usec = te.m_period_msec * 1000;

		// Start at next whole second
		{

			for(;;)
			{
				struct timeval t;
				gettimeofday(&t, 0);

				if(abs(t.tv_usec < g_time_slice_usec))
				{
					break;
				}

			}

		}

		struct timespec master_clock;
		clock_gettime(CLOCK_MONOTONIC, &master_clock);
		int usec = master_clock.tv_nsec / 1000;
		int sec = master_clock.tv_sec;

		for(;;)
		{
			usec += period_usec;

			if(usec >= 1000000)
			{
				usec -= 1000000;
				++sec;
			}

			master_clock.tv_sec = sec;
			master_clock.tv_nsec = usec * 1000;
			wait_for_next_period(master_clock);

			{
				struct timeval t;
				gettimeofday(&t, 0);
				const int& sec = t.tv_sec;
				const int msec = ((t.tv_usec / period_usec) * period_usec) / 1000;

				te.lock();

				if(te.m_tcb.empty())
				{
					te.unlock();
					break;
				}

				std::map <const ats::String, TimerControlBlock*>::const_iterator i = te.m_tcb.begin();

				do
				{
					TimerControlBlock& tcb = *(i->second);
					++i;
					tcb.on_timeout(sec, msec);
				}
				while(i != te.m_tcb.end());

				te.unlock();
			}

		}

	}
	else
	{
		const int period_sec = te.m_period_msec / 1000;

		// Start at next whole period
		struct timespec master_clock;
		{

			for(;;)
			{
				struct timeval t;
				gettimeofday(&t, 0);

				if(!(t.tv_sec % period_sec))
				{
					clock_gettime(CLOCK_MONOTONIC, &master_clock);
					break;
				}

				usleep(500 * 1000);
			}

		}

		int sec = master_clock.tv_sec;

		for(;;)
		{
			{
				struct timeval t;
				gettimeofday(&t, 0);
				const int& sec = t.tv_sec;

				te.lock();

				if(te.m_tcb.empty())
				{
					te.unlock();
					break;
				}

				std::map <const ats::String, TimerControlBlock*>::const_iterator i = te.m_tcb.begin();

				do
				{
					TimerControlBlock& tcb = *(i->second);
					++i;
					tcb.on_timeout(sec, 0);
				}
				while(i != te.m_tcb.end());

				te.unlock();
			}

			sec += period_sec;
			master_clock.tv_sec = sec;
			wait_for_next_period_sec(master_clock);

			// ATS FIXME: This is a hack to keep "ontime" seconds divisible by the period
			//	without remainder. Find out why this is needed.
			for(;;)
			{
				struct timeval t;
				gettimeofday(&t, 0);

				if(!(t.tv_sec % period_sec))
				{
					break;
				}

				usleep(500 * 1000);
			}

		}

	}

	return 0;
}

TimerEngine::TimerEngine()
{
	m_period_msec = -1;
	pthread_mutex_init(&m_lock, 0);
}

TimerEngine::~TimerEngine()
{
	TimerControlBlockMap::const_iterator i = m_tcb.begin();

	lock();

	while(i != m_tcb.end())
	{
		delete (i->second);
		++i;
	}

	m_tcb.clear();
	unlock();

	pthread_join(m_thread, 0);
}

void TimerEngine::lock()
{
	pthread_mutex_lock(&m_lock);
}

void TimerEngine::unlock()
{
	pthread_mutex_unlock(&m_lock);
}

int TimerEngine::time_to_period_msec(const ats::String& p_time, ats::String* p_emsg)
{
	const int max_time = 2147483;
	const float time = strtod(p_time.c_str(), 0);
	int period_msec = -1;

	if(time < 0.0f)
	{

		if(p_emsg)
		{
			*p_emsg = "Invalid timing=" + p_time;
		}

		return -1;
	}

	if(time < 1.0f)
	{
		const int i = int(time * 1000.0f);

		switch(i)
		{
		case 20: // 50 Hz
		case 25: // 40 Hz
		case 40: // 25 Hz
		case 50: // 20 Hz
		case 100: // 10 Hz
		case 125: // 8 Hz
		case 200: // 5 Hz
		case 250: // 4 Hz
		case 500: // 2 Hz
			period_msec = i;
			break;

		default:

			if(p_emsg)
			{
				*p_emsg = "Invalid sub-second timing=" + p_time;
			}

			return -1;
		}

	}
	else if(int(time) > max_time)
	{

		if(p_emsg)
		{
			*p_emsg = "Period too large for timing=" + p_time + ", max_time=" + ats::toStr(max_time);
		}

		return -1;
	}
	else
	{
		period_msec = int(time) * 1000;
	}

	return period_msec;
}

ats::String TimerEngine::add(const ats::StringMap& p_arg, TimerControlBlock* p_tcb)
{
	const ats::String& key = p_arg.get("key");

	if(!p_tcb)
	{
		return "p_tcb is null for key \"" + key + "\"";
	}

	lock();

	if(m_tcb.find(key) != m_tcb.end())
	{
		unlock();
		delete p_tcb;
		return "duplicate key \"" + key + "\"";
	}

	if(m_tcb.empty())
	{

		if(p_arg.has_key("time"))
		{
			ats::String emsg;
			m_period_msec = time_to_period_msec(p_arg.get("time"), &emsg);

			if(m_period_msec < 0)
			{
				unlock();
				return emsg;
			}

		}
		else if(m_period_msec < 0)
		{
			unlock();
			return "Invalid m_period_msec=" + ats::toStr(m_period_msec);
		}

	}

	const bool start_thread = m_tcb.empty();
	std::pair <std::map <const ats::String, TimerControlBlock*>::iterator, bool> r =
		m_tcb.insert(std::pair <const ats::String, TimerControlBlock*>(key, p_tcb));

	p_tcb->m_key = &((r.first)->first);
	p_tcb->m_te = this;
	unlock();

	if(start_thread)
	{
		pthread_create(&m_thread, 0, timer_thread, this);
	}

	return ats::String();
}

void TimerEngine::remove(const ats::String& p_key)
{
	lock();
	std::map<const ats::String, TimerControlBlock*>::iterator i = m_tcb.find(p_key);

	if(m_tcb.end() != i)
	{
		TimerControlBlock* tcb = i->second;
		m_tcb.erase(i);
		delete tcb;

		if(m_tcb.empty())
		{
			unlock();
			pthread_join(m_thread, 0);
			return;
		}

	}

	unlock();
}

int TimerEngine::set_time(const ats::String& p_time, ats::String* p_emsg)
{
	const int period_msec = time_to_period_msec(p_time, p_emsg);

	if(period_msec > 0)
	{
		m_period_msec = period_msec;
	}

	return period_msec;
}
