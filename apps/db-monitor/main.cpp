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

#include "MyData.h"
#include "DBLock.h"
#include "DBMapping.h"

static int g_dbg = 0;

int g_fd = -1;

static MyData g_md;

class SQLContext
{
public:
	ClientData* m_cd;
	MyData* m_md;

	SQLContext(MyData& p_md, ClientData& p_cd) : m_cd(&p_cd), m_md(&p_md)
	{
	}

	db_monitor::ResultRow m_column;
	db_monitor::ResultTable m_table;
};

static int sql_callback(void* p_context, int p_argc, char** p_argv, char** p_colname)
{
	SQLContext& context = *((SQLContext*)p_context);

	if(context.m_column.empty() && p_colname)
	{

		if(p_argc > 0)
		{
			context.m_column.reserve(p_argc);
		}

		int i;

		for(i = 0; i < p_argc; ++i)
		{
			context.m_column.push_back(p_colname[i] ? p_colname[i] : "");
		}

	}

	context.m_table.push_back(db_monitor::ResultRow());
	db_monitor::ResultRow& row = context.m_table[context.m_table.size() - 1];

	if(p_argc > 0)
	{
		row.reserve(p_argc);
		int i;

		for(i = 0; i < p_argc; i++)
		{
			row.push_back(p_argv ? (p_argv[i] ? p_argv[i] : "") : "");
		}

	}

	return 0;
}

// ************************************************************************
// Description: Command server.
//
// Parameters:  p - pointer to ClientData
// Returns:     NULL pointer
// ************************************************************************
static void* client_command_server(void* p)
{
	ClientData* cd = (ClientData*)p;
	MyData& md = *((MyData*)(cd->m_data->m_hook));

	bool command_too_long = false;
	ats::String cmd;
	ats::String cmdkey;
	ats::String cmdline;

	const size_t max_command_length = 1000 * 1000;

	const int fd = cd->m_sockfd;

	CommandBuffer cb;
	init_CommandBuffer(&cb);
	alloc_dynamic_buffers(&cb, 256, 256 * 1024);

	ClientDataCache cache;
	init_ClientDataCache(&cache);

	for(;;)
	{
		char ebuf[1024];
		const int c = client_getc_cached(cd, ebuf, sizeof(ebuf), &cache);

		if(c < 0)
		{

			if(c != -ENODATA)
			{
				syslog(LOG_ERR, "%s,%d: %s", __FILE__, __LINE__, ebuf);
			}

			break;
		}

		if(c != '\r' && c != '\n')
		{

			if(cmdline.length() >= max_command_length)
			{
				command_too_long = true;
			}
			else
			{
				cmdline += c;
			}

			continue;
		}

		if(command_too_long)
		{
			command_too_long = false;
			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "error: command is too long\n\r");
			cmdline.clear();
			continue;
		}

		{
			const char* err = gen_arg_list(cmdline.c_str(), cmdline.length(), &cb);
			cmdline.clear();

			if(err)
			{
				send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "error: gen_arg_list failed (%s)\n\r", err);
				continue;
			}

		}

		if(cb.m_argc <= 0)
		{
			continue;
		}

		ats::get_command_key(cb.m_argv[0], cmd, cmdkey);

		if("sql" == cmd)
		{
			// XXX: Perform as many "slow" operations as possible outside of the critical section (such as
			//	checking parameters).
			int rc = SQLITE_ERROR;
			ats::StringMap args;
			args.from_args(cb.m_argc - 1, cb.m_argv + 1);
			const ats::String& dbname = args.get("db");
			const ats::String& q = args.get("query");
			const ats::String& query =
				((q.size() >= 1) && ((0 == q.compare(0, 1, "X") || (0 == q.compare(0, 1, "x"))))) ?
				ats::from_hex(q.c_str() + 1) 
				:
				q;

			DBMapping* dbm = md.get_db(dbname);

			if(!dbm)
			{
				const ats::String& key = dbname.empty() ? md.m_default_db_key : dbname;
				send_cmd(fd, MSG_NOSIGNAL, "%s,%s: error %s\n\r", cmd.c_str(), cmdkey.c_str(), ats::to_hex("no database opened (db key=" + key + ")").c_str());
			}
			else
			{
				SQLContext context(md, *cd);
				char* err = 0;

				// XXX: Critical section begins.
				dbm->lock();
				rc = sqlite3_exec(dbm->m_db, query.c_str(), sql_callback, &context, &err);
				dbm->unlock();
				// XXX: Critical section ends.

				if(rc != SQLITE_OK)
				{
					send_cmd(fd, MSG_NOSIGNAL, "%s,%s: error %s\n\r", cmd.c_str(), cmdkey.c_str(), ats::to_hex(err).c_str());
				}
				else
				{

					if(context.m_column.empty())
					{
						send_cmd(fd, MSG_NOSIGNAL, "%s,%s: ok\n\r", cmd.c_str(), cmdkey.c_str());
					}
					else
					{
						const bool single_column_single_row_response =
							(1 == context.m_column.size()) &&
							(1 == context.m_table.size()) &&
							(1 == (context.m_table[0]).size());

						if(single_column_single_row_response)
						{
							send_cmd(fd, MSG_NOSIGNAL, "%s,%s: ok\n'%s'\n'%s'\n\r", cmd.c_str(), cmdkey.c_str(),
								ats::to_hex(context.m_column[0]).c_str(),
								ats::to_hex(context.m_table[0][0]).c_str());
						}
						else
						{
							send_cmd(fd, MSG_NOSIGNAL, "%s,%s: ok\n'%s'%s", cmd.c_str(), cmdkey.c_str(), ats::to_hex(context.m_column[0]).c_str(),
								(context.m_table.empty() && (context.m_column.size() <= 1)) ? "\n\r" : "");

							size_t i;

							for(i = 1; i < context.m_column.size(); ++i)
							{
								const bool last_column = ((i+1) == context.m_column.size());
								send_cmd(fd, MSG_NOSIGNAL, " '%s'%s", ats::to_hex(context.m_column[i]).c_str(), last_column ? (context.m_table.size() ? "" : "\n\r") : "");
							}

							if(context.m_table.size())
							{
								size_t i;

								for(i = 0; i < context.m_table.size(); ++i)
								{
									const db_monitor::ResultRow& row = context.m_table[i];
									size_t j;

									for(j = 0; j < row.size(); ++j)
									{
										const bool first_row_first_column = ((!i) && (!j));
										send_cmd(fd, MSG_NOSIGNAL, "%s%s'%s'", (first_row_first_column ? "\n" : ""), (j ? " " : ""), ats::to_hex(row[j]).c_str());
									}

									const bool last_row = (context.m_table.size() == (i+1));
									send_cmd(fd, MSG_NOSIGNAL, "\n%s", last_row ? "\r" : "");
								}

							}

						}

					}

				}

				md.put_db(dbm);
			}

		}
		else if("open_db" == cmd)
		{

			if(cb.m_argc >= 3)
			{
				ats::StringMap args;
				args.from_args(cb.m_argc - 3, cb.m_argv + 3);

				const ats::String& err = md.open_db(cb.m_argv[1], cb.m_argv[2], args.get("lock"));

				if(!err.empty())
				{
					send_cmd(fd, MSG_NOSIGNAL, "%s,%s: error: %s\n\r", cmd.c_str(), cmdkey.c_str(), err.c_str());
				}
				else
				{
					send_cmd(fd, MSG_NOSIGNAL, "%s,%s: ok\n\r", cmd.c_str(), cmdkey.c_str());
				}

			}
			else
			{
				send_cmd(fd, MSG_NOSIGNAL, "%s,%s: error: usage <dbname> <dbfile>\n\r", cmd.c_str(), cmdkey.c_str());
			}

		}
		else if("close_db" == cmd)
		{

			if(cb.m_argc >= 2)
			{
				const ats::String& err = md.close_db(cb.m_argv[1]);

				if(!err.empty())
				{
					send_cmd(fd, MSG_NOSIGNAL, "%s,%s: error: %s\n\r", cmd.c_str(), cmdkey.c_str(), err.c_str());
				}
				else
				{
					send_cmd(fd, MSG_NOSIGNAL, "%s,%s: ok\n\r", cmd.c_str(), cmdkey.c_str());
				}

			}
			else
			{
				send_cmd(fd, MSG_NOSIGNAL, "%s,%s: error: usage <dbname>\n\r", cmd.c_str(), cmdkey.c_str());
			}

		}
		else if("status" == cmd)
		{

			md.lock_sql();

			DBMap::const_iterator i = md.m_db.begin();

			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "Databases: %zu\n", md.m_db.size());

			while(i != md.m_db.end())
			{
				const ats::String& key = i->first;
				const DBMapping& db = *(i->second);
				++i;

				send_cmd(cd->m_sockfd, MSG_NOSIGNAL,
					"\t%s, \"%s\":\n"
						"\t\tref=%zu\n"
						"\t\tlock=%s\n"
						"\t\tsqlite3=%p\n",
					key.c_str(),
					db.m_file.c_str(),
					db.m_ref_count,
					db.m_lock ? (db.m_lock->m_iter->first).c_str() : "",
					db.m_db);
			}

			md.unlock_sql();

		}
		else if(("debug" == cmd) || ("dbg" == cmd))
		{
			if(cb.m_argc >= 2)
			{
				g_dbg = strtol(cb.m_argv[1], 0, 0);
				send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "debug=%d\n\r", g_dbg);
			}
			else
			{
				send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "debug=%d\n\r", g_dbg);
			}

		}
		else
		{
			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "Invalid command \"%s\"\n\r", cmd.c_str());
		}

	}

	free_dynamic_buffers(&cb);

	return 0;
}

