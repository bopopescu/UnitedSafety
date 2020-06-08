
#include <errno.h>
#include <string.h>
#include <sys/time.h>

#include "can-j1939-db.h"

static const ats::String& g_db_pgn_name(J1939_PGN_DB_FNAME);
static const ats::String& g_db_pgn_key("pgndb");
static const ats::String& g_db_config_name(J1939_CONFIG_DB_FNAME);
static const ats::String& g_db_config_key("configdb");
static const ats::String& g_db_ong_name(J1939_ONG_DB_FNAME);
static const ats::String& g_db_ong_key("ongdb");

static ats::String g_no_app_so_select_all;
CanJ1939DB::CanJ1939DB()
{
	m_cs = new ClientSocket;
	init_ClientSocket(m_cs);
	connect_redstone_ud_client(m_cs, "db-monitor");
	m_sockfd = m_cs->m_fd;
}

CanJ1939DB::~CanJ1939DB()
{
	if(m_cs)
	{
		close_ClientSocket(m_cs);
		delete m_cs;
	}
}

static void h_open_db_parse_first_line(bool& p_first_line, ats::String& p_result, ats::String& p_error, ats::String& p_bad_response_format)
{

	if(!p_first_line)
	{
		return;
	}

	p_first_line = false;
	size_t i = p_result.find(':');

	if(ats::String::npos == i)
	{
		p_bad_response_format = p_result;
	}

	i = p_result.find(" ok", i + 1);

	if(ats::String::npos == i)
	{
		i = p_result.find(" error", i + 1);

		if(ats::String::npos == i)
		{
			p_error = p_result;
		}
		else
		{
			p_error = p_result.substr(i + 1 + strlen(" error"));
		}

	}

}

ats::String CanJ1939DB::open_db(const ats::String& p_db_key, const ats::String& p_db_loc)
{
	const ats::String& cmd = "open_db \"" + p_db_key + "\" \"" + p_db_loc + "\"\r";

	send_cmd(m_sockfd, MSG_NOSIGNAL, "%s", cmd.c_str());

	bool first_line = true;
	ats::String bad_response_format;
	bool command_too_long = false;
	ats::String result;
	ats::String error;

	const size_t max_query_length = 32 * 1024;

	for(;;)
	{
		char c;
		const ssize_t nread = recv( m_sockfd, &c, 1, 0);

		if(!nread)
		{
			return "unexpected end of input before reading carriage return";
		}

		if(nread < 0)
		{
			return ats::String("error reading from server: ") + strerror(errno);
		}

		if('\n' == c)
		{

			h_open_db_parse_first_line(first_line, result, error, bad_response_format);

			result.clear();

		}
		else if(c != '\r')
		{

			if(result.length() >= max_query_length)
			{
				command_too_long = true;
			}
			else
			{
				result += c;
			}

			continue;
		}
		else
		{
			h_open_db_parse_first_line(first_line, result, error, bad_response_format);

			if(command_too_long)
			{
				return "result is too large";
			}

			if(!bad_response_format.empty())
			{
				return "bad response format: (" + bad_response_format + ")";
			}

			if(!error.empty())
			{
				return error;
			}

			break;
		}

	}

	return error;
}

