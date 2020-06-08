#include <iostream>
#include <map>
#include <list>
#include <fstream>
#include <vector>

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/ioctl.h>
#include <sqlite3.h>

#include "ats-common.h"
#include "socket_interface.h"
#include "command_line_parser.h"
#include "db-monitor.h"

#include "MyData.h"
#include "DBMapping.h"

DBMapping::DBMapping(MyData& p_data, const ats::String& p_file, sqlite3* p_db)
{
	m_data = &p_data;
	m_close_database = false;
	m_ref_count = 0;
	m_file = p_file;
	m_db = p_db;
	m_lock = 0;
}

DBMapping::~DBMapping()
{

	if(m_db)
	{
		sqlite3_close(m_db);
	}

	remove_lock_manager();
}

DBLock* DBMapping::set_lock_manager(const ats::String& p_lock)
{

	if(m_ref_count)
	{
		return 0;
	}

	DBLockMap::iterator i = m_data->m_db_lock.find(p_lock);

	if(m_data->m_db_lock.end() == i)
	{
		i = (m_data->m_db_lock.insert(DBLockPair(p_lock, new DBLock()))).first;
		(i->second)->m_iter = i;
	}

	return set_lock_manager( *(i->second));
}

DBLock* DBMapping::set_lock_manager(DBLock& p_lock)
{

	if(m_ref_count)
	{
		return 0;
	}

	if(m_lock != &p_lock)
	{
		remove_lock_manager();
		m_lock = &p_lock;
		m_lock->m_db.insert(DBPointerPair(this, NULL));
	}

	return m_lock;
}

void DBMapping::remove_lock_manager()
{

	if(m_lock)
	{
		DBPointerMap::iterator i = m_lock->m_db.find(this);

		if(i != m_lock->m_db.end())
		{
			m_lock->m_db.erase(i);
		}

		m_data->garbage_collect_unused_lock(m_lock);
		m_lock = 0;
	}

}

void DBMapping::lock() const
{
	m_lock->lock();
}

void DBMapping::unlock() const
{
	m_lock->unlock();
}

const ats::String& DBMapping::get_lock_name() const
{

	if(m_lock)
	{
		return m_lock->m_iter->first;
	}

	return ats::g_empty;
}
