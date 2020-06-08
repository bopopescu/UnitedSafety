#include "state-machine-data.h"
#include "event_listener.h"

static const ats::StringMap g_empty_data;

const ats::String AppEvent::m_type = "AppEvent";

AppEvent::AppEvent()
{
	m_listener = 0;
	m_data = 0;
	m_event_key = 0;;
}

AppEvent::~AppEvent()
{
	stop_monitor();

	if(m_data)
	{
		delete m_data;
	}

}

const ats::String& AppEvent::type() const
{
	return m_type;
}

bool AppEvent::is_a(const ats::String& p_key) const
{
	return AppEvent::m_type == p_key;
}

bool AppEvent::h_is_a(const ats::String& p_key) const
{
	return ((&p_key) == (&AppEvent::m_type));
}

void AppEvent::start_monitor()
{
}

void AppEvent::stop_monitor()
{
}

const ats::StringMap& AppEvent::data() const
{

	if(m_data)
	{
		return *m_data;
	}

	return g_empty_data;
}

ats::String AppEvent::default_event_name()
{
	return type();
}

ats::String AppEvent::set_default_event_name(const ats::String&)
{
	return type();
}

ats::String AppEvent::event_key() const
{
	return m_event_key ? (*m_event_key) : ats::String();
}

#if 0
AppEvent* EventListener::remove_event(const ats::String& p_key)
{
	AppEvent* p;
	(my_data()).remove_event_listener(*this, p_key, p);
	return p;
}
#endif

AppEvent* EventListener::remove_event(AppEvent* p_event)
{

	if(p_event)
	{
		(my_data()).remove_event_listener(this, p_event);
	}

	return p_event;
}

#if 0
void EventListener::destroy_event(const ats::String& p_key)
{
	AppEvent* e = remove_event(p_key);

	if(e)
	{
		delete e;
	}

}
#endif

void EventListener::destroy_event(AppEvent* p_event)
{

	if(p_event)
	{
		remove_event(p_event);
		delete p_event;
	}

}
