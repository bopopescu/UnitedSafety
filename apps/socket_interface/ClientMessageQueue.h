#pragma once

#include <list>

#include <semaphore.h>

namespace ats
{

template <class T> class ClientMessageManager;

template <class T> class ClientMessageQueue
{
public:

	ClientMessageQueue()
	{
		m_manager = 0;
		m_busy = 0;
		m_reference_count = 0;
		m_sent = 0;

		m_sem = new sem_t;
		sem_init(m_sem, 0, 0);

		m_mutex = new pthread_mutex_t;
		pthread_mutex_init(m_mutex, 0);
	}

	virtual~ ClientMessageQueue()
	{
		sem_destroy(m_sem);
		delete m_sem;

		pthread_mutex_destroy(m_mutex);
		delete m_mutex;
	}

	// Description: A convenience function for inherited classes that wish to synchronize
	//	member variable access (this function is not used by "this" class).
	//
	//	Starts mutual exclusion (lock).
	void lock() const
	{
		pthread_mutex_lock(m_mutex);
	}

	// Description: A convenience function for inherited classes that wish to synchronize
	//	member variable access (this function is not used by "this" class).
	//
	//	Stops mutual exclusion (unlock).
	void unlock() const
	{
		pthread_mutex_unlock(m_mutex);
	}

	typedef long long MessageCount;

	// Description: Number of messages sent.
	MessageCount m_sent;

private:
	friend class ClientMessageManager<T>;

	ClientMessageManager<T>* m_manager;

	pthread_mutex_t* m_mutex;

	std::list<T> m_msg;
	sem_t* m_sem;

	// Description: The line of code that the client is busy on. 0 means not busy.
	//	This is used to prevent deleting an in-use queue.
	int m_busy;

	// Description: Internal variable used to keep track of the number of references to
	//	this message queue (to prevent deleting an in-use queue).
	int m_reference_count;

	ClientMessageQueue& operator =(const ClientMessageQueue&);
	ClientMessageQueue(const ClientMessageQueue&);
};

};
