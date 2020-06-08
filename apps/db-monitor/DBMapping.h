#pragma once

#include <map>
#include "ats-common.h"

class DBMapping;
class DBLock;
class MyData;

typedef std::map <const ats::String, DBMapping*> DBMap;
typedef std::pair <const ats::String, DBMapping*> DBPair;

typedef std::map <const ats::String, DBLock*> DBLockMap;
typedef std::pair <const ats::String, DBLock*> DBLockPair;

typedef std::map <DBMapping*, void*> DBPointerMap;
typedef std::pair <DBMapping*, void*> DBPointerPair;

class DBMapping
{
public:
	DBMap::iterator m_iter;
	ats::String m_file;
	MyData* m_data;
	sqlite3* m_db;
	DBLock* m_lock;
	size_t m_ref_count;
	bool m_close_database;

	DBMapping(MyData& p_data, const ats::String& p_file, sqlite3* p_db);

	~DBMapping();

	// XXX: Only class MyData my call these functions. The "MyData::lock_sql()" lock
	//	must be held before calling these functions.
	DBLock* set_lock_manager(const ats::String& p_lock);

	DBLock* set_lock_manager(DBLock& p_lock);

	const ats::String& get_lock_name() const;
	/////////////////////////////////////////////////////////////////////

	void lock() const;

	void unlock() const;

private:
	void remove_lock_manager();

	DBMapping(const DBMapping&);
	DBMapping& operator =(const DBMapping&);
};
