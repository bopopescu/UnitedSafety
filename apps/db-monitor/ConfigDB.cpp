#include <sstream>
#include <sys/time.h>

#include "ConfigDB.h"

using namespace db_monitor;

ConfigDB::ConfigDB()
{
	init();
}

ConfigDB::ConfigDB(const ats::String& p_db_fname, const ats::String& p_db_key) : DBMonitorContext(p_db_fname, p_db_key)
{
	init();
}

ConfigDB::~ConfigDB()
{
}

void ConfigDB::init()
{
	open_db_config();
}

const ats::String& ConfigDB::open_db_config()
{
	m_error = open_db(db_key(), db_fname());

	if(!m_error.empty())
	{
		return m_error;
	}

	return query(db_key(),
		"create table if not exists t_Config("
			"v_Date bigint,"
			"v_App text not null,"
			"v_Key text not null,"
			"v_Value text,"
			"v_Previous text,"
			"primary key (v_App,v_Key)"
		")");
}

const ats::String& ConfigDB::set_config(const ats::String& p_app, const ats::String& p_key, const ats::String& p_value)
{
	struct timeval t;
	gettimeofday(&t, 0);
	const long long date = (((long long)(t.tv_sec)) * 1000000L) + ((long long)(t.tv_usec));

	const ats::String& app = ats::to_hex(p_app);
	const ats::String& key = ats::to_hex(p_key);
	const ats::String& value = ats::to_hex(p_value);
	std::stringstream s;
	s	<< "insert or replace into t_Config (v_Date,v_App,v_Key,v_Value,v_Previous) values("
		<< date << ",'" << app << "','" << key << "','" << value << "',(select v_Value from t_Config where v_App='" << app << "' and v_Key='" << key << "'))";
	return query(db_key(), s.str());
}

const ats::String& ConfigDB::Set(const ats::String& p_app, const ats::String& p_key, const ats::String& p_value)
{
	return set_config(p_app, p_key, p_value);
}

const ats::String& ConfigDB::Update(const ats::String& p_app, const ats::String& p_key, const ats::String& p_value)
{
	const ats::String& value = Get(p_app, p_key);

	if(value != p_value)
	{
		return Set(p_app, p_key, p_value);
	}

	m_error.clear();
	return m_error;
}

const bool ConfigDB::UpdateB(const ats::String& p_app, const ats::String& p_key, const ats::String& p_value)
{
	const ats::String& value = Get(p_app, p_key);

	if(value != p_value)
	{
		Set(p_app, p_key, p_value);
		return true;
	}

	m_error.clear();
	return false;
}


const ats::String& ConfigDB::ClearAll()
{
	return query(db_key(), "delete from t_Config");
}

const ats::String& ConfigDB::ClearApp(const ats::String& p_app)
{
	const ats::String& app = ats::to_hex(p_app);
	return query(db_key(), "delete from t_Config where v_App='" + app + "'");
}

const ats::String& ConfigDB::ClearKey(const ats::String& p_app, const ats::String& p_key)
{
	const ats::String& app = ats::to_hex(p_app);
	const ats::String& key = ats::to_hex(p_key);
	return query(db_key(), "delete from t_Config where v_App='" + app + "' and v_Key='" + key + "'");
}

const ats::String& ConfigDB::Unset()
{
	return ClearAll();
}

const ats::String& ConfigDB::Unset(const ats::String& p_app)
{
	return ClearApp(p_app);
}

const ats::String& ConfigDB::Unset(const ats::String& p_app, const ats::String& p_key)
{
	return ClearKey(p_app, p_key);
}

// Description: Special case string variables to indiciate when no argument has been passed in (or
//	to ignore an argument). This is just one particular implementation, but it is fine to change
//	this in the future by using "overloaded" functions if this current implementation is not
//	ideal.
static const ats::String g_no_key_so_select_all;
static const ats::String g_no_app_so_select_all;

const ats::String& ConfigDB::h_get_config(const ats::String& p_app, const ats::String& p_key, const ats::String& p_default, bool p_auto_set, bool* p_exists)
{
	const ats::String& app = ats::to_hex(p_app);
	std::stringstream s;

	if(&p_key == &g_no_key_so_select_all)
	{

		if(&p_app == &g_no_app_so_select_all)
		{
			s << "select v_Date,v_Value,v_Previous,v_Key,v_App from t_Config order by v_App";
		}
		else
		{
			s << "select v_Date,v_Value,v_Previous,v_Key from t_Config where v_App='" << app << "'";
		}

	}
	else
	{
		const ats::String& key = ats::to_hex(p_key);

		if(p_auto_set)
		{
			struct timeval t;
			gettimeofday(&t, 0);
			const long long date = (((long long)(t.tv_sec)) * 1000000L) + ((long long)(t.tv_usec));
			const ats::String& value = ats::to_hex(p_default);
			s	<< "insert or ignore into t_Config (v_Date, v_App, v_Key, v_Value) values("
				<< date
				<< ",'" << app << "'"
				<< ",'" << key << "'"
				<< ",'" << value << "'"
				<< ");";
		}

		s << "select v_Date,v_Value,v_Previous from t_Config where v_App='" << app << "' and v_Key='" << key << "'";
	}

	const ats::String& err = query(db_key(), s.str());

	if(!err.empty())
	{
		return err;
	}

	size_t i;

	if(p_exists)
	{
		*p_exists = !m_table.empty();
	}

	for(i = 0; i < m_table.size(); ++i)
	{
		ResultRow& row = m_table[i];
		size_t i;

		for(i = 1; i < row.size(); ++i)
		{
			row[i] = ats::from_hex(row[i]);
		}

	}

	return err;
}

