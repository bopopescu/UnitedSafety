#pragma once

#include <vector>

#include "ats-common.h"

class ClientSocket;

namespace db_monitor
{

typedef std::vector <ats::String> ResultRow;
typedef std::vector <ResultRow> ResultTable;

// Description:
//
// XXX: DBMonitorContext is not thread safe because it is designed to store data/results within the class object, not
//	in thread-local storage. Always use some type of thread synchronization when using this
//	class in a threaded context.
class DBMonitorContext
{
protected:
	ResultRow m_column;
	ResultTable m_table;
	ats::String m_result;
	ats::String m_error;
	ats::String m_db_fname;
	ats::String m_db_key;
	ClientSocket* m_cs;

public:
	// Description: Creates a DBMonitorContext.
	DBMonitorContext();

	// Description: Creates a DBMonitorContext and sets the default database key and file to be "p_db_key" and "p_db_fname" respectively.
	//
	//	The empty string can be passed in for "p_db_key" and "p_db_fname".
	//
	//	"p_db_key" and "p_db_fname" are used for "convenience" by having DBMonitorContext automatically open the keyed database
	//	(with the supplied file name). If these values are empty strings, then there will be no automatic database open (caller will
	//	have to manually open the database using the "open_db" call).
	DBMonitorContext(const ats::String& p_db_key, const ats::String& p_db_fname);

	virtual~ DBMonitorContext();

	const ats::String& db_fname() const;

	const ats::String& db_key() const;

	// Description: Opens a database. If a database is already opened with key "p_db_key" and the database
	//	file keyed is not the same as "p_db_fname", then a duplicate key error is returned. If a database is
	//	already opened with key "p_db_key" and the database file keyed is the same as "p_db_fname", then
	//	it is not an error, and nothing is done (since the database is already opened).
	//
	//	ATS FIXME: The below behavior is not currently implemented.
	//	A file that is already opened by an existing key cannot be opened by a new key. An error will be
	//	returned if an open on the same file is attempted.
	//
	// Parameters:
	//    p_context - Contains the connection to the database
	//    p_db_key  - The name/key of the database
	//    p_db_fname  - The location of the database in the file system
	//
	// Return: The empty string is returned on success, and an error message is returned otherwise.
	const ats::String& open_db(const ats::String& p_db_key, const ats::String& p_db_fname);

	// Description: Closes the named database.
	const ats::String& close_db(const ats::String& p_db_key);

	const ats::String& close_db();

	const ats::String& connect(bool p_open_db=true);

	// Description: Disconnects (closes) the socket connection to db-monitor.
	void disconnect();

	void reconnect();

	// Description: Performs an execute or query on the database "p_db_key". If "p_db_key" is the empty string, then
	//	the system default database is used.
	//
	// Parameters:
	//    p_context          - Contains the connection to the database and stores query results
	//    p_db_key           - The name/key of the database
	//    p_query            - The SQL query (in Sqlite3 format)
	//    p_max_query_length - The maximum length that an SQL query result can have. If the query result is greater than this
	//                         an error message is returned.
	//
	// Return: The empty string is returned on success, and an error message is returned otherwise. On success, the query
	//	results will be in "p_context".
	const ats::String& query(const ats::String& p_db_key, const ats::String& p_query, size_t p_max_query_length=1000000);

	const ats::String& query(const ats::String& p_query, size_t p_max_query_length=1000000);

	ResultTable& Table()
	{
		return m_table;
	}

	const ResultTable& Table() const
	{
		return m_table;
	}

	ResultRow& Column()
	{
		return m_column;
	}

	const ResultRow& Column() const
	{
		return m_column;
	}
  
private:
	void init();

	DBMonitorContext(const DBMonitorContext&);

	DBMonitorContext& operator =(const DBMonitorContext&);
};

}