int main(int argc, char* argv[])
{
	MyData& md = g_md;

	md.set(md.m_default_db_key, "/tmp/default.db");
	md.set("user", "applet");
	md.set("app_name", "db-monitor");
	md.set_from_args(argc - 1, argv + 1);

	g_dbg = md.get_int("debug");

	const ats::String& app_name = md.get("app_name");

	openlog(app_name.c_str(), LOG_PID, LOG_USER);

	const ats::String& err = md.open_db(md.m_default_db_key, md.get("default.db").c_str(), md.m_default_db_key);

	if(!err.empty())
	{
		syslog(LOG_ERR, "%s", err.c_str());
		return 1;
	}

	{
		ServerData* sd = md.m_command_server;
		init_ServerData(sd, 64);
		sd->m_port = 41012;
		sd->m_hook = &md;
		sd->m_cs = client_command_server;
		::start_server(sd);
		signal_app_tcp_socket_ready(app_name.c_str(), ats::toStr(sd->m_port).c_str());
	}

	{
		ServerData* sd = md.m_command_server + 1;
		init_ServerData(sd, 64);
		sd->m_hook = &md;
		sd->m_cs = client_command_server;
		const ats::String& user = md.get("user");
		set_unix_domain_socket_user_group(sd, user.c_str(), "db-monitor");
		::start_redstone_ud_server(sd, app_name.c_str(), 1);
		signal_app_unix_socket_ready(app_name.c_str(), app_name.c_str());
	}

	signal_app_ready(app_name.c_str());
	ats::infinite_sleep();
	return 1;
}
