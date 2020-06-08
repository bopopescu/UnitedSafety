#pragma once

#include <map>

#include <semaphore.h>

#include "ats-common.h"
#include "ClientMessageQueue.h"

namespace ats
{

template <class T> class ClientMessageManager
{
public:
	typedef std::map <const ats::String, ClientMessageQueue<T>* > ClientMap;
	typedef std::pair <const ats::String, ClientMessageQueue<T>* > ClientPair;
	typedef typename ClientMap::const_iterator ClientMapConstIterator;
	typedef typename ClientMap::iterator ClientMapIterator;

	typedef std::map <ClientMessageQueue<T>*, void *> ClientPtrMap;
	typedef std::pair <ClientMessageQueue<T>*, void *> ClientPtrPair;
	typedef typename ClientPtrMap::const_iterator ClientPtrMapConstIterator;
	typedef typename ClientPtrMap::iterator ClientPtrMapIterator;

	typedef std::list <T> MessageList;
	typedef typename MessageList::const_iterator MessageListConstIterator;
	typedef typename MessageList::iterator MessageListIterator;

	ClientMessageManager()
	{
		m_mutex = new pthread_mutex_t;
		pthread_mutex_init(m_mutex, 0);
	}

	// Description:
	//
	// XXX: Do NOT call this desctructor until all clients have been removed (destroyed) from
	//      the manager.
	virtual~ ClientMessageManager()
	{
		pthread_mutex_destroy(m_mutex);
		delete m_mutex;
	}

	// Description: Returns a client message queue with key "p_client". If no client exists with
	//	the given key, then NULL is returned. If a client message queue is returned, then the
	//	reference count for the message queue is incremented by one. "put_client" must be
	//	called on the client to restore the reference count.
	//
	// XXX: Must call "put_client" when finished with the client (to restore the reference count).
	//      A non-zero reference count will prevent the client message queue from being destroyed,
	//      and will result in a resource leak (if the message queue is no longer being used).
	//
	// Return: A client message queue on success, and NULL otherwise.
	ClientMessageQueue<T>* get_client(const ats::String& p_client) const
	{
		lock();
		ClientMapConstIterator i = m_client.find(p_client);
		ClientMessageQueue<T>* q = 0;

		if(i != m_client.end())
		{
			q = i->second;
			++(q->m_reference_count);
		}

		unlock();
		return q;
	}

	// Description: Decrements the client message queue reference count by one for the given client
	//	(only if the given client belongs to "this" message manager, and the reference count is
	//	greater than 0).
	//
	// Return: True is returned if the reference count was decremented, and false is returned
	//	otherwise.
	bool put_client(ClientMessageQueue<T>* p_q) const
	{
		bool b = false;

		if(p_q && (this == p_q->m_manager))
		{
			lock();

			if((p_q->m_reference_count) > 0)
			{
				--(p_q->m_reference_count);
				b = true;
			}

			unlock();
		}

		return b;
	}

	// Description: Adds client "p_client" to "this" message manager with key "p_key".
	//
	//	If "p_client" is already managed, then NULL is returned, and "p_client" is untouched.
	//
	//	Otherwise if a client with key "p_key" already exists (and "p_client" is not managed),
	//	then "p_client" is deleted, and NULL is returned.
	//
	//	If "p_client" with key "p_key" is added, then "p_client" is returned (its message
	//	manager is set to "this" manager).
	//
	// XXX: Message manager will "own" all "p_client" pointers passed to it (that means it will
	//      control the memory and will perform the free/delete), EXCEPT in the case where
	//	"p_client" already has a manager, then that manager will control "p_client". In short,
	//	do not use "p_client" after passing it to this function, but instead use the returned
	//	pointer, which will be "p_client" itself on success, or NULL on error (where "p_client"
	//	was destroyed).
	//
	// Return: "p_client" is returned on success, and NULL is returned on error.
	ClientMessageQueue<T>* add_client(const ats::String& p_key, ClientMessageQueue<T>* p_client)
	{

		if(p_client)
		{
			lock();

			if(p_client->m_manager)
			{
				unlock();
				return 0;
			}

			if(!(m_client.insert(ClientPair(p_key, p_client)).second))
			{
				unlock();
				delete p_client;
				return 0;
			}

			p_client->m_manager = this;
			unlock();
		}

		return p_client;
	}

	// Description: Exactly like "add_client" defined above (which takes a pre-created ClientMessageQueue<T> pointer).
	//	The difference is that this function does not take a pre-created ClientMessageQueue<T> pointer, but instead
	//	will call "p_create_fn" to create the pointer only if it is needed. The pointer will not be needed if
	//	there already exists a client queue with key "p_key".
	//
	//	In short, this function avoids the caller from having to create a new ClientMessageQueue<T> pointer that
	//	will simply be deleted immediately because there already exists a client with key "p_key".
	//
	//	"p_create_fn" is expected to return a new ClientMessageQueue<T> pointer, or NULL when called.
	//	The function "p_create_fn" must NOT access "this" class. In addition, the queue will be locked, which
	//	means that "p_create_fn" will be running in a critical section and should therefore return as soon as
	//	possible.
	//
	//	   Example "p_create_fn":
	//	      ClientMessageQueue<int>* example_create_fn() { return new ClientMessageQueue<int>();}
	//
	ClientMessageQueue<T>* add_client(const ats::String& p_key, ClientMessageQueue<T>* (*p_create_fn)())
	{
		ClientMessageQueue<T>* p_client = 0;

		if(p_create_fn)
		{
			lock();
			std::pair <ClientMapIterator, bool> r = m_client.insert(ClientPair(p_key, 0));

			if(!r.second)
			{
				unlock();
				return p_client;
			}

			if(!((r.first)->second = p_create_fn()))
			{
				m_client.erase(r.first);
				unlock();
				return p_client;
			}

			p_client = (r.first)->second;
			p_client->m_manager = this;
			unlock();
		}

		return p_client;
	}

	// Description: Removes client "p_client" from the message manager.
	//
	// Return: The empty string is returned on success, and an error message is returned
	//	otherwise. If an error occurs, then the client is still in the message
	//	manager list (and may still process messages/do its usual work).
	//
	// Error Messages:
	//
	//      "busy" - The client message queue is waiting/sleeping on a message semaphore.
	//               The "signal_client" function can be used on the client to wake it up and
	//               make it not busy. However it is up to the code following the client wakeup
	//               to check if it needs to terminate (stop waiting on messages) so that a
	//               future call to "remove_client" can properly remove it.
	ats::String remove_client(const ats::String& p_client)
	{
		lock();
		ClientMapIterator i = m_client.find(p_client);

		if(i != m_client.end())
		{
			h_stop_client(i);
			ClientMessageQueue<T>& q = *(i->second);

			if(q.m_busy)
			{
				unlock();
				return "busy";
			}

			delete &q;
			m_client.erase(i);
		}

		unlock();
		return ats::String();
	}

	// Description: Send message "p_msg" to all client message queues (broadcast message).
	//
	// XXX: Messages are queued (posted in order of call) for all actively listening clients.
	void post_msg(const T& p_msg)
	{
		lock();

		ClientPtrMapConstIterator i = m_running.begin();

		while(i != m_running.end())
		{
			ClientMessageQueue<T>& q = *(i->first);
			++i;

			q.m_msg.push_back(p_msg);
			sem_post(q.m_sem);
		}

		unlock();
	}

	// Description: Send message "p_msg" to client "p_client".
	//
	// XXX: Messages are queued (posted in order of call).
	void post_msg(const T& p_msg, const ats::String& p_client)
	{
		lock();

		ClientMapConstIterator i = m_client.find(p_client);

		if(i != m_client.end())
		{
			ClientPtrMapConstIterator j = m_running.find(i->second);

			if(j != m_running.end())
			{
				ClientMessageQueue<T>& q = *(j->first);
				q.m_msg.push_back(p_msg);
				sem_post(q.m_sem);
			}

		}

		unlock();
	}

	// Description: Signals client "p_client". This will cause "p_client" to stop its
	//	wait/sleep on a message event (if it is sleeping) by sending a "no-data"
	//	event to the client.
	//
	// XXX: If the client is not currently waiting on a message, then calling this function
	//      will cause the client to immediately exit a wait/sleep and see no data (for
	//      the very next time it waits/sleeps for a message). This is due to the fact that
	//      the message semaphore is incremented, but no data is added. So the client will
	//      see a "no-data" event (in which case it should understand that it is being
	//      signaled by an external thread/process thread so it can handle a non-data related
	//      event).
	void signal_client(const ats::String& p_client)
	{
		lock();

		ClientMapConstIterator i = m_client.find(p_client);

		if(i != m_client.end())
		{
			sem_post((i->second)->m_sem);
		}

		unlock();
	}

	// Description: Causes the caller to block and wait/sleep until a message arrives for
	//	client "p_client" (where the message will be stored in "p_msg"). The caller
	//	will also stop waiting/sleeping if "signal_client" was called on "p_client".
	//
	// XXX: Messages (if any) are received in order. If there is already a queued message
	//      for client "p_client", then this function will returned immediately, and the
	//      message will be stored in "p_msg". This function will also return immediately
	//      is "signal_client" has already been called on "p_client".
	//
	// Return: True is returned if a message was received, and false is returned if no
	//	message was received. If false is returned, then the contents of "p_msg" are
	//	undefined.
	bool wait_msg(const ats::String& p_client, T& p_msg)
	{
		ClientMessageQueue<T>* q;

		lock();
		{
			ClientMapConstIterator i = m_client.find(p_client);

			if(m_client.end() == i)
			{
				unlock();
				return false;
			}

			q = i->second;
			q->m_busy = __LINE__;
		}
		unlock();

		sem_wait(q->m_sem);

		lock();
		{
			q->m_busy = 0;

			if(q->m_msg.empty())
			{
				unlock();
				return false;
			}

			p_msg = *(q->m_msg.begin());
			q->m_msg.pop_front();
			++(q->m_sent);
		}
		unlock();

		return true;
	}

	// Description: Allows client "p_client" to start receiving messages (moves client
	//	"p_client" into the "run-queue".
	//
	// XXX: By default, clients are not in the "run-queue", and so do not receive messages
	//      addressed to them.
	void start_client(const ats::String& p_client)
	{
		lock();
		ClientMapConstIterator i = m_client.find(p_client);

		if(i != m_client.end())
		{
			m_running.insert(ClientPtrPair(i->second, 0));
		}

		unlock();
	}

	// Description: Stops client "p_client" from receiving messages (removes client "p_client"
	//	from the "run-queue").
	void stop_client(const ats::String& p_client)
	{
		lock();
		h_stop_client(m_client.find(p_client));
		unlock();
	}

	ats::String m_line_prefix;

	friend std::ostream& operator<<(std::ostream& p_o, const ClientMessageManager<T>& p_cmm)
	{
		return p_cmm.print(p_o, ats::StringMap());
	}

	// Description: Prints the current state of the ClientMessagManager (including all client message
	//	queues).
	//
	//	"p_opt" supports the following options:
	//	   "max_msg" is the maximum number of messages to print.
	//	   "client" limits the message queue output to just the named client. If this is not specified
	//	            or is the empty string, then all client message queues are displayed.
	//
	// XXX: This function may only be used if "std::ostream << T" has been defined (in other words,
	//	a standard input/output stream function for printing template type "T" must be defined,
	//	otherwise compiler errors will result. The errors will be the compiler indicating that
	//	it does not have a conversion/implementation for "std::ostream << T").
	std::ostream& print(std::ostream& p_o, const ats::StringMap& p_opt) const
	{
		lock();
		const ats::String& lp = m_line_prefix;

		p_o << lp << "ClientMessageManager(" << this << "): " << m_client.size() << " client" << (m_client.size() == 1 ? "" : "s") << "\n";

		int max_messages = p_opt.get_int("max_msg");
		max_messages = max_messages < 0 ? 0 : max_messages;

		const size_t last_half = size_t((max_messages/2) + ((max_messages % 2) ? 1 : 0));

		const ats::String* only_show_client = p_opt.has_key("client") ? (&((p_opt.find("client"))->second)) : 0;
		ClientMapConstIterator i = (only_show_client && (!(only_show_client->empty()))) ? m_client.find(*only_show_client) : m_client.begin();

		if(only_show_client && only_show_client->empty())
		{
			only_show_client = 0;
		}

		while(i != m_client.end())
		{
			ClientMessageQueue<T>* c = i->second;
			p_o << lp << "\t" << i->first << "(" << i->second << "): " << c->m_msg.size() << " message" << ((c->m_msg.size() == 1) ? "" : "s") << " (" << c->m_sent << " sent)\n";

			++i;

			MessageListConstIterator i = c->m_msg.begin();
			size_t msg_number = 0;
			size_t max_msg = size_t(max_messages);

			while(i != c->m_msg.end())
			{
				const T& t = *i;
				++i;
				p_o << lp << "\t\t[" << msg_number << "] " << t << "\n";
				++msg_number;

				if(max_msg && (max_msg < c->m_msg.size()))
				{

					if(last_half == msg_number)
					{
						size_t count = max_msg - last_half;

						if(count)
						{
							p_o << lp << "\t\t\t...\n";
						}

						msg_number = c->m_msg.size() - count;
						i = c->m_msg.end();

						for(; count; --count)
						{
							--i;
						}

						max_msg = 0;
					}
				
				}

			}

			if(only_show_client)
			{
				break;
			}

		}
		
		unlock();
		return p_o;
	}

private:
	// Description: A mapping/list of all clients managed by the message manager.
	ClientMap m_client;

	// Description: A mapping/list of all clients actively listening for (accepting) messages.
	ClientPtrMap m_running;

	pthread_mutex_t* m_mutex;

	// Description: See "stop_client". This is a helper for "stop_client".
	void h_stop_client(ClientMapConstIterator p_i)
	{
		ClientMapConstIterator i = p_i;

		if(i != m_client.end())
		{
			ClientPtrMapIterator j = m_running.find(i->second);

			if(j != m_running.end())
			{
				m_running.erase(j);
			}

		}

	}

	void lock() const
	{
		pthread_mutex_lock(m_mutex);
	}

	void unlock() const
	{
		pthread_mutex_unlock(m_mutex);
	}

	ClientMessageManager& operator =(const ClientMessageManager&);
	ClientMessageManager(const ClientMessageManager&);
};

};