ats::String CanJ1939DB::query(const ats::String &p_db, const ats::String& p_query, size_t p_max_query_length)
{
	m_column.clear();
	m_table.clear();

	const ats::String& query = "sql db=\"" + p_db + "\" query=X" + ats::to_hex(p_query) + "\r";

	send_cmd(m_sockfd, MSG_NOSIGNAL, "%s", query.c_str());

	bool reading_columns = true;
	bool first_line = true;
	ats::String bad_response_format;
	bool command_too_long = false;
	ats::String result;
	ats::String error;

	for(;;)
	{
		char c;
		const ssize_t nread = recv( m_sockfd, &c, 1, 0);

		if(!nread)
		{
			return "unexpected end of input before reading carriage return";
		}

		if(nread < 0)
		{
			return ats::String("error reading from server: ") + strerror(errno);
		}

		if('\n' == c)
		{

			if(first_line)
			{
				first_line = false;
				size_t i = result.find(':');

				if(ats::String::npos == i)
				{
					bad_response_format = result;
				}

				i = result.find(" ok", i + 1);

				if(ats::String::npos == i)
				{
					i = result.find(" error", i + 1);

					if(ats::String::npos == i)
					{
						error = result;
					}
					else
					{
						error = ats::from_hex(result.substr(i + 1 + strlen(" error")));
					}

				}

			}
			else
			{

				if(reading_columns)
				{
					reading_columns = false;
					ats::String::const_iterator i = result.begin();
					ats::String column;

					while(i != result.end())
					{
						const char c = *i;
						++i;

						if(' ' == c)
						{
							m_column.push_back(ats::from_hex(column.c_str() + 1));
							column.clear();
						}
						else
						{
							column += c;
						}

					}

					if(!column.empty())
					{
						m_column.push_back(ats::from_hex(column.c_str() + 1));
					}

				}
				else
				{
					ats::String::const_iterator i = result.begin();
					ats::String value;
					ResultRow *row = 0;

					while(i != result.end())
					{
						const char c = *i;
						++i;

						if(' ' == c)
						{

							if(!row)
							{
								m_table.push_back(ResultRow());
								row = &(m_table[m_table.size() - 1]);
								row->reserve(m_column.size());
							}

							row->push_back(ats::from_hex(value.c_str() + 1));
							value.clear();
						}
						else
						{
							value += c;
						}

					}

					if(!value.empty())
					{

						if(!row)
						{
							m_table.push_back(ResultRow());
							row = &(m_table[m_table.size() - 1]);
						}

						row->push_back(ats::from_hex(value.c_str() + 1));
					}

				}

			}

			result.clear();

		}
		else if(c != '\r')
		{

			if(result.length() >= p_max_query_length)
			{
				command_too_long = true;
			}
			else
			{
				result += c;
			}

			continue;
		}
		else
		{

			if(command_too_long)
			{
				return "result is too large";
			}

			if(!bad_response_format.empty())
			{
				return "bad response format: (" + bad_response_format + ")";
			}

			if(!error.empty())
			{
				return error;
			}

			break;
		}

	}

	return ats::String();
}

ats::String CanJ1939DB::open_db_ong()
{
	{
		const ats::String& err = open_db(g_db_ong_key, g_db_ong_name);

		if(!err.empty())
		{
			return err;
		}
	}

	{
		const ats::String& err= query(g_db_ong_key,
				"create table if not exists t_xml("
				"v_Date bigint,"
				"v_App text not null unique,"
				"v_Value text not null,"
				"primary key (v_App)"
				")");

		if(!err.empty())
		{
			return err;
		}

	}

	return ats::String();
}

ats::String CanJ1939DB::open_db_config()
{
	{
		const ats::String& err = open_db(g_db_pgn_key, g_db_pgn_name);

		if(!err.empty())
		{
			return err;
		}
	}

	{
		const ats::String& err= query(g_db_pgn_key,
			"create table if not exists t_xml("
			"v_Date bigint,"
			"v_App text not null unique,"
			"v_Value text not null,"
			"primary key (v_App)"
			")");

		if(!err.empty())
		{
			return err;
		}

	}
	{
		const ats::String& err = open_db(g_db_config_key, g_db_config_name);

		if(!err.empty())
		{
			return err;
		}
	}

	{
		const ats::String& err= query(g_db_config_key,
			"create table if not exists t_xml("
			"v_Date bigint,"
			"v_App text not null unique,"
			"v_Value text not null,"
			"primary key (v_App)"
			")");

		if(!err.empty())
		{
			return err;
		}

	}

	return ats::String();
}