bool ConfigDB::Exists(const ats::String& p_app, const ats::String& p_key)
{
	bool exists;
	h_get_config(p_app, p_key, "", false, &exists);
	return exists;
}

ats::String ConfigDB::Get()
{
	return h_get_config(g_no_app_so_select_all, g_no_key_so_select_all, "", false);
}

ats::String ConfigDB::Get(const ats::String& p_app)
{
	return h_get_config(p_app, g_no_key_so_select_all, "", false);
}

ats::String ConfigDB::Get(const ats::String& p_app, const ats::String& p_key)
{
	return h_get_config(p_app, p_key, "", false);
}

ats::String ConfigDB::Get(const ats::String& p_app, const ats::String& p_key, const ats::String& p_default)
{
	return h_get_config(p_app, p_key, p_default, true);
}

ats::String ConfigDB::GetValue(const ats::String& p_app, const ats::String& p_key)
{

	if(Get(p_app, p_key).empty())
	{
		if(m_table.size() == 1)
		{
			if (m_table[0].size() >= 2)
			{
				return (m_table[0])[1];
			}
		}
	}

	return ats::g_empty;
}

ats::String ConfigDB::GetValue(const ats::String& p_app, const ats::String& p_key, const ats::String& p_default_value)
{
	if (Get(p_app, p_key, p_default_value).empty())
	{
		if (m_table.size() == 1)
		{
			if (m_table[0].size() >= 2)
			{
				return (m_table[0])[1];
			}
		}
	}

	return p_default_value;
}

bool ConfigDB::GetBool(const ats::String& p_app, const ats::String& p_key, const ats::String& p_default_value)
{
	return ats::get_bool(GetValue(p_app, p_key, p_default_value));
}

int ConfigDB::GetInt(const ats::String& p_app, const ats::String& p_key, const ats::String& p_default_value)
{
	return (int)strtol(GetValue(p_app, p_key, p_default_value).c_str(), 0, 0);
}

float ConfigDB::GetFloat(const ats::String& p_app, const ats::String& p_key, const ats::String& p_default_value)
{
	return strtof(GetValue(p_app, p_key, p_default_value).c_str(), 0);
}

double ConfigDB::GetDouble(const ats::String& p_app, const ats::String& p_key, const ats::String& p_default_value)
{
	return strtod(GetValue(p_app, p_key, p_default_value).c_str(), 0);
}

long long ConfigDB::GetLongLong(const ats::String& p_app, const ats::String& p_key, const ats::String& p_default_value)
{
	return strtoll(GetValue(p_app, p_key, p_default_value).c_str(), 0, 0);
}

static const ats::String g_true("1");

bool ConfigDB::GetBool(const ats::String& p_app, const ats::String& p_key, int p_default_value)
{
	return GetBool(p_app, p_key, p_default_value ? g_true : ats::g_empty);
}

int ConfigDB::GetInt(const ats::String& p_app, const ats::String& p_key, int p_default_value)
{
	return GetInt(p_app, p_key, ats::toStr(p_default_value));
}

float ConfigDB::GetFloat(const ats::String& p_app, const ats::String& p_key, float p_default_value)
{
	return GetFloat(p_app, p_key, ats::toStr(p_default_value));
}

double ConfigDB::GetDouble(const ats::String& p_app, const ats::String& p_key, double p_default_value)
{
	return GetDouble(p_app, p_key, ats::toStr(p_default_value));
}

long long ConfigDB::GetLongLong(const ats::String& p_app, const ats::String& p_key, long long p_default_value)
{
	return GetLongLong( p_app, p_key, ats::toStr(p_default_value));
}

const ats::String& ConfigDB::GetKeyList(const ats::String& p_app, std::vector<ats::String>& p_list)
{
	p_list.clear();
	const ats::String& app = ats::to_hex(p_app);
	std::stringstream s;
	s << "select v_Key from t_Config where v_App='" << app << "'";
	const ats::String& err = query(db_key(), s.str());

	if(!err.empty())
	{
		return err;
	}

	size_t i;

	for(i = 0; i < m_table.size(); ++i)
	{
		const ResultRow& row = m_table[i];
		size_t i;

		for(i = 0; i < row.size(); ++i)
		{
			p_list.push_back(ats::from_hex(row[i]));
		}

	}

	return err;
}
