#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/ioctl.h>

#include "ats-common.h"
#include "ats-string.h"
#include "atslogger.h"
#include "socket_interface.h"
#include "command_line_parser.h"
#include "state-machine-data.h"
#include "timer-event.h"
#include "GPIOPin.h"
#include "Blinker.h"
#include "MyData.h"

const ats::String Blinker::m_no_name;

//-----------------------------------------------------------------------------------------------
const ats::String& Blinker::name() const
{
	return *m_name;
}

//-----------------------------------------------------------------------------------------------
bool Blinker::led_terminate_state() const
{
	lock();
	const bool b = m_led_terminate_state;
	unlock();
	return b;
}

//-----------------------------------------------------------------------------------------------
int Blinker::timeout(bool p_lock) const
{

	if(p_lock)
	{
		lock();
		const int n = m_timeout;
		unlock();
		return n;
	}

	return m_timeout;
}

//-----------------------------------------------------------------------------------------------
bool Blinker::empty() const
{
	lock();
	const bool b = m_script.empty();
	unlock();
	return b;
}

//-----------------------------------------------------------------------------------------------
void Blinker::lock() const
{
	pthread_mutex_lock(m_mutex);
}

//-----------------------------------------------------------------------------------------------
void Blinker::unlock() const
{
	pthread_mutex_unlock(m_mutex);
}

//-----------------------------------------------------------------------------------------------
Blinker::Blinker(MyData& p_md, const ats::String& p_script) :	m_listener(p_md), m_to_listener(p_md)
{
	m_timer = new ats::TimerEvent[2];
	m_led_terminate_state = false;
	m_name = &m_no_name;
	m_md = &p_md;
	m_timeout = 0;
	m_timeout_thread = 0;
	m_priority = 0;
	m_running = 0;
	m_index = 0;
	m_mutex = new pthread_mutex_t;
	pthread_mutex_init(m_mutex, 0);
	set_script(p_script);
	m_bIsRestart = false;
}

//-----------------------------------------------------------------------------------------------
Blinker::~Blinker()
{
	// XXX: No locks held on this blinker during destruction, so all functions requiring "lock" to be held may
	//	be called here without acquiring "lock".
	clean_up_script_thread();
	pthread_mutex_destroy(m_mutex);
	delete m_mutex;
	delete [] m_timer;
}

//-----------------------------------------------------------------------------------------------
void Blinker::clean_up_script_thread()
{

	if(h_running_script())
	{
		pthread_join(m_thread, 0);
		m_running = 0;
	}

}

//-----------------------------------------------------------------------------------------------
bool Blinker::initialize_leds(const ats::String& p_led, ats::String& p_emsg)
{

	if(p_led.empty())
	{
		ats_sprintf(&p_emsg, "no LEDs specified");
		return false;
	}

	{
		size_t i = 0;

		for(;;)
		{
			ats::String current_led;

			if(!ats::next_token(current_led, p_led, i, ","))
			{
				break;
			}

			int addr;
			int byte;
			int pin;
			const char* emsg = m_md->get_led_addr_byte_pin(current_led, addr, byte, pin);

			if(emsg)
			{
				ats_sprintf(&p_emsg, "Failed to create blinker for LED \"%s\": %s", current_led.c_str(), emsg);
				m_led_info.clear();
				return false;
			}

			m_led_info.push_back(GPIOPin(addr, byte, pin));
		}

	}

	if(m_led_info.empty())
	{
		ats_sprintf(&p_emsg, "no LEDs parsed in \"%s\"", p_led.c_str());
		return false;
	}

	for(size_t i = 0; i < m_led_info.size(); ++i)
	{
		m_led_info[i].m_addr = (m_led_info[i].m_addr == EXP_2) ? EXP_2 : EXP_1;
	}

	m_led = p_led;
	return true;
}

//-----------------------------------------------------------------------------------------------
bool Blinker::led_on() const
{
	lock();
	const bool b = m_script.empty() ? false : ((m_script[m_index] & 0x80000000) ? false : true);
	unlock();
	return b;
}

//-----------------------------------------------------------------------------------------------
int Blinker::usleep_time() const
{
	lock();
	const int n = m_script.empty() ? 0 : m_script[m_index] & 0xffffff;
	unlock();
	return n;
}

//-----------------------------------------------------------------------------------------------
void Blinker::next_index()
{
	lock();

	if(m_script.empty())
	{
		unlock();
		return;
	}

	m_index = (m_index + 1) % m_script.size();
	unlock();
}

//-----------------------------------------------------------------------------------------------
//
// the incoming script is 1:100000;0;200000;1;300000....
// the outgoing m_script is an array of ints with on being 0x80 (on) o r 0x00 (off) as the
// first byte in the int and the time being in the last 3 bytes of the int.
void Blinker::set_script(const ats::String& p_script)
{
	std::vector <int> script;
	ats::StringList quanta;
	ats::split(quanta, p_script, ";");
	ats::StringList::const_iterator i = quanta.begin();

	while(i != quanta.end())
	{
		ats::StringList spec;
		ats::split(spec, *i, ",");
		++i;

		if(spec.size() >= 2)
		{
			int n = 0;

			if(strtol(spec[0].c_str(), 0, 0))
			{
				n |= 0x80 << 24;
			}

			n |= strtol(spec[1].c_str(), 0, 0);

			script.push_back(n);
		}

	}

	lock();
	m_script = script;
	m_index = 0;
	unlock();
}