ats::String CanJ1939DB::set_config(const ats::String &p_db, const ats::String& p_app, const ats::String& p_value)
{
	const ats::String& app = ats::to_hex(p_app);
	const ats::String& value = ats::to_hex(p_value);

	std::stringstream s;
	struct timeval t;
	gettimeofday(&t, 0);
	const long long date = (((long long)(t.tv_sec)) * 1000000L) + ((long long)(t.tv_usec));
	s	<< "insert or replace into t_xml (v_Date, v_App, v_Value) values("
		<< date
		<< ",'" << app << "'"
		<< ",'" << value << "'"
		<< ")";
	return query(p_db, s.str());
}

ats::String CanJ1939DB::Set(const ats::String &p_db , const ats::String& p_app, const ats::String& p_value)
{
	return set_config(p_db, p_app,p_value);
}

ats::String CanJ1939DB::h_get_applist(const ats::String &p_db)
{
	std::stringstream s;

	s << "select v_Date, v_App from t_xml order by v_App";

	const ats::String& err = query( p_db, s.str());

	if(!err.empty())
	{
		return err;
	}

	size_t i;

	for(i = 0; i < m_table.size(); ++i)
	{
		ResultRow& row = m_table[i];
		size_t i;

		for(i = 1; i < row.size(); ++i)
		{
			row[i] = ats::from_hex(row[i]);
		}
	}

	return ats::String();
}

ats::String CanJ1939DB::h_get_config(const ats::String &p_db, const ats::String& p_app)
{
	const ats::String& app = ats::to_hex(p_app);
	std::stringstream s;


	if(p_app == g_no_app_so_select_all)
	{
		s << "select v_Date,v_Value,v_App from t_xml order by v_App";
	}
	else
	{
		s << "select v_Date,v_Value from t_xml where v_App='" << app << "'";
	}

	const ats::String& err = query( p_db, s.str());

	if(!err.empty())
	{
		return err;
	}

	size_t i;

	for(i = 0; i < m_table.size(); ++i)
	{
		ResultRow& row = m_table[i];
		size_t i;

		for(i = 1; i < row.size(); ++i)
		{
			row[i] = ats::from_hex(row[i]);
		}

	}

	return ats::String();
}

ats::String CanJ1939DB::Get(const ats::String &p_db)
{
	return h_get_config(p_db, g_no_app_so_select_all);
}

ats::String CanJ1939DB::Get(const ats::String &p_db, const ats::String& p_app)
{
	return h_get_config(p_db, p_app);
}

ats::String CanJ1939DB::GetApp(const ats::String &p_db)
{
	return h_get_applist(p_db);
}

ats::String CanJ1939DB::GetValue(const ats::String &p_db, const ats::String& p_app)
{

	if(Get(p_db, p_app).empty())
	{
		if(m_table.size() == 1)
		{
			if (m_table[0].size() >= 2)
			{
				return (m_table[0])[1];
			}
		}
	}

	return ats::String();
}

ats::String CanJ1939DB::Update(const ats::String &p_db, const ats::String& p_app, const ats::String& p_value)
{
	const ats::String& value = Get(p_db, p_app);

	if(value != p_value)
	{
		return Set(p_db, p_app, p_value);
	}

	return ats::String();
}

ats::String CanJ1939DB::Clear(const ats::String &p_db)
{
	std::stringstream s;
	s << "delete from t_xml";
	return query(p_db, s.str());
}

ats::String CanJ1939DB::ClearApp(const ats::String &p_db, const ats::String& p_app)
{
	const ats::String& app = ats::to_hex(p_app);

	std::stringstream s;
	s << "delete from t_xml where v_App='" << app << "'";
	return query(p_db, s.str());
}

ats::String CanJ1939DB::Unset(const ats::String &p_db)
{
	return Clear(p_db);
}

ats::String CanJ1939DB::Unset(const ats::String &p_db, const ats::String& p_app)
{
	return ClearApp(p_db, p_app);
}

