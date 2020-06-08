#include <vector>

#include "state-machine-data.h"

EventListener::~EventListener()
{       
	m_data->remove_event_listener(this);
	delete m_sem;
	delete m_lock;
	remove_all_queued_events();
}

StateMachineData::StateMachineData()
{
	m_event_mutex = new pthread_mutex_t;
	pthread_mutex_init(m_event_mutex, 0);
}

bool StateMachineData::add_event_listener(AppEvent* p_event, const ats::String& p_key, EventListener* p_listener)
{
	return add_event_listener(p_event, p_key, p_listener, p_event->default_event_name());
}

bool StateMachineData::add_event_listener(AppEvent* p_event, const ats::String& p_key, EventListener* p_listener, const ats::String& p_event_name)
{

	if(!p_event)
	{
		return false;
	}

	if(!p_listener)
	{
		delete p_event;
		return false;
	}

	lock_event();

	std::pair <EventListenerMap::iterator, bool> result = m_event_listener.insert(EventListenerMapPair(p_event_name, EventListenerList()));
	EventListenerList& list = result.first->second;
	const ats::String& event_str = result.first->first;
	{
		std::pair <EventListenerList::iterator, bool> result = list.insert(EventListenerListPair(p_key, p_event));

		if(!result.second)
		{
			unlock_event();
			delete p_event;
			return false;
		}
		else
		{
			p_event->m_listener = p_listener;

			const ats::String& key_str = result.first->first;
			EventReverseMapping reverse_mapping(event_str, key_str, p_event);
			std::pair <EventListenerReverseMap::iterator, bool> r = m_event_reverse_map.insert(EventListenerReversePair(p_listener, reverse_mapping));

			if(!r.second)
			{
				EventReverseMapping& mapping = r.first->second;
				mapping.add_mapping(event_str, key_str, p_event);
			}

			p_event->m_event_key = &key_str;
			p_event->start_monitor();
		}

	}

	unlock_event();

	flush_post_queues();

	return true;
}

EventListener* StateMachineData::remove_event_listener(const ats::String& p_event_name, const ats::String& p_key)
{
	AppEvent* dont_care_about_this_param;
	return remove_event_listener(p_event_name, p_key, dont_care_about_this_param);
}

EventListener* StateMachineData::remove_event_listener(const ats::String& p_event_name, const ats::String& p_key, AppEvent*& p_event)
{
	lock_event();
	EventListener* listener = h_remove_event_listener(p_event_name, p_key, p_event);
	unlock_event();
	return listener;
}

EventListener* StateMachineData::h_remove_event_listener(const ats::String& p_event_name, const ats::String& p_key)
{
	AppEvent* dont_care_about_this_param;
	return h_remove_event_listener(p_event_name, p_key, dont_care_about_this_param);
}

EventListener* StateMachineData::h_remove_event_listener(const ats::String& p_event, const ats::String& p_key, AppEvent*& p_wde)
{
	p_wde = 0;
	EventListener* listener = 0;
	EventListenerMap::iterator i = m_event_listener.find(p_event);

	if(i != m_event_listener.end())
	{
		EventListenerList& list = i->second;
		{
			EventListenerList::iterator i = list.find(p_key);

			if(i != list.end())
			{
				p_wde = i->second;
				listener = p_wde->m_listener;
				list.erase(i);
				{
					EventListenerReverseMap::iterator i = m_event_reverse_map.find(listener);

					if(i != m_event_reverse_map.end())
					{
						EventReverseMapping& m = i->second;
						EventListenerReverseMap::iterator j = i;
						++i;

						m.remove_mapping(p_wde);

						if(m.empty())
						{
							m_event_reverse_map.erase(j);
						}

					}

				}
			}

		}

		if(list.empty())
		{
			m_event_listener.erase(i);
		}

	}

	return listener;
}

EventListener* StateMachineData::remove_event_listener(EventListener* p_listener)
{
	lock_event();
	EventListenerReverseMap::iterator i = m_event_reverse_map.find(p_listener);

	if(i != m_event_reverse_map.end())
	{
		EventReverseMapping& reverse_mapping = i->second;
		EventReverseMapping::EventMappingMap::iterator i = reverse_mapping.m_map.begin();

		while(i != reverse_mapping.m_map.end())
		{
			AppEvent* wde = i->first;
			// XXX: "mapping" is no longer valid after calling h_remove_event_listener.
			const EventReverseMapping::EventMapping& mapping = i->second;
			++i;
			h_remove_event_listener(*(mapping.m_event), *(mapping.m_key));
			delete wde;
		}

	}

	unlock_event();
	return p_listener;
}

