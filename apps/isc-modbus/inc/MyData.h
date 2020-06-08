#pragma once

#include <iostream>
#include <map>
#include <list>
#include <fstream>
#include <vector>

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <syslog.h>
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

#include "DBLock.h"
#include "DBMapping.h"

class MyData : public ats::CommonData
{
public:
	static const ats::String m_default_db_key;

	MyData();

	void lock_sql() const;

	void unlock_sql() const;

	DBMap m_db;
	DBLockMap m_db_lock;

	void garbage_collect_unused_lock(DBLock* p_lock);

	ats::String open_db(const ats::String& p_key, const ats::String& p_db, const ats::String& p_lock);

	ats::String close_db(const ats::String& p_key);

	DBMapping* get_db(const ats::String& p_key) const;

	void put_db(DBMapping* p_dbm);

	ServerData m_command_server[2];
private:
	pthread_mutex_t* m_mutex;
	pthread_mutex_t* m_sql_mutex;
};
