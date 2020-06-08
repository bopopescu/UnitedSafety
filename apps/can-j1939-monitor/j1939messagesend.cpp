#include <iostream>
#include <map>
#include <list>

#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "ats-common.h"
#include "ats-string.h"
#include "atslogger.h"
#include "socket_interface.h"
#include "command_line_parser.h"
#include "state-machine-data.h"
#include "redstone-socketcan-j1939.h"
#include "ConfigDB.h"
#include "AFS_Timer.h"
#include "expat.h"

/* message xml format:
<data>
    <pgn>0x0FEE5</pgn>
    <value>
	    <destaddress>0xFF</destaddress>
	    <cycletime>2</cycletime>
	    <rawdata>00 00 00 00 00 00 00 00</rawdata>
    </value>
    <pgn>0x0FECA</pgn>
    <value>
	    <destaddress>0xFF</destaddress>
	    <cycletime>2</cycletime>
	    <rawdata>11 22 33 44 55 66 77 88 99 aa bb cc dd ee ff</rawdata>
    </value>
</data>
*/

static void XMLCALL xmlend(void *p, const char *name);
static void handle_data(void *p, const char *content, int length);
static int g_dbg = 0;
static ATSLogger g_log;
static ats::String lastcontent;
static const ats::String g_app_name("j1939messagesend");

struct msgcontent
{
	int pgn;
	int destaddress;
	int cycletime;
	ats::String rawdata;
public:
	msgcontent():pgn(0), destaddress(0xFF), cycletime(0), rawdata(ats::String()){}
};

class MessageHandler;
class MyData;

class AdminCommandContext
{
public:
	AdminCommandContext(MyData& p_data, ClientData& p_cd)
	{
		m_data = &p_data;
		m_cd = &p_cd;
	}

	int get_sockfd() const
	{
		return (m_cd ? m_cd->m_sockfd : 0);
	}

	MyData& my_data() const
	{
		return *m_data;
	}

	MyData* m_data;

private:
	ClientData* m_cd;
};

class AdminCommand;
typedef int (*AdminCommandFn)(AdminCommandContext&, const AdminCommand&, int p_argc, char* p_argv[]);

class AdminCommand
{
public:
	AdminCommand(AdminCommandFn p_fn)
	{
		m_fn = p_fn;
	}

	AdminCommandFn m_fn;
};

class MessageHandler
{
public:
	AFS_Timer m_timer;

	MessageHandler(MyData& p_md, const msgcontent& msg)
	{
		m_md = &p_md;
		m_msg = msg;

		m_mutex = new pthread_mutex_t;
		pthread_mutex_init(m_mutex, 0);

		m_available = true;
		m_timer.SetTime();

		ats::StringList a;
		ats::split(a, m_msg.rawdata, " ");

		ats::StringList::const_iterator i = a.begin();
		while( i != a.end())
		{
			m_data.push_back(strtol((*i).c_str() ,0 ,0));
			++i;
		}
	}

	~MessageHandler()
	{
		delete m_mutex;
	}

	void send(CANSocket* can)
	{
		if(can == 0)
			return;

		//j1939 has a feature for PGNs up to 1785 bytes (7 * 255 = 1785)
		uint8_t data[1785];
		int size = (m_data.size() > 1785) ? 1785 : m_data.size();

		for( int i = 0; i < size; ++i)
		{
			data[i] = m_data[i];
		}

		CAN_write(can, m_msg.pgn & 0x3FFFF, data, size, m_msg.destaddress);
		//ats_logf(ATSLOG(5), "send message pgn:0x%x, data:0x%x ", m_msg.pgn, *((int*)data));
	}

	int touch(CANSocket* can)
	{
		int err = 0;
		lock();

		if(m_msg.cycletime == 0)
		{
			if(m_available == true)
			{
				send(can);
				m_available = false;
				err = -1;
			}
		}
		else if(m_timer.DiffTime() > m_msg.cycletime)
		{
			send(can);
			m_timer.SetTime();
		}

		unlock();
		return err;
	}


	void lock() const
	{
		pthread_mutex_lock(m_mutex);
	}

	void unlock() const
	{
		pthread_mutex_unlock(m_mutex);
	}

private:

	MyData* m_md;
	msgcontent m_msg;
	pthread_mutex_t* m_mutex;
	bool m_available;
	std::vector <uint8_t> m_data;
};


typedef std::map <const ats::String, AdminCommand> AdminCommandMap;
typedef std::pair <const ats::String, AdminCommand> AdminCommandPair;
typedef std::map <const int, msgcontent> messageMap;
typedef std::map <const int, MessageHandler*> messageHandlerMap;
typedef std::pair <const int, MessageHandler*> messageHandlerPair;