EventListener* StateMachineData::remove_event_listener(EventListener* p_listener, AppEvent* p_event)
{
	lock_event();
	EventListenerReverseMap::iterator i = m_event_reverse_map.find(p_listener);

	if(i != m_event_reverse_map.end())
	{
		EventReverseMapping& reverse_mapping = i->second;
		EventReverseMapping::EventMappingMap::iterator i = reverse_mapping.m_map.find(p_event);

		if(i != reverse_mapping.m_map.end())
		{
			// XXX: "mapping" is no longer valid after calling h_remove_event_listener.
			const EventReverseMapping::EventMapping& mapping = i->second;
			h_remove_event_listener(*(mapping.m_event), *(mapping.m_key));
		}

	}

	unlock_event();
	return p_listener;
}

const char* StateMachineData::h_post_event(AppEvent* p_event, bool p_cancel)
{
	EventListenerReverseMap::iterator i = m_event_reverse_map.find(p_event->m_listener);

	if(i != m_event_reverse_map.end())
	{
		EventReverseMapping& reverse_mapping = i->second;
		EventReverseMapping::EventMappingMap::iterator i = reverse_mapping.m_map.find(p_event);

		if(i != reverse_mapping.m_map.end())
		{
			// XXX: "mapping" is no longer valid after calling h_remove_event_listener.
			const EventReverseMapping::EventMapping& mapping = i->second;
			h_remove_event_listener(*(mapping.m_event), *(mapping.m_key));
		}

		if(p_cancel)
		{
			p_event->m_listener->post_cancel_event(p_event);
		}
		else
		{
			p_event->m_listener->post_event(p_event);
		}

		return 0;
	}

	static const char* err = "Event has no listener";
	return err;
}

const char* StateMachineData::post_cancel_event(const ats::String& p_event, ats::StringMap* p_data)
{
	lock_event();
	const char* result = h_post_event(p_event, p_data, true);
	unlock_event();
	return result;
}

const char* StateMachineData::post_event(const ats::String& p_event, ats::StringMap* p_data)
{
	lock_event();
	const char* result = h_post_event(p_event, p_data);
	unlock_event();
	return result;
}

const char* StateMachineData::h_post_event(const ats::String& p_event, ats::StringMap* p_data, bool p_cancel)
{
	const char* result = 0;
	EventListenerMap::iterator i = m_event_listener.find(p_event);

	if(i != m_event_listener.end())
	{
		EventListenerList& list = i->second;
		EventListenerList::iterator i = list.begin();
		std::vector <AppEvent*> event_list;
		event_list.resize(list.size());
		size_t j = 0;

		while(i != list.end())
		{
			AppEvent* e = i->second;
			++i;
			event_list[j++] = e;
		}

		{
			std::vector <AppEvent*>::iterator i = event_list.begin();

			while(i != event_list.end())
			{
				AppEvent* event = *i;
				++i;

				if(p_data)
				{

					if(event->m_data)
					{
						delete event->m_data;
					}

					event->m_data = new ats::StringMap(*p_data);
				}

				const char* ret = h_post_event(event, p_cancel);

				if(ret)
				{
					result = ret;
				}

			}

		}
	}
	else
	{
		static const char* err = "No listeners for given event name";
		result = err;
	}

	if(result)
	{
		PostQueueMap::iterator i = m_post_queue.find(p_event);

		if(i != m_post_queue.end())
		{
			PostQueue& q = i->second;
			++i;
			q.push_back(QueuedPost(p_data));
			p_data = 0;
		}
		else
		{
			static const char* err = "Could not queue for given event name";
			result = err;
		}

	}

	if(p_data)
	{
		delete p_data;
	}

	return result;
}

