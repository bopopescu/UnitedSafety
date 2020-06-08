#pragma once

#include <map>

#include <pthread.h>

#include "ats-common.h"

class DBMapping;
class DBLock;

typedef std::map <const ats::String, DBLock*> DBLockMap;
typedef std::pair <const ats::String, DBLock*> DBLockPair;

typedef std::map <DBMapping*, void*> DBPointerMap;
typedef std::pair <DBMapping*, void*> DBPointerPair;

class DBLock
{
public:

	DBLock();

	~DBLock();

	void lock();

	void unlock();

	DBLockMap::iterator m_iter;
	pthread_mutex_t* m_mutex;
	DBPointerMap m_db;
};
