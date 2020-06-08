#include "DBLock.h"

DBLock::DBLock()
{
	m_mutex = new pthread_mutex_t;
	pthread_mutex_init(m_mutex, 0);
}

DBLock::~DBLock()
{
	delete m_mutex;
}

void DBLock::lock()
{
	pthread_mutex_lock(m_mutex);
}

void DBLock::unlock()
{
	pthread_mutex_unlock(m_mutex);
}