void StateMachineData::flush_post_queues()
{
	// ATS FIXME: The current algorithm is not efficient in time-complexity.
	//
	// Algorithm:
	//    1. Lock event system
	//    2. Copy entire post queue to temporary variable qmap
	//    3. Delete all queued events in post queue (the events are backed up in the temporary post queue)
	//    4. Unlock event system
	//    5. Send all events in qmap again (if still no listener, then they will
	//       be re-queued, otherwise they will be handled. 
	lock_event();
	const PostQueueMap qmap(m_post_queue);
	{
		PostQueueMap::iterator i = m_post_queue.begin();

		while(i != m_post_queue.end())
		{
			(i->second).clear();
			++i;
		}
	}
	unlock_event();

	PostQueueMap::const_iterator i = qmap.begin();

	while(i != qmap.end())
	{
		const ats::String& event_name = i->first;
		const PostQueue &q = i->second;
		++i;

		PostQueue::const_iterator i = q.begin();

		while(i != q.end())
		{
			const QueuedPost& post = *i;
			++i;
			post_event(event_name, post.m_data);
		}

	}

}

void StateMachineData::queue_undeliverable_events(const ats::String& p_event, bool p_enable_queue)
{
	lock_event();

	if(p_enable_queue)
	{
		m_post_queue.insert(PostQueuePair(p_event, PostQueue()));
	}
	else
	{
		PostQueueMap::iterator i = m_post_queue.find(p_event);

		if(i != m_post_queue.end())
		{
			m_post_queue.erase(i);
		}

	}

	unlock_event();
}

std::ostream& operator <<(std::ostream& p_out, const StateMachineData& p_smd)
{
	p_smd.lock_event();
	p_out << "StateMachineData(" << &p_smd << "):\n";

	{
		p_out << "\tEventQueue: " << p_smd.m_event.size() << "\n";
	}

	{
		p_out << "\n\tPost Queue(" << &(p_smd.m_post_queue) << "): " << p_smd.m_post_queue.size() << "\n";

		StateMachineData::PostQueueMap::const_iterator i = p_smd.m_post_queue.begin();

		while(i != p_smd.m_post_queue.end())
		{

			#if 0
			if(i != p_smd.m_post_queue.end())
			{
				p_out << "\n";
			}
			#endif

			p_out << "\t\t[" << i->first << "]: PostQueue(" << &(i->second) << ")\n";
			++i;

		}

	}

	{
		p_out << "\n\tEvent: " << p_smd.m_event_listener.size() << "\n";
		StateMachineData::EventListenerMap::const_iterator i = p_smd.m_event_listener.begin();

		while(i != p_smd.m_event_listener.end())
		{

			if(i != p_smd.m_event_listener.begin())
			{
				p_out << "\n";
			}

			const StateMachineData::EventListenerList&ell = i->second;
			p_out << "\t\t[" << i->first << "]: EventListenerList(" << &ell << "), " << ell.size() << "\n";
			++i;

			StateMachineData::EventListenerList::const_iterator i = ell.begin();

			while(i != ell.end())
			{
				p_out << "\t\t\t[" << i->first << "]-->" << (i->second)->type() << "(" << i->second << ")\n";
				++i;
			}

		}

	}

	{
		p_out << "\n\tEventListenerReverseMap: " << p_smd.m_event_reverse_map.size() << "\n";
		StateMachineData::EventListenerReverseMap::const_iterator i = p_smd.m_event_reverse_map.begin();

		while(i != p_smd.m_event_reverse_map.end())
		{

			if(i != p_smd.m_event_reverse_map.begin())
			{
				p_out << "\n";
			}

			const StateMachineData::EventReverseMapping& erm = i->second;
			p_out << "\t\tEventListener(" << i->first << ")-->EventReverseMapping(" << &erm << "): " << erm.m_map.size() << "\n";

			{
				StateMachineData::EventReverseMapping::EventMappingMap::const_iterator i = erm.m_map.begin();

				while(i != erm.m_map.end())
				{
					const StateMachineData::EventReverseMapping::EventMapping& emm = (i->second);

					p_out << "\t\t\t" << (i->first)->type() << "(" << i->first << "): event="
						<< (emm.m_event ? *(emm.m_event) : ats::String())
						<< ", key=" << (emm.m_key ? *(emm.m_key) : ats::String())
						<< "\n";

					++i;
				}

			}

			++i;
		}

	}

	p_smd.unlock_event();
	return p_out;
}

void StateMachineData::lock_event() const
{
	pthread_mutex_lock(m_event_mutex);
}

void StateMachineData::unlock_event() const
{
	pthread_mutex_unlock(m_event_mutex);
}
