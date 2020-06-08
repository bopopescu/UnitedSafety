#pragma once

#include <vector>

#include "ats-common.h"
#include "db-monitor.h"

class ClientSocket;

namespace db_monitor
{

// Description:
//
// XXX: ConfigDB is not thread safe because it is designed to store data/results within the class object, not
//	in thread-local storage. Always use some type of thread synchronization when using this
//	class in a threaded context.
class ConfigDB : public DBMonitorContext
{
public:
	ConfigDB();

	// Description: Creates a ConfigDB context using database file "p_db_fname" with key
	//	"p_db_key".
	//
	//	If "p_db_fname" is the empty string, then the default database file is used.
	//
	//	If "p_db_key" is the empty string, then the default database key is used.
	ConfigDB(const ats::String& p_db_fname, const ats::String& p_db_key);

	virtual~ ConfigDB();

	const ats::String& open_db_config();

	const ats::String& set_config(const ats::String& p_app, const ats::String& p_key, const ats::String& p_value);

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
  
	// Description: Unset all configuration values.
	const ats::String& ClearAll();

	// Description: Unset all configuration values from application "p_app".
	const ats::String& ClearApp(const ats::String& p_app);

	// Description: Unset the configuration value "p_key" for application "p_app".
	const ats::String& ClearKey(const ats::String& p_app, const ats::String& p_key);

	// Description: Unset all configuration values.
	const ats::String& Unset();

	// Description: Unset all configuration values from application "p_app".
	const ats::String& Unset(const ats::String& p_app);

	// Description: Unset the configuration value "p_key" for application "p_app".
	const ats::String& Unset(const ats::String& p_app, const ats::String& p_key);

	// Description: Sets value "p_value" with key "p_key" in the configuration database.
	//
	// Return: The empty string is returned on success, and an error message is returned otherwise.
	const ats::String& Set(const ats::String& p_app, const ats::String& p_key, const ats::String& p_value);

	// Description: Exactly like "Set" except that the value is set only if "p_value" differs from the current value in
	//	the configuration database. If "p_key" does not exist, then that also is interpreted as "being different".
	//
	// Return: The empty string is returned on success, and an error message is returned otherwise.
	const ats::String& Update(const ats::String& p_app, const ats::String& p_key, const ats::String& p_value);
	const bool UpdateB(const ats::String& p_app, const ats::String& p_key, const ats::String& p_value);

	// Description: Returns true if config value "p_key" exists in the database, and false is returned otherwise.
	bool Exists(const ats::String& p_app, const ats::String& p_key);

	// Description: Returns all configuration values.
	//
	// Return: The empty string is returned on success, and an error message is returned otherwise. On success, the query
	//	results will be in "p_context".
	//
	//         The query results will be in the following format (per row of ConfigDB::m_table):
	//
	//            index  | 0     | 1       | 2          | 3     | 4    |
	//            -------+-------+---------+------------+-------+------+
	//            column |v_Date | v_Value | v_Previous | v_Key |v_App |
	ats::String Get();

	// Description: Returns all configuration values for application "app".
	//
	// Return: The empty string is returned on success, and an error message is returned otherwise. On success, the query
	//	results will be in "p_context".
	//
	//         The query results will be in the following format (per row of ConfigDB::m_table):
	//
	//            index  | 0     | 1       | 2          | 3     |
	//            -------+-------+---------+------------+-------+
	//            column |v_Date | v_Value | v_Previous | v_Key |
	ats::String Get(const ats::String& p_app);

	// Description: Gets the configuration value "p_key" for application "p_app".
	//
	// Return: The empty string is returned on success, and an error message is returned otherwise. On success, the query
	//	results will be in "p_context".
	//
	//         The query results will be in the following format (per row of ConfigDB::m_table):
	//
	//            index  | 0     | 1       | 2          |
	//            -------+-------+---------+------------+
	//            column |v_Date | v_Value | v_Previous |
	ats::String Get(const ats::String& p_app, const ats::String& p_key);

	// Description: Gets the configuration value "p_key" for application "p_app". If no key exists for application "p_app" it will be
	//	added and set to the value "p_default".
	//
	// Return: The empty string is returned on success, and an error message is returned otherwise. On success, the query
	//         results will be in "p_context".
	//
	//         The query results will be in the following format (per row of ConfigDB::m_table):
	//
	//            index  | 0     | 1       | 2          |
	//            -------+-------+---------+------------+
	//            column |v_Date | v_Value | v_Previous |
	ats::String Get(const ats::String& p_app, const ats::String& p_key, const ats::String& p_default);

	// Description: Convenience function that just returns the config value.
	//
	// Return: Returns the empty string on error, and the value for key "p_key" otherwise.
	ats::String GetValue(const ats::String& p_app, const ats::String& p_key);

	// Description: Convenience function that just returns the config value, or the default value "p_default_value" if
	//	a config value could not be read.
	//
	// NOTE: The Empty string is considered to be a no-value response, and so "p_default_value" will be returned instead.
	//	In other words, this function only returns the empty string on error conditions (or when "p_default_value" itself
	//	is the empty string).
	ats::String GetValue(const ats::String& p_app, const ats::String& p_key, const ats::String& p_default_value);

	// Description: Convenience function that just returns the config value as an integer, or the default value "p_default_value" if
	//	a config value could not be read.
	//
	// NOTE: The Empty string (from "db_monitor::get_config") is considered to be a no-value response, and so "p_default_value" will be
	//	returned instead of integer 0.
	//
	// Return: The integer with key "p_key" or the value in "p_default_value" if "p_key" did not exist.
	int GetInt(const ats::String& p_app, const ats::String& p_key, const ats::String& p_default_value);
	int GetInt(const ats::String& p_app, const ats::String& p_key, int p_default_value);

	// Description: Exactly like "GetInt" except that this function deals with boolean values.
	//
	//	NOTE: Boolean string values are converted according to "ats::get_bool". See "ats::get_bool" for
	//	more information.
	bool GetBool(const ats::String& p_app, const ats::String& p_key, const ats::String& p_default_value);
	bool GetBool(const ats::String& p_app, const ats::String& p_key, int p_default_value);

	// Description: Exactly like "GetInt" except that this function deals with floating point values.
	float GetFloat(const ats::String& p_app, const ats::String& p_key, const ats::String& p_default_value);
	float GetFloat(const ats::String& p_app, const ats::String& p_key, float p_default_value);
  
	// Description: Exactly like "GetInt" except that this function deals with double precision floating point values.
	double GetDouble(const ats::String& p_app, const ats::String& p_key, const ats::String& p_default_value);
	double GetDouble(const ats::String& p_app, const ats::String& p_key, double p_default_value);

	// Description: Exactly like "GetInt" except that this function deals with 64 bit integers.
	long long GetLongLong(const ats::String& p_app, const ats::String& p_key, const ats::String& p_default_value);
	long long GetLongLong(const ats::String& p_app, const ats::String& p_key, long long p_default_value);

	// Description: Returns a list of all keys (in "p_list") for application "p_app".
	//
	// Return: The empty string is returned on success and "p_list" will contain a list of all
	//	keys for application "p_app". "p_list" will be empty if there is no application "p_app"
	//	or it has no keys. An error message is returned on error and "p_list" will be empty.
	const ats::String& GetKeyList(const ats::String& p_app, std::vector<ats::String>& p_list);

private:
	const ats::String& h_get_config(const ats::String& p_app, const ats::String& p_key, const ats::String& p_default, bool p_auto_set, bool* p_exists=0);

	void init();

	ConfigDB(const ConfigDB&);

	ConfigDB& operator =(const ConfigDB&);
};

}
