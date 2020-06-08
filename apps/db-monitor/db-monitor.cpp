#include <sstream>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/time.h>

#include "socket_interface.h"
#include "db-monitor.h"

static const ats::String g_config_db_fname(CONFIG_DB_FNAME);
static const ats::String g_config_db_key("config");

using namespace db_monitor;

DBMonitorContext::DBMonitorContext()
{
	init();
}

DBMonitorContext::DBMonitorContext(const ats::String& p_db_key, const ats::String& p_db_fname) : m_db_fname(p_db_fname), m_db_key(p_db_key)
{
	init();
}

DBMonitorContext::~DBMonitorContext()
{
	disconnect();
	delete m_cs;
}

void DBMonitorContext::init()
{
	m_cs = new ClientSocket;
	init_ClientSocket(m_cs);

	wait_for_app_ready("db-monitor");
}

const ats::String& DBMonitorContext::db_fname() const
{
	return (m_db_fname.empty()) ? g_config_db_fname : m_db_fname;
}

const ats::String& DBMonitorContext::db_key() const
{
	return (m_db_key.empty()) ? g_config_db_key : m_db_key;
}

const ats::String& DBMonitorContext::connect(bool p_open_db)
{
	m_error.clear();

	if(is_connected_ClientSocket(m_cs))
	{
		return m_error;
	}

	const int err = connect_redstone_ud_client(m_cs, "db-monitor");

	if(err)
	{
		m_error = "connect to db-monitor failed: (" + (ats::toStr(err) + ") ") + strerror(err);
		return m_error;
	}

	if(p_open_db && (!(m_db_key.empty() || m_db_fname.empty())))
	{
		open_db(m_db_key, m_db_fname);
	}

	return m_error;
}

void DBMonitorContext::disconnect()
{
	close_ClientSocket(m_cs);
}

void DBMonitorContext::reconnect()
{
	disconnect();
	connect();
}

static void purge_remainder_of_command(DBMonitorContext& p_db, int p_fd, ClientDataCache& p_cdc, ats::String& p_error)
{

	for(;;)
	{
		const int c = recv_cached(p_fd, 0, &p_cdc);

		if(c < 0)
		{
			p_db.disconnect();
			p_error += ats::String(": error reading from server: ") + strerror(errno);
			break;
		}

		if('\r' == c)
		{
			break;
		}

	}

}

