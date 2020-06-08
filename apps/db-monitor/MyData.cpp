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

const ats::String MyData::m_default_db_key = "default.db";

MyData::MyData()
{
	m_mutex = new pthread_mutex_t;
	pthread_mutex_init(m_mutex, 0);

	m_sql_mutex = new pthread_mutex_t;
	pthread_mutex_init(m_sql_mutex, 0);
}

void MyData::lock_sql() const
{
	pthread_mutex_lock(m_sql_mutex);
}

void MyData::unlock_sql() const
{
	pthread_mutex_unlock(m_sql_mutex);
}

void MyData::garbage_collect_unused_lock(DBLock* p_lock)
{

	if(p_lock && (p_lock->m_db.empty()))
	{
		m_db_lock.erase(p_lock->m_iter);
		delete p_lock;
	}

}

ats::String MyData::open_db(const ats::String& p_key, const ats::String& p_db, const ats::String& p_lock)
{
	lock_sql();

	DBMap::const_iterator i = m_db.find(p_key);

	if(i != m_db.end())
	{
		DBMapping* dbm = i->second;

		bool open_modified = false;

		if(!(p_lock.empty()) && (p_lock != dbm->get_lock_name()))
		{

			if(dbm->set_lock_manager(p_lock))
			{
				open_modified = true;
			}

		}

		if(dbm->m_close_database)
		{
			dbm->m_close_database = false;
			unlock_sql();
			return ats::String();
		}

		if(open_modified)
		{
			unlock_sql();
			return ats::String();
		}

		const ats::String current_file((i->second)->m_file);
		const bool is_same = (p_db == current_file);
		unlock_sql();

		return is_same ? ats::String() : ("db key \"" + p_key + "\" is already opened to file \"" + current_file + "\"");
	}

	sqlite3* db;
	const int rc = sqlite3_open(p_db.c_str(), &db);

	if(rc != SQLITE_OK)
	{
		const ats::String& err = ("can't open database \"" + p_db + "\": ") + sqlite3_errmsg(db);
		sqlite3_close(db);
		unlock_sql();
		return err;
	}

	DBMapping* dbm = new DBMapping(*this, p_db, db);
	dbm->m_iter = (m_db.insert(DBPair(p_key, dbm))).first;

	{
		const ats::String& lock_key = p_lock.empty() ? p_key : p_lock;

		DBLockMap::iterator i = m_db_lock.find(lock_key);

		if(m_db_lock.end() == i)
		{
			i = (m_db_lock.insert(DBLockPair(lock_key, new DBLock()))).first;
			(i->second)->m_iter = i;
		}

		DBLock& l = *(i->second);
		dbm->set_lock_manager(l);
	}

	unlock_sql();
	return ats::String();
}

ats::String MyData::close_db(const ats::String& p_key)
{
	lock_sql();
	DBMap::iterator i = m_db.find(p_key);

	if(m_db.end() == i)
	{
		unlock_sql();
		return "There is no database opened for key \"" + p_key + "\"";
	}

	DBMapping* dbm = i->second;
	dbm->m_close_database = true;

	if(dbm->m_ref_count)
	{
		unlock_sql();
		return ats::String();
	}

	delete dbm;
	m_db.erase(i);
	unlock_sql();
	return ats::String();
}

DBMapping* MyData::get_db(const ats::String& p_key) const
{
	lock_sql();
	DBMap::const_iterator i = m_db.find(p_key);

	if((m_db.end() == i) && p_key.empty())
	{
		i = m_db.find(m_default_db_key);
	}

	if(m_db.end() != i)
	{
		DBMapping* dbm = i->second;
		dbm->m_ref_count++;
		unlock_sql();
		return dbm;
	}

	unlock_sql();
	return 0;
}

void MyData::put_db(DBMapping* p_dbm)
{

	if(p_dbm)
	{
		lock_sql();
		p_dbm->m_ref_count--;

		if(!(p_dbm->m_ref_count) && p_dbm->m_close_database)
		{
			m_db.erase(p_dbm->m_iter);
			delete p_dbm;
		}

		unlock_sql();
	}

}
