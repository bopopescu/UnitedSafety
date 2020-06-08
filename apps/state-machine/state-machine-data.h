#pragma once
#include <iostream>

#include "ats-common.h"
#include "event_listener.h"

class StateMachineData : public ats::CommonData
{
public:
	StateMachineData();

	// Description: Posts a "p_event" to all listeners of the "p_event" type. The number of listeners
	//	may be zero, one or more.
	//
	//	If there are no listeners for event "p_event", and "p_event" has not been marked as a
	//	queued event, then the event is lost (thrown away).
	//
	//	If there are no listeners for event "p_event", and "p_event" has been marked as a queued
	//	event, then the event will be stored, and sent the instant that a listener arrives for
	//	the event.
	//
	// Parameters:
	//
	//	p_event - The event to post.
	//
	//	p_data - Optional data to pass to the event. "p_data" must be NULL if no data will be passed,
	//		or must be a pointer to an ats::StringMap allocated with "new". If "p_data" is not NULL,
	//		then it will be owned/managed by "this" class and the caller shall no longer reference
	//		the pointer.
	const char* post_event(const ats::String& p_event, ats::StringMap* p_data = 0);

	// Description: Posts a "cancel" message to event "p_event".
	//
	// Parameters:
	//
	//	p_event - The event to cancel.
	//
	//	p_data - Optional data to pass to the event. "p_data" must be NULL if no data will be passed,
	//		or must be a pointer to an ats::StringMap allocated with "new". If "p_data" is not NULL,
	//		then it will be owned/managed by "this" class and the caller shall no longer reference
	//		the pointer.
	const char* post_cancel_event(const ats::String& p_event, ats::StringMap* p_data = 0);

	typedef std::map <const ats::String, AppEvent*> EventListenerList;
	typedef std::pair <const ats::String, AppEvent*> EventListenerListPair;

	typedef std::map <const ats::String, EventListenerList> EventListenerMap;
	typedef std::pair <const ats::String, EventListenerList> EventListenerMapPair;

	class QueuedPost
	{
	public:
		ats::StringMap* m_data;

		QueuedPost(ats::StringMap* p_data)
		{
			m_data = p_data;
		}

	};

	void flush_post_queues();

	typedef std::list <QueuedPost> PostQueue;
	typedef std::map <ats::String, PostQueue> PostQueueMap;
	typedef std::pair <ats::String, PostQueue> PostQueuePair;

	PostQueueMap m_post_queue;

	void queue_undeliverable_events(const ats::String& p_event, bool p_enable_queue);

	// Description: A reverse mapping which supports the removal of all event handling
	//    related to a specific EventListener object. Given an EventListener object, the
	//    reverse mapping will return all references to the EventListener object, which
	//    can be iterated through to completely remove the EventListener object from
	//    the event handling system.
	//
	// See Also: http://svn.atsplace.int/software-group/AbsoluteTrac/RedStone/Documents/Design/System-Watchdog-Event-Diagram.odg
	class EventReverseMapping
	{
	public:
		// Description: A mapping for the EventListener object. A specific EventListener object
		//    entry in the event system can be uniquely identified by the event name, and the event
		//    key.
		class EventMapping
		{
		public:
			EventMapping(const ats::String& p_event, const ats::String& p_key)
			{
				m_event = &p_event;
				m_key = &p_key;
			}

			const ats::String* m_event;
			const ats::String* m_key;
		};

		typedef std::map <AppEvent*, EventMapping> EventMappingMap;
		typedef std::pair <AppEvent*, EventMapping> EventMappingPair;

		EventReverseMapping(const ats::String& p_event, const ats::String& p_key, AppEvent* p_wdevent)
		{
			add_mapping(p_event, p_key, p_wdevent);
		}

		void add_mapping(const ats::String& p_event, const ats::String& p_key, AppEvent* p_wdevent)
		{
			m_map.insert(EventMappingPair(p_wdevent, EventMapping(p_event, p_key)));
		}

		void remove_mapping(AppEvent* p_event)
		{
			EventMappingMap::iterator i = m_map.find(p_event);

			if(i != m_map.end())
			{
				m_map.erase(i);
			}

		}

		bool empty() const
		{
			return m_map.empty();
		}

		EventMappingMap m_map;
	};

	typedef std::map <EventListener*, EventReverseMapping> EventListenerReverseMap;
	typedef std::pair <EventListener*, EventReverseMapping> EventListenerReversePair;

	bool add_event_listener(AppEvent* p_event, const ats::String& p_key, EventListener*);
	bool add_event_listener(AppEvent* p_event, const ats::String& p_key, EventListener*, const ats::String& p_event_name);

	EventListener* remove_event_listener(const ats::String& p_event_name, const ats::String& p_key, AppEvent*& p_event);

	EventListener* remove_event_listener(const ats::String& p_event, const ats::String& p_key);

	// Description: Removes the AppEvent "p_event" for listener "p_listener". Once removed "p_event"
	//	will no longer be able to post events (until it is added again using "add_event_listener").
	//
	// Return: Event listener "p_listener" is returned.
	EventListener* remove_event_listener(EventListener* p_listener, AppEvent* p_event);

	// Description:
	//
	// XXX: This function will delete all AppEvents managed by "p_listener".
	//
	// ATS FIXME: Add good documentation (or fix API).
	//
	// Return: Event listener "p_listener" is returned.
	EventListener* remove_event_listener(EventListener* p_listener);

private:
	// ATS FIXME: "WD" stands for "WatchDog" and is an old/obsolete naming.
	//	Replace with "App" for application (ex: AppEventQueue, instead of WDEventQueue).
	typedef std::list <AppEvent> WDEventQueue;
	WDEventQueue m_event;

	EventListenerMap m_event_listener;
	EventListenerReverseMap m_event_reverse_map;

	pthread_mutex_t* m_event_mutex;

	void lock_event() const;

	void unlock_event() const;

	const char* h_post_event(AppEvent* p_event, bool p_cancel);
	const char* h_post_event(const ats::String& p_event, ats::StringMap* p_data = 0, bool p_cancel = false);

	EventListener* h_remove_event_listener(const ats::String& p_event_name, const ats::String& p_key, AppEvent*& p_event);

	EventListener* h_remove_event_listener(const ats::String& p_event, const ats::String& p_key);

	friend std::ostream& operator <<(std::ostream& p_out, const StateMachineData& p_smd);

	StateMachineData(const StateMachineData&);
	StateMachineData& operator =(const StateMachineData&) const;
};