static void h_parse_first_line(const ats::String& p_result, ats::String& p_error)
{
	size_t i = p_result.find(':');

	if(ats::String::npos == i)
	{
		p_error = "bad response format: (" + p_result + ")";
		return;
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

const ats::String& DBMonitorContext::open_db(const ats::String& p_db_key, const ats::String& p_db_loc)
{

	if(!(connect(false)).empty())
	{
		return m_error;
	}

	const int& fd = m_cs->m_fd;

	{
		const int ret = send_cmd(fd, MSG_NOSIGNAL, "open_db \"%s\" \"%s\"\r", p_db_key.c_str(), p_db_loc.c_str());

		if(ret < 0)
		{
			disconnect();
			return m_error = (ats::String("open_db: send_cmd error: ") + strerror(-ret));
		}

	}

	bool first_line = true;
	const size_t max_query_length = 32 * 1024;
	m_result.clear();
	m_error.clear();

	ClientDataCache cdc;
	init_ClientDataCache(&cdc);

	for(;;)
	{
		const int ret = recv_cached(fd, 0, &cdc);

		if(ret < 0)
		{
			disconnect();
			m_error = (ats::String("error reading from server: ") + strerror(errno));
			break;
		}

		const char c = char(ret);

		if('\n' == c)
		{

			if(first_line)
			{
				first_line = false;
				h_parse_first_line(m_result, m_error);
			}

			if(!m_error.empty())
			{
				purge_remainder_of_command(*this, fd, cdc, m_error);
				break;
			}

			m_result.clear();

		}
		else if(c != '\r')
		{

			if(m_result.length() >= max_query_length)
			{
				m_error = "result is too large";
				purge_remainder_of_command(*this, fd, cdc, m_error);
				break;
			}

			m_result += c;
		}
		else
		{

			if(first_line)
			{
				h_parse_first_line(m_result, m_error);
			}

			break;
		}

	}

	return m_error;
}

const ats::String& DBMonitorContext::close_db(const ats::String& p_db_key)
{

	if(!(connect(false)).empty())
	{
		return m_error;
	}

	const int& fd = m_cs->m_fd;

	{
		const int ret = send_cmd(fd, MSG_NOSIGNAL, "close_db \"%s\"\r", p_db_key.c_str());

		if(ret < 0)
		{
			disconnect();
			return (m_error = (ats::String("close_db: send_cmd error: ") + strerror(-ret)));
		}

	}

	bool first_line = true;
	const size_t max_query_length = 32 * 1024;
	m_result.clear();
	m_error.clear();
	ats::String response_line;

	ClientDataCache cdc;
	init_ClientDataCache(&cdc);

	for(;;)
	{
		const int ret = recv_cached(fd, 0, &cdc);

		if(ret < 0)
		{
			disconnect();
			return (m_error = (ats::String("error reading from server: ") + strerror(errno)));
		}

		const char c = char(ret);

		if('\n' == c)
		{

			if(first_line)
			{
				first_line = false;
				h_parse_first_line(m_result, m_error);
			}

		}
		else if(c != '\r')
		{

			if(m_result.length() >= max_query_length)
			{
				m_error = "result is too large";
				purge_remainder_of_command(*this, fd, cdc, m_error);
				break;
			}

			m_result += c;
		}
		else
		{

			if(first_line)
			{
				h_parse_first_line(m_result, m_error);
			}

			break;
		}

	}

	return m_error;
}

const ats::String& DBMonitorContext::close_db()
{
	return close_db(m_db_key);
}

static void parse_result_line(const ats::String& p_result, ResultRow& p_row)
{
	ats::String* value = 0;
	const char* s = p_result.c_str();
	size_t i;

	for(i = 0; (i+1) < p_result.size();)
	{

		if(' ' == s[i])
		{
			++i;
			value = 0;
			continue;
		}
		else if('\'' == s[i])
		{

			if('\'' == s[i+1])
			{
				i += 3;
				value = 0;
				p_row.push_back(ats::g_empty);
			}
			else
			{
				++i;
			}

			continue;
		}

		if(!value)
		{
			p_row.push_back(ats::g_empty);
			value = &(p_row[p_row.size() - 1]);
		}

		(*value) += char((ats::g_hextable[s[i] & 0x7f] << 4) | ats::g_hextable[s[i + 1] & 0x7f]);
		i += 2;
	}

}

static char* strapp(char* p_des, const char* p_src)
{

	while(*p_src)
	{
		*(p_des++) = *(p_src++);
	}

	*p_des = '\0';
	return p_des;
}

static const char g_query_sql_prefix[] = "sql query=X";
static const char g_query_db_prefix[] = " db=\"";
static const char g_query_db_postfix[] = "\"\r";
static const size_t g_max_fast_query_length = 3584;

const ats::String& DBMonitorContext::query(const ats::String& p_db, const ats::String& p_query, size_t p_max_query_length)
{

	if(!(connect()).empty())
	{
		return m_error;
	}

	const int& fd = m_cs->m_fd;
	const size_t query_offset = sizeof(g_query_sql_prefix) - 1;
	const size_t db_param_length = (sizeof(g_query_db_prefix) - 1) + p_db.size() + (sizeof(g_query_db_postfix) - 1);

	if(((p_query.length() << 1) + query_offset + db_param_length) > g_max_fast_query_length)
	{
		const int ret = send_cmd(fd, MSG_NOSIGNAL, "sql db=\"%s\" query=X%s\r", p_db.c_str(), ats::to_hex(p_query).c_str());

		if(ret < 0)
		{
			disconnect();
			return (m_error = ats::String("query: send_cmd error: ") + strerror(-ret));
		}

	}
	else
	{
		char query[g_max_fast_query_length];
		char* endp = query;
		endp = strapp(endp, g_query_sql_prefix);
		ats::to_hex(query + query_offset, (sizeof(query) - (query_offset + db_param_length)), p_query, &endp);
		endp = strapp(endp, g_query_db_prefix);
		endp = strapp(endp, p_db.c_str());
		endp = strapp(endp, g_query_db_postfix);
		const int ret = send(fd, query, endp - query, MSG_NOSIGNAL);

		if(ret < 0)
		{
			disconnect();
			return (m_error = ats::String("query: send_cmd error: ") + strerror(-ret));
		}

	}

	bool reading_columns = true;
	bool first_line = true;
	m_column.clear();
	m_table.clear();
	m_result.clear();
	m_error.clear();

	ClientDataCache cdc;
	init_ClientDataCache(&cdc);

	for(;;)
	{
		const int c = recv_cached(fd, 0, &cdc);

		if(c < 0)
		{
			disconnect();
			m_error = (ats::String("error reading from server: (") + ats::toStr(c) + ") ") + strerror(-c);
			break;
		}

		if('\n' == c)
		{

			if(first_line)
			{
				first_line = false;
				h_parse_first_line(m_result, m_error);

				if(!m_error.empty())
				{
					purge_remainder_of_command(*this, fd, cdc, m_error);
					break;
				}

			}
			else
			{

				if(reading_columns)
				{
					reading_columns = false;
					parse_result_line(m_result, m_column);
				}
				else
				{
					m_table.push_back(ResultRow());
					ResultRow* row = &(m_table[m_table.size() - 1]);
					row->reserve(m_column.size());
					parse_result_line(m_result, *row);
				}

			}

			m_result.clear();
		}
		else if(c != '\r')
		{

			if(m_result.length() >= p_max_query_length)
			{
				m_error = "result is too large";
				purge_remainder_of_command(*this, fd, cdc, m_error);
				break;
			}

			m_result += char(c);
		}
		else
		{

			if(first_line)
			{
				h_parse_first_line(m_result, m_error);
			}

			break;
		}

	}

	return m_error;
}

const ats::String& DBMonitorContext::query(const ats::String& p_query, size_t p_max_query_length)
{
	return query(m_db_key, p_query, p_max_query_length);
}
