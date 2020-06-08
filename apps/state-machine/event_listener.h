#pragma once

#include <map>
#include <list>

#include <semaphore.h>

#include "ats-common.h"

class StateMachineData;

class EventListener;
typedef void (*EventListenerCallback)(EventListener&);

// ATS FIXME: Move OBJECT_IS_A into "ats-common.h", and update comment appropriately.
//	OBJECT_IS_A is useful for more than just event classes/objects.

// Description: Returns true if object P_obj is of type P_type.
//
// Usage: OBJECT_IS_A(Object*, Type_Name)
//
//    Object* - A pointer to an object which supports the "h_is_a" function.
//    Type_Name - The C or C++ type name.
//
// Example:
//    AppEvent* e;
//
//    if(OBJECT_IS_A(e, SpeedEvent))
//    {
//       printf("SpeedEvent!");
//    }
#define OBJECT_IS_A(P_obj, P_type) ((P_obj)->h_is_a(P_type::m_type))

#define COMMON_EVENT_DECLARATION(P_type) \
	P_type(); \
	virtual~ P_type(); \
	\
	virtual const ats::String& type() const; \
	virtual bool is_a(const ats::String& p_key) const; \
	\
	virtual bool h_is_a(const ats::String& p_key) const; \
	\
	static const ats::String m_type; \
	\
	virtual void start_monitor(); \
	\
	virtual void stop_monitor();

#define COMMON_EVENT_DEFINITION(P_Namespace, P_Type, P_Base) \
	\
	const ats::String P_Namespace P_Type::m_type = #P_Type; \
	\
	const ats::String& P_Namespace P_Type::type() const \
	{ \
		return m_type; \
	} \
	\
	bool P_Namespace P_Type::is_a(const ats::String& p_key) const \
	{ \
		if(P_Type::m_type == p_key) \
		{ \
			return true; \
		} \
		return P_Base::is_a(p_key); \
	} \
	\
	bool P_Namespace P_Type::h_is_a(const ats::String& p_key) const \
	{ \
		if((&p_key) == (&P_Type::m_type)) \
		{ \
			return true; \
		} \
		return P_Base::h_is_a(p_key); \
	}

// ************************************************************************
// AppEvent
class AppEvent
{
public:
	AppEvent();
	virtual~ AppEvent();

	virtual const ats::String& type() const;
	virtual bool is_a(const ats::String& p_key) const;

	// XXX: Only macro "OBJECT_IS_A" may call this function.
	virtual bool h_is_a(const ats::String& p_key) const;

	virtual void start_monitor();

	virtual void stop_monitor();

	// Description: Returns the event name to use for the event. This name
	//	is used only when the event user has not requested a specific/custom
	//	event name.
	virtual ats::String default_event_name();

	virtual ats::String set_default_event_name(const ats::String& p_name);

	// Description: Returns the event key for this event. If there is no event key
	//	(because this event is not actively listening for an event) then the
	//	empty string is returned.
	ats::String event_key() const;

	// Description:
	//
	// XXX: Only function "StateMachineData::add_event_listener" may access this variable.
	const ats::String* m_event_key;

	class Reason
	{
	public:
		Reason()
		{
			m_cancel = false;
		}

		bool m_cancel;

		bool is_cancelled() const
		{
			return m_cancel;
		}
	};

	Reason m_reason;

	bool is_cancelled() const
	{
		return m_reason.is_cancelled();
	}

	const ats::StringMap& data() const;

	static const ats::String m_type;

	EventListener* m_listener;

	// XXX: Only class StateMachineData event processing may access this variable.
	ats::StringMap* m_data;
};

class AppEventHandler
{
public:
	AppEventHandler(AppEvent* p_event)
	{
		m_event = p_event;
	}

	virtual ~AppEventHandler()
	{

		if(m_event)
		{
			delete m_event;
		}

	}

	AppEvent* m_event;
};

class EventListener
{
public:
	EventListenerCallback m_callback;

	sem_t* m_sem;

	typedef std::list <AppEvent*> EventQueue;

	EventListener(StateMachineData& p_data)
	{
		m_data = &p_data;

		m_sem = new sem_t;
		sem_init(m_sem, 0, 0);

		m_lock = new pthread_mutex_t;
		pthread_mutex_init(m_lock, 0);
	}

	virtual~ EventListener();

	StateMachineData& my_data() const
	{
		return *m_data;
	}

	void post_event(AppEvent* p_event)
	{
		lock();
		m_event.push_back(p_event);
		sem_post(m_sem);
		unlock();
	}

	void post_cancel_event(AppEvent* p_event=0)
	{
		AppEvent* e = p_event ? p_event : new AppEvent();
		e->m_reason.m_cancel = true;
		post_event(e);
	}

	AppEvent* wait_event()
	{
		sem_wait(m_sem);
		lock();
		AppEvent* e = m_event.front();
		m_event.pop_front();
		unlock();
		return e;
	}

	AppEvent* remove_event(const ats::String& p_key);
	AppEvent* remove_event(AppEvent* p_event);

	void destroy_event(const ats::String& p_key);
	void destroy_event(AppEvent* p_event);

private:
	StateMachineData* m_data;

	pthread_mutex_t* m_lock;

	EventQueue m_event;

	void lock()
	{
		pthread_mutex_lock(m_lock);
	}

	void unlock()
	{
		pthread_mutex_unlock(m_lock);
	}

	void remove_all_queued_events()
	{
		while(!m_event.empty())
		{
			AppEvent* e = m_event.front();
			m_event.pop_front();
			delete e;
		}
	}
};