class MyData : public StateMachineData
{
public:
	ats::StringMap m_config;
	AdminCommandMap m_command;
	messageMap m_msgMap;
	messageHandlerMap m_msgHandlerMap;

	MyData()
	{
		m_mutex = new pthread_mutex_t;
		pthread_mutex_init(m_mutex, 0);

		m_exit_sem = new sem_t;
		sem_init(m_exit_sem, 0, 0);
	}

	void lock() const
	{
		pthread_mutex_lock(m_mutex);
	}

	void unlock() const
	{
		pthread_mutex_unlock(m_mutex);
	}

	void post_exit_sem(){ sem_post(m_exit_sem);}
	void wait_exit_sem(){ sem_wait(m_exit_sem);}

	MessageHandler* add_messageHandler(const msgcontent& msg)
	{
		MessageHandler* b = new MessageHandler(*this, msg);

		std::pair <messageHandlerMap::iterator, bool> r = m_msgHandlerMap.insert(messageHandlerPair(msg.pgn, b));
		if(!r.second)
		{
			delete b;
			b = (r.first)->second;
		}

		return b;
	}

private:
	pthread_mutex_t* m_mutex;
	sem_t* m_exit_sem;
};

static void h_xml_parse(void *p, const ats::String& buf)
{
	MyData &md = *((MyData *)p);
	XML_Parser parser = XML_ParserCreate(NULL);
	if (parser == NULL)
	{
		ats_logf(ATSLOG(0), "%s,%d: Xml parser not created", __FILE__, __LINE__);
		return ;
	}

	XML_SetUserData(parser, &md);

	XML_SetElementHandler(parser, NULL, xmlend);

	XML_SetCharacterDataHandler(parser, handle_data);

	if ( XML_STATUS_ERROR == XML_Parse( parser, buf.c_str(), buf.size(), 0 ) )
	{
		ats_logf(ATSLOG(0), "failed to parser: %s( line:%lu, column:%lu )", XML_ErrorString( XML_GetErrorCode( parser ) ),
				XML_GetCurrentLineNumber( parser ), XML_GetCurrentColumnNumber( parser ));
	}

	XML_ParserFree(parser);
}

static void print_usage(const char* p_prog_name)
{
	fprintf(stderr,
		"Usage: %s ,config=<path to xml file>\n"
		"\n"
		,p_prog_name);
}

static int read_from_file(FILE* p_f, ats::String& p_des)
{
	p_des.clear();
	char buf[256];

	for(;;)
	{
		const size_t nread = fread(buf, 1, 256, p_f);

		if(!nread)
		{

			if(!feof(p_f))
			{
				return -EIO;
			}

			return 0;
		}

		p_des.append(buf, nread);
	}
}

static int currentpgn = 0;
static void XMLCALL xmlend(void *p, const char *name)
{
	MyData &md = *((MyData *)p);

	if(strcmp(name, "pgn") == 0)
	{
		currentpgn = (int)(strtol(lastcontent.c_str(), 0, 0));
		msgcontent msg;
		msg.pgn = currentpgn;
		md.m_msgMap[currentpgn] = msg;
	}
	else if (strcmp(name, "destaddress") == 0)
	{
		msgcontent& msg = md.m_msgMap[currentpgn];
		msg.destaddress = (int)(strtol(lastcontent.c_str(), 0, 0));
	}
	else if (strcmp(name, "cycletime") == 0)
	{
		msgcontent& msg = md.m_msgMap[currentpgn];
		msg.cycletime = (int)(strtol(lastcontent.c_str(), 0, 0));
	}
	else if (strcmp(name, "rawdata") == 0)
	{
		msgcontent& msg = md.m_msgMap[currentpgn];
		msg.rawdata = lastcontent.c_str();
	}

	return ;
}

static void handle_data(void *p, const char *content, int length)
{
	lastcontent = ats::String(content).substr(0,length);
}