//-----------------------------------------------------------------------------------------------
void Blinker::run()
{
	lock();

	if(!m_running)
	{
		m_running = 's';
		pthread_create(&m_thread, 0, Blinker::blinker_task, this);
	}

	unlock();
}

//-----------------------------------------------------------------------------------------------
void Blinker::stop_timeout_thread()
{

	if(m_timeout_thread)
	{
		const bool cancel_timer = true;
		const bool disable_timer = true;
		timeout_timer()->stop_timer(cancel_timer, disable_timer);
	}

}

//-----------------------------------------------------------------------------------------------
void Blinker::detach()
{
	m_name = &m_no_name;
}

//-----------------------------------------------------------------------------------------------
void Blinker::kick_blink_timeout_thread()
{
	// XXX: "timeout_timer->stop_timer" will have no effect if the timer has not been started.
	//      Redundant calls to "timeout_timer->stop_timer" are also OK, so there is no need
	//      to use locks to ensure that a timeout thread is running.
	const bool cancel_timer = true;
	timeout_timer()->stop_timer(cancel_timer);
}

//-----------------------------------------------------------------------------------------------
void Blinker::stop()
{
	lock();
	h_stop();
	unlock();
}

//-----------------------------------------------------------------------------------------------
void Blinker::h_stop()
{
	m_script.clear();
	const bool cancel_timer = true;
	const bool disable_timer = true;
	script_timer()->stop_timer(cancel_timer, disable_timer);
}

//-----------------------------------------------------------------------------------------------
const char* Blinker::set_app(const ats::String& p_app)
{
	const char* emsg = 0;
	lock();

	if(m_running && ('a' != m_running))
	{
		emsg = "script running (in use)";
	}
	else
	{
		m_app = p_app;
		m_running = 'a';
	}

	unlock();
	return emsg;
}

//-----------------------------------------------------------------------------------------------
void* Blinker::blinker_timeout_thread(void* p)
{
	Blinker& b = *((Blinker*)p);
	int timeout;

	while(!(b.empty()) && (timeout = b.timeout()))
	{
		ats::TimerEvent& t = *(b.timeout_timer());
		t.start_timer_and_wait(0, timeout);

		b.lock();

		if(!(t.is_cancelled()) || (!(t.is_enabled())))
		{
			b.h_stop();
			b.unlock();
			break;
		}

		b.unlock();
	}

	return 0;
}

//-----------------------------------------------------------------------------------------------
void* Blinker::blinker_task(void* p)
{
	Blinker& b = *((Blinker*)p);
	MyData& md = *(b.m_md);

	if(b.timeout())
	{
		b.m_timeout_thread = new pthread_t;
		pthread_create(b.m_timeout_thread, 0, blinker_timeout_thread, &b);
	}

	const ats::String gpio_name("Blinker(" + b.name() + ")");
	std::vector <GPIOContext*> gc;
	gc.resize(b.m_led_info.size());

	{

		for(size_t i = 0; i < gc.size(); ++i)
		{
			const GPIOPin& g = b.m_led_info[i];
			gc[i] = b.m_md->m_gpio->get_gpio_context(&gpio_name, md.m_exp_addr[g.m_addr], g.m_byte, g.m_pin, b.m_priority);
		}

	}

	while(!b.empty())
	{
		{
			for(size_t i = 0; i < b.m_led_info.size(); ++i)
			{
				const GPIOPin& g = b.m_led_info[i];
				set_led(*(b.m_md), &gpio_name, g.m_addr, g.m_byte, g.m_pin, b.led_on() ? 1 : 0);
			}
		}

		const int usec = b.usleep_time();
		ats::TimerEvent& t = *(b.script_timer());
		t.start_timer_and_wait(0, usec);

		b.lock();

		if(t.is_cancelled() || (!(t.is_enabled())))
		{
			if (!b.GetRestart())  // do not want to break on restart 
			{
				b.h_stop();
				b.unlock();
				b.stop_timeout_thread();
				break;
			}
			else
			{
				b.SetRestart(false);
				b.unlock();
				continue;  // don't increment to next_index
			}

		}

		b.unlock();
		b.next_index();
	}

	{

		for(size_t i = 0; i < b.m_led_info.size(); ++i)
		{
			const GPIOPin& g = b.m_led_info[i];
			set_led(*(b.m_md), &gpio_name, g.m_addr, g.m_byte, g.m_pin, b.led_terminate_state() ? 1 : 0);
		}

	}

	if(b.m_timeout_thread)
	{
		pthread_join(*(b.m_timeout_thread), 0);
		delete b.m_timeout_thread;
		b.m_timeout_thread = 0;
	}

	{

		for(size_t i = 0; i < gc.size(); ++i)
		{
			b.m_md->m_gpio->put_gpio_context(gc[i]);
		}

		b.m_md->m_gpio->flush(*(b.m_md));
	}

	b.m_md->add_to_reaper_list(b.name());
	return 0;
}