void *h_transmit_thread(void *p)
{
	MyData &md = *((MyData *)p);

	messageMap::iterator it = md.m_msgMap.begin();
	while( it != md.m_msgMap.end())
	{
		msgcontent& msg = (*it).second;

		md.add_messageHandler(msg);

		ats_logf(ATSLOG(0), "insert %d %d %s", msg.pgn, msg.destaddress, msg.rawdata.c_str());
		++it;
	}

	db_monitor::ConfigDB db;
	int sourceaddress = strtol((db.GetValue("CanJ1939Monitor", "sourceaddress")).c_str(), 0, 0);
	if(!sourceaddress)sourceaddress=0xf9;

	CANSocket *can;
	can = create_new_CANSocket();
	CAN_connect(can, "can0", sourceaddress);

	while(md.m_msgHandlerMap.size() > 0)
	{
		messageHandlerMap::iterator it = md.m_msgHandlerMap.begin();

		while(it != md.m_msgHandlerMap.end())
		{
			if((*it).second->touch(can) !=0 )
			{
				delete (*it).second;
				md.m_msgHandlerMap.erase(it++);
				continue;
			}

			usleep(100000);
			++it;
		}
	}

	destroy_CANSocket(can);

	md.post_exit_sem();

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
	MyData &md = *((MyData *)(cd->m_data->m_hook));

	bool command_too_long = false;
	ats::String cmd;

	const size_t max_command_length = 1024;

	ClientDataCache cdc;
	init_ClientDataCache(&cdc);

	AdminCommandContext acc(md, *cd);

	for(;;)
	{
		char ebuf[1024];
		const int c = client_getc_cached(cd, ebuf, sizeof(ebuf), &cdc);

		if(c < 0)
		{
			if(c != -ENODATA) ats_logf(ATSLOG(3), "%s,%d: %s", __FILE__, __LINE__, ebuf);
			break;
		}

		if(c != '\r' && c != '\n')
		{
			if(cmd.length() >= max_command_length) command_too_long = true;
			else cmd += c;

			continue;
		}

		if(command_too_long)
		{
			ats_logf(ATSLOG(0), "%s,%d: command is too long", __FILE__, __LINE__);
			cmd.clear();
			command_too_long = false;
			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "xml <devResp error=\"command is too long\"></devResp>\r");
			continue;
		}

		CommandBuffer cb;
		init_CommandBuffer(&cb);
		const char *err;

		if((err = gen_arg_list(cmd.c_str(), cmd.length(), &cb)))
		{
			ats_logf(ATSLOG(0), "%s,%d: gen_arg_list failed (%s)", __FILE__, __LINE__, err);
			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "xml <devResp error=\"gen_arg_list failed\">%s</devResp>\r", err);
			cmd.clear();
			continue;
		}
		cmd.clear();

		if(cb.m_argc <= 0)
		{
			continue;
		}

		const ats::String cmd(cb.m_argv[0]);
		ats::StringMap args;
		args.from_args(cb.m_argc - 1, cb.m_argv + 1);

		if(("debug" == cmd) || ("dbg" == cmd))
		{

			if(cb.m_argc >= 2)
			{
				g_dbg = strtol(cb.m_argv[1], 0, 0);
				send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "%s: debug=%d\n\r", cmd.c_str(), g_dbg);
			}
			else
			{
				send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "%s: debug=%d\n\r", cmd.c_str(), g_dbg);
			}

		}
		else if("start_logging" == cmd)
		{
			g_log.open_testdata(g_app_name);
		}
		else
		{
			AdminCommandMap::const_iterator i = md.m_command.find( cmd);

			if(i != md.m_command.end())
			{
				(i->second).m_fn(acc, i->second, cb.m_argc, cb.m_argv);
			}
			else
			{
				send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "Invalid command \"%s\"\n", cmd.c_str());
			}

		}

	}
	return 0;
}

int main(int argc, char* argv[])
{
	static MyData md;

	ats::StringMap &config = md.m_config;
	config.from_args(argc - 1, argv + 1);

	g_dbg = config.get_int("debug");
	g_log.set_global_logger(&g_log);
	g_log.open_testdata(g_app_name);
	g_log.set_level(g_dbg);

	ats_logf(ATSLOG(0), "J1939 Message Transmit started");

	const ats::String& fname = config.get("config");
	if(fname.empty())
	{
		print_usage(argv[0]);
		return 1;
	}

	FILE* f = fopen(fname.c_str(), "r");
	ats::String value;

	if(f)
	{
		const int err = read_from_file(f, value);
		fclose(f);

		if(err)
		{
			ats_logf(ATSLOG(0), "ERROR(%d,%s): An error occurred while reading from \"%s\"\n", err, strerror(err), fname.c_str());
			return 1;
		}

	}
	else
	{
		ats_logf(ATSLOG(0), "ERROR: Could not open \"%s\" for reading\n", fname.c_str());
		return 1;
	}

	h_xml_parse(&md, value);

	{
		pthread_t m_transmit_thread;
		const int retval = pthread_create(
				&(m_transmit_thread),
				(pthread_attr_t *)0,
				h_transmit_thread,
				&md);
		if( retval )
		{
			ats_logf(ATSLOG(0), "%s,%d: Failed to create reader. (%d) %s", __FILE__, __LINE__, errno, strerror(errno));
			return 1;
		}
	}

	ServerData sd;
	init_ServerData(&sd, 64);
	sd.m_hook = &md;
	sd.m_cs = client_command_server;
	::start_redstone_ud_server(&sd, "j1939messagesend", 1);

	md.wait_exit_sem();
	return 0;
}
