#include <iostream>
#include <list>
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
#include <unistd.h>
#include "linux/can.h"
#include <boost/algorithm/string.hpp>

#include "ats-common.h"
#include "ats-string.h"
#include "atslogger.h"
#include "socket_interface.h"
#include "command_line_parser.h"
#include "expat.h"
#include "timer-event.h"

#include "can-j1939-monitor-state-machine.h"
#include "can-j1939-monitor.h"
#include "feature.h"

static ats::String g_type_name = ats::String();

static int g_current_pgn = 0;
static int g_current_spn = 0;
static bool	g_begin_definition = false;
static bool	g_begin_exceed = false;
static bool	g_pgn_request = false;
static int	g_normaldelay = 0;
static ats::String g_last_content = ats::String();
static SignalData g_signalData;
static bool identificationSent = false;
int g_exceedStatTime;
int g_faultStatTime;
AFS_Timer g_faultStatTimer;
ATSLogger g_log;

int g_dbg = 0;

static const ats::String g_app_name("can-j1939-monitor");
bool g_has_message_assembler = false;
const ats::String Checksum(const ats::String& str)
{
	ats::String buf;
	char checksum = 0;

	if(str.size() < 2)
		return ats::String();

	ats::String s;
	if(str[0] == '$')
	{
		s = str.substr(1);
	}
	else
	{
		s = str;
	}

	for(size_t i = 0; i < s.size(); ++i)
	{
		checksum ^= s[i];
	}

	ats_sprintf(&buf, "*%.2X", checksum);

	return buf;
}

int ac_debug(AdminCommandContext &p_acc, const AdminCommand&, int p_argc, char *p_argv[])
{
	if(p_argc <= 1)
	{
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "debug=%d\n", g_dbg);
	}
	else
	{
		g_dbg = strtol(p_argv[1], 0, 0);
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "debug=%d\n", g_dbg);
	}
	return 0;
}

int ac_help(AdminCommandContext &p_acc, const AdminCommand&, int p_argc, char *p_argv[])
{
	send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "This is the help system\n\r");

	return 0;
}

int ac_ignition(AdminCommandContext& p_acc, const AdminCommand&, int p_argc, char* p_argv[])
{
	if(p_argc < 2)
	{
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s:error: usage: %s <on/off> \n\r", p_argv[0], p_argv[0]);
		return 0;
	}

	p_acc.m_data->set("ignition", p_argv[1]);
	ats::StringMap* s = new ats::StringMap();
	s->set("ignition", p_argv[1]);

	p_acc.m_data->post_event("IgnitionEvent", s);

	send_cmd(p_acc.get_sockfd(),  MSG_NOSIGNAL, "%s:ok:\n\r", p_argv[0]);

	return 0;
}

COMMON_EVENT_DEFINITION(,IgnitionEvent, AppEvent)

void XMLCALL xmlstart(void *p, const char* element, const char** attribute)
{
	if ( strcmp("data", element) == 0 )
	{
		for (size_t i = 0; attribute[i]; i += 2 )
		{
			if ( strcmp("name", attribute[i]) == 0 )
			{
				ats_logf(ATSLOG(5), "template name %s", attribute[i + 1]);
			}
			else if ( strcmp("type", attribute[i]) == 0)
			{
				ats_logf(ATSLOG(0), "type %s", attribute[i + 1]);
				g_type_name = attribute[i+1];
			}
		}
	}
	else if ( strcmp("pgn", element) == 0 )
	{
		for (size_t i = 0; attribute[i]; i += 2 )
		{
			if ( strcmp("name", attribute[i]) == 0 )
			{
				g_current_pgn = (int)(strtol( attribute[i+1], 0, 0));
				g_signalData.pgnnum = g_current_pgn;
			}
		}
	}
	else if ( strcmp("spn", element) == 0 )
	{
		for (size_t i = 0; attribute[i]; i += 2 )
		{
			if ( strcmp("name", attribute[i]) == 0 )
			{
				g_current_spn = (int)(strtol( attribute[i+1], 0, 0));
				g_signalData.clean();
				g_signalData.spnnum = g_current_spn;
				g_signalData.pgnnum = g_current_pgn;
			}
		}
	}
	else if ( strcmp("definition", element) == 0 )
	{
		g_begin_definition = true;
	}
	else if ( strcmp("exceed", element) == 0 )
	{
		g_begin_exceed= true;
	}
}

void XMLCALL xmlend(void *p, const char *element)
{
	MyData &md = *((MyData *)p);

	if(strcmp(element, "data") == 0)
	{
	}
	else if(strcmp(element, "pgn") == 0)
	{
		g_current_pgn = 0;
		g_pgn_request = false;
	}
	else if( strcmp(element, "normaldelay") == 0)
	{
		g_normaldelay = (unsigned int)(strtol(g_last_content.c_str(), 0, 0));
	}
	else if( strcmp(element, "request") == 0)
	{
		g_pgn_request = (bool)(strtol(g_last_content.c_str(), 0, 0));
		if(g_pgn_request == true)
		{
			ats_logf(ATSLOG(5), "%s,%d: Inset pgn %d into request list", __FILE__, __LINE__, g_current_pgn);
			md.m_requestSet.insert(g_current_pgn);
		}
	}
	else if(strcmp(element, "spn") == 0)
	{
		md.m_spnsignalMap.insert(std::make_pair(g_current_spn, new SignalMonitor(md, g_signalData)));
		g_current_spn = 0;
	}
	else if(strcmp(element, "exceed") == 0)
	{
		g_begin_exceed = false;
	}
	else if(strcmp(element, "definition") == 0)
	{
		g_begin_definition = false;
	}
	else if(g_begin_definition)
	{
		if ( g_current_spn && g_current_pgn )
		{
			if (strcmp(element, "desc") == 0)
			{
				g_signalData.desc = g_last_content;
			}
			else if (strcmp(element, "periodic") == 0)
			{
				g_signalData.periodic = (bool)(strtol(g_last_content.c_str(), 0, 0));
			}
			else if (strcmp(element, "size") == 0)
			{
				g_signalData.span = (int)(strtol(g_last_content.c_str(), 0, 0));
			}
			else if (strcmp(element, "bitoffset") == 0)
			{
				g_signalData.startposition = (int)(strtol(g_last_content.c_str(), 0, 0));
			}
			else if (strcmp(element, "scale") == 0)
			{
				g_signalData.scale = (float)(strtof(g_last_content.c_str(), NULL));
			}
			else if (strcmp(element, "offset") == 0)
			{
				g_signalData.offset = (float)(strtof(g_last_content.c_str(), NULL));
			}
			else if (strcmp(element, "units") == 0)
			{
				g_signalData.unit = g_last_content;
			}
			else if (strcmp(element, "lrange") == 0)
			{
				g_signalData.lrange = (float)(strtof(g_last_content.c_str(), NULL));
			}
			else if (strcmp(element, "hrange") == 0)
			{
				g_signalData.hrange = (float)(strtof(g_last_content.c_str(), NULL));
			}
		}
	}
	else if(g_begin_exceed)
	{
		if ( g_current_spn && g_current_pgn )
		{
			if (strcmp(element, "condition") == 0)
			{
				if( g_last_content  == "startup" || g_last_content == "STARTUP")
					g_signalData.condition = exceedcondition_startup;
				if( g_last_content == "operation" || g_last_content == "OPERATION")
					g_signalData.condition = exceedcondition_operation;
			}
			else if (strcmp(element, "operation") == 0)
			{
				g_signalData.operation = (bool)(strtol(g_last_content.c_str(), 0, 0));
			}
			else if (strcmp(element, "hlimit") == 0)
			{
				g_signalData.hlimit= (int)(strtol(g_last_content.c_str(), 0, 0));
			}
			else if (strcmp(element, "llimit") == 0)
			{
				g_signalData.LowExLimit= (int)(strtol(g_last_content.c_str(), 0, 0));
			}
		}
	}

	return ;
}

void handle_data(void *p, const char *content, int length)
{
	g_last_content = ats::String(content).substr(0,length);
}

bool h_qualify_signal_task(MyData& md, std::vector<SignalMonitor*> p_pList)
{
	spnsignalMap::iterator i = md.m_spnsignalMap.begin();
	for(; i != md.m_spnsignalMap.end(); ++i)
	{
		SignalMonitor &sm = *((*i).second);
		SignalData& set = sm.getset();
		std::vector<SignalMonitor*>::iterator it =  p_pList.begin();
		while( it != p_pList.end() )
		{
			if( set.spnnum == (*it)->getset().spnnum )
			{
				if(!(*it)->IsInExceedence())
				{
					ats_logf(ATSLOG(5), "%s,%d:[SIGNAL TASK]PGN:%d SPN:%d failed the qualification process",
							__FILE__, __LINE__, set.pgnnum, set.spnnum);
					return false;
				}
			}

			++it;
		}
	}

	ats_logf(ATSLOG(5), "%s,%d: Qualification process passed.", __FILE__, __LINE__);
	return true;
}

void *h_signalmonitor_thread(void *p)
{
	MyData &md = *((MyData *)p);
	REDSTONE_IPC redstone_ipc_data;

	bool isQualified = false;
	bool tempQualified = false;
	AFS_Timer qual_timer;

	std::vector<SignalMonitor*> operationList;
	std::vector<SignalData*> condition_startupList;

	md.wait_scan_sem();
	spnsignalMap::const_iterator it = md.m_spnsignalMap.begin();
	for( ;it != md.m_spnsignalMap.end(); ++it)
	{
		SignalData& set = ((*it).second)->getset();
		if( set.operation )
		{
			operationList.push_back(((*it).second));
		}
		if( set.condition == exceedcondition_startup )
			condition_startupList.push_back(&set);
	}

	for(;;)
	{
		// wait sema
		md.wait_scan_sem();
		if(!redstone_ipc_data.IgnitionOn())
		{
			ats_logf(ATSLOG(0), "%s,%d:Ignition Off Condition detected. Resetting qualification process for defined signals.",
					__FILE__, __LINE__);
			isQualified = false;
			sleep(5);
		}

		if(isQualified)
		{
			spnsignalMap::iterator i = md.m_spnsignalMap.begin();
			for(; i != md.m_spnsignalMap.end(); ++i)
			{
				SignalMonitor &sm = *((*i).second);
				sm.scan();
			}
		}
		else
		{
			spnsignalMap::iterator i = md.m_spnsignalMap.begin();
			for(; i != md.m_spnsignalMap.end(); ++i)
			{
				SignalMonitor &sm = *((*i).second);
				SignalData& set = sm.getset();
				std::vector<SignalData*>::iterator it = condition_startupList.begin();
				while( it != condition_startupList.end())
				{
					if( (*it)->spnnum == set.spnnum )
						sm.scan();
					++it;
				}
			}

			if(h_qualify_signal_task(md, operationList))
			{
				if(!tempQualified)
				{
					qual_timer.SetTime();
					tempQualified = true;
				}

				if(qual_timer.DiffTime() > g_normaldelay )
				{
					ats_logf(ATSLOG(0), "%s,%d:Switching to Normal mode. Qualification of defined signals passed.",
							__FILE__, __LINE__);
					isQualified = true;
				}
			}
			else
			{
				tempQualified = false;
			}
		}
	}
	return 0;
}

void h_xml_parse(void *p, const ats::String& buf)
{
	MyData &md = *((MyData *)p);
	XML_Parser parser = XML_ParserCreate(NULL);
	if (parser == NULL)
	{
		ats_logf(ATSLOG(0), "%s,%d: Xml parser not created", __FILE__, __LINE__);
		return ;
	}

	XML_SetUserData(parser, &md);

	XML_SetElementHandler(parser, xmlstart, xmlend);

	XML_SetCharacterDataHandler(parser, handle_data);

	if ( XML_STATUS_ERROR == XML_Parse( parser, buf.c_str(), buf.size(), 0 ) )
	{
		ats_logf(ATSLOG(0), "failed to parser: %s( line:%lu, column:%lu )", XML_ErrorString( XML_GetErrorCode( parser ) ),
				XML_GetCurrentLineNumber( parser ), XML_GetCurrentColumnNumber( parser ));
	}

	XML_ParserFree(parser);
}

bool xml_process(void *p)
{
	MyData &md = *((MyData *)p);
	db_monitor::ConfigDB db;
	const ats::String& info = db.GetValue( "j1939-db", "template");

	if(info.empty()) return false;

	const ats::String& content = db.GetValue( "j1939-db", "template_" + info);
	if(content.empty()) return false;

	h_xml_parse( &md, content );
	return true;
}

ull getbits(const ull& t, int at, int numbits)
{
	ull mask = ((~0llu)>>(sizeof(ull)*8 - numbits)) << at;
	return (t&mask) >> at;
}

void indicateUpdate(MyData& md)
{
	struct timespec updatetime;
	int res = clock_gettime(CLOCK_MONOTONIC, &updatetime);
	if(res == 0)
	{
		md.m_RedStoneData.ObdLastUpdated(updatetime.tv_sec);
	}
}

void process_Identification( MyData& md, const uint8_t* p, int bufsize)
{
	char pp[bufsize+1];
	memset(pp,'\0',bufsize+1);
	strncpy(pp, (char*)p, bufsize);
	ats::String s;
	ats_sprintf(&s, "%s", pp);
	ats::StringList sp;
	ats::split(sp, s, "*");
	if( sp.size() < 4 ) return;
	ats_logf(ATSLOG(0),"Identification Make: %s, Model: %s, Serial Number: %s, Unit Number: %s",
			sp[0].c_str(), sp[1].c_str(), sp[2].c_str(), sp[3].c_str());

	ats::String buf = "$PATSPM,0,,4,5";
	ats::String s_buf;
	ats_sprintf(&s_buf, ",65259,586,,,%s", sp[0].c_str());//make
	buf.append(s_buf);
	ats_sprintf(&s_buf, ",65259,587,,,%s", sp[1].c_str());//make
	buf.append(s_buf);
	ats_sprintf(&s_buf, ",65259,588,,,%s", sp[2].c_str());//make
	buf.append(s_buf);
	ats_sprintf(&s_buf, ",65259,233,,,%s", sp[3].c_str());//make
	buf.append(s_buf);
	const ats::String& sbuf = buf + Checksum(buf);
	ats_logf(ATSLOG(0), "%s,%d: %s ", __FILE__, __LINE__, sbuf.c_str());
	const int pri = md.get_int("mb_overiridium_priority_periodic");
	IF_PRESENT(message_assembler, send_app_msg(g_app_name.c_str(), "message-assembler", 0,
				"msg calamp_user_msg msg_priority=%d usr_msg_data=\"%s\"\r", pri, ats::to_hex(sbuf).c_str()))

	identificationSent = true;
	std::set<int>::iterator it = md.m_requestSet.find(J1939_CAN_IDENTIFICATION_PGN);
	if( it != md.m_requestSet.end())
		md.m_requestSet.erase(it);
}

void process_dm1( MyData& md, const uint8_t* buf, int bufsize )
{
	uint8_t *p = (uint8_t *)buf;
	if( bufsize < 8 ) return; //at least one DTC.

	if((p[0] | 0x03) == 0x03)
	{
		md.m_reported_dm1_map.clear();
		return;
	}

	for(int i = 0; i < bufsize/4; ++i)
	{
		ull pp = (*((ull*)(buf + i*4)) >> 16);
		//spn is 19bits
		int spnnum = (int)getbits((ull)pp, 0, 16) | (int)((getbits((ull)pp, 21,3 )) << 16);
		//spn is 19 bit number and has a range from 0 to 0x7FFFF
		if( spnnum <= 0 || spnnum >= 0x7FFF ) continue;
		uint8_t fmi = (uint8_t)getbits((ull)pp, 16, 5);
		uint8_t oc = (uint8_t)getbits((ull)pp, 24, 7);

		bool needSendMessage = true;
		std::map<int, int>::iterator it = md.m_reported_dm1_map.begin();
		while( it != md.m_reported_dm1_map.end())
		{
			if( spnnum == (*it).first )
			{
				if( (*it).second >= oc || g_faultStatTimer.DiffTime() < g_faultStatTime)
				{
					needSendMessage = false;
					break;
				}
			}
			++it;
		}

		if( needSendMessage )
		{
			ats::String s_buf;
			ats_sprintf(&s_buf, "$PATSFT,0,%d,%d,%d", spnnum, fmi, oc);
			const int pri = md.get_int("mb_overiridium_priority_fault");
			const ats::String& sbuf = s_buf + Checksum(s_buf);
			IF_PRESENT(message_assembler, send_app_msg(g_app_name.c_str(), "message-assembler", 0,
						"msg calamp_user_msg msg_priority=%d usr_msg_data=\"%s\"\r", pri, ats::to_hex(sbuf).c_str()))

			md.m_reported_dm1_map[spnnum] = oc;
			g_faultStatTimer.SetTime();

			ats_logf(ATSLOG(0),"%s,%d: MIL lamp %s, Red Stop lamp %s, Amber warning lamp %s, SPN: 0x%x, FMI: 0x%x, OC: 0x%x", __FILE__, __LINE__,
					(p[0] & 0xC0)?"On":"Off", (p[0] & 0x30)?"On":"Off", (p[0] & 0x0C)?"On":"Off",spnnum, fmi, oc);

		}
	}
}

void * h_request_thread(void *p)
{
	MyData &md = *((MyData *)p);
	ats_logf(ATSLOG(5),"%s,%d: enter request thread", __FILE__, __LINE__);

	for(;;)
	{
		std::set<int>::const_iterator it = md.m_requestSet.begin();
		while( it != md.m_requestSet.end())
		{
			md.lock();
			int pgn = (*it);
			CAN_write(md.m_s, J1939_CAN_REQ_PGN, &pgn, 3, 0x00);
			md.unlock();
			sleep(1);
			++it;
		}
	}

	return 0;
}

void * h_j1939reader_thread(void *p)
{
	MyData &md = *((MyData *)p);

	md.m_requestSet.clear();
	md.m_spnsignalMap.clear();
	xml_process(&md);

	spnsignalMap::const_iterator it = md.m_spnsignalMap.begin();
	for( ;it != md.m_spnsignalMap.end(); ++it)
	{
		SignalData& set = ((*it).second)->getset();
		ats_logf(ATSLOG(0), "Signaldataaset:pgn %d,spn %d,span %d,startposition %d,\
offset %.1f,scale %.1f,desc: %s,lowlimit %.1f,toplimit %.1f,lrange %.1f,hrange %.1f,condition %d,operation %d,periodic %d",
				set.pgnnum, set.spnnum, set.span, set.startposition, set.offset,
				set.scale, set.desc.c_str(), set.LowExLimit, set.hlimit, set.lrange, set.hrange, set.condition, set.operation, set.periodic);
	}

	//Diagnostic dynamic management section
	md.m_diagnosticMap[J1939_CAN_DM1_PGN] = process_dm1;
	md.m_diagnosticSet.insert(J1939_CAN_DM1_PGN);
	md.m_reported_dm1_map.clear();
	int src = md.get_int("sourceaddress");

	//j1939 has a feature for PGNs up to 1785 bytes (7 * 255 = 1785)
	unsigned char __attribute__((aligned(8))) buf[1785 + 4];
	for(;;)
	{
		md.lock();
		int bytes_read = CAN_read(md.m_s, buf, sizeof(buf));
		md.unlock();

#if 0
		{
			for(int i = 0; i < bytes_read; ++i)
				fprintf(stderr, "0x%.2x ", buf[i]);
			fprintf(stderr, "\n");
		}
#endif

		const unsigned char saddr = buf[3];
		if( saddr != 0 && saddr != src) continue;
		if(bytes_read > 0 && bytes_read <= 1785 + 4) // maximum 1785 bytes + 4 bytes pgn
		{
			int pgn = 0x3FFFF & ((buf[0]) | (buf[1] << 8) | (buf[2] << 16 ));

#if 1
			//Diagnostic section
			{
				std::set<int>::iterator i = std::find(md.m_diagnosticSet.begin(), md.m_diagnosticSet.end(), pgn);
				if( i != md.m_diagnosticSet.end())
				{
					diagnosticMap::const_iterator it = md.m_diagnosticMap.find(pgn);
					if( it != md.m_diagnosticMap.end())
					{
						uint8_t * p = buf + 4;
						(*it).second(md, p, bytes_read - 4);
					}
				}
			}
#endif
			if( pgn == J1939_CAN_IDENTIFICATION_PGN && identificationSent == false )
			{
				uint8_t * p = buf + 4;
				process_Identification( md, p, bytes_read - 4);
			}

			std::vector<SignalMonitor*> vec;
			md.getSignalMonitorList(pgn, vec);

			std::vector<SignalMonitor*>::const_iterator it = vec.begin();
			while( it != vec.end() )
			{
				SignalMonitor* m = (*it);
				SignalData& set = m->getset();
				ull data = getbits(*(ull *)(buf + 4), set.startposition, set.span);
				long long currentvalue = (1.0*(data * set.scale)) + set.offset;
				ats_logf(ATSLOG(5), "pgn:%d, spn:%d, desc:%s,start:%d, span: %d, data: 0x%llx, value: %lld",
						set.pgnnum, set.spnnum, set.desc.c_str(), set.startposition, set.span, data, currentvalue);
				if( currentvalue < set.lrange) data = (1.0 * (set.lrange - set.offset)/set.scale);
				if( currentvalue > set.hrange) data = (1.0 * (set.hrange - set.offset)/set.scale);
				m->set((unsigned int)data);

				if( pgn == J1939_CAN_SPEED_PGN	&& set.spnnum == J1939_CAN_WHEEL_BASED_SPEEP_SPN )
				{
					md.m_RedStoneData.Speed(currentvalue);
					indicateUpdate(md);
				}
				else if ( pgn == J1939_CAN_RPM_PGN && set.spnnum == J1939_CAN_ENGINE_SPEEP_SPN)
				{
					md.m_RedStoneData.RPM(currentvalue);
					indicateUpdate(md);
				}

				++it;
			}

			md.post_scan_sem();
		}
		else
		{
			ats_logf(ATSLOG(0), "%s,%d: CAN_read Failed or data is more than 1785 bytes: %d", __FILE__, __LINE__, bytes_read);
		}
	}

	ats_logf(ATSLOG(0), "leave h_j1939reader_thread");
	return 0;
}

bool MyData::getSignalMonitorList(int pgn, std::vector<SignalMonitor*>& vec)
{
	spnsignalMap::const_iterator it = m_spnsignalMap.begin();
	for( ;it != m_spnsignalMap.end(); ++it)
	{
		SignalData& set = ((*it).second)->getset();
		if( set.pgnnum == pgn)
			vec.push_back((*it).second);
	}

	return true;
}

MyData::MyData()
{
	m_exit_sem = new sem_t;
	sem_init(m_exit_sem, 0, 0);

	m_scan_sem = new sem_t;
	sem_init(m_scan_sem, 0, 0);

	m_mutex = new pthread_mutex_t;
	pthread_mutex_init(m_mutex, 0);

	m_command.insert(AdminCommandPair("help", AdminCommand(ac_help, "Displays help information")));
	m_command.insert(AdminCommandPair("h", AdminCommand(ac_help, "Displays help information")));
	m_command.insert(AdminCommandPair("?", AdminCommand(ac_help, "Displays help information")));

	m_command.insert(AdminCommandPair("debug", AdminCommand(ac_debug, "Displays debug information")));
	m_command.insert(AdminCommandPair("ignition", AdminCommand(ac_ignition, "Ignition on/off message")));

	m_s = create_new_CANSocket();

	if(!m_s)
	{
		ats_logf(ATSLOG(0), "%s,%d: Failed to create CANSocket\n", __FILE__, __LINE__);
		exit(1);
	}
}


// ************************************************************************
// Description: A local command server so that other applications or
//      developers can query this application.
//
//      One instance of this function is created for every local connection
//      made.
//
// Parameters:
// Return: NULL pointer
// ************************************************************************
static void* local_command_server( void* p)
{
	ClientData* cd = (ClientData*)p;
	MyData &md =* ((MyData*)(cd->m_data->m_hook));

	bool command_too_long = false;
	ats::String cmd;

	const size_t max_command_length = 1024;

	AdminCommandContext acc(md, *cd);

	ClientDataCache cdc;
	init_ClientDataCache(&cdc);

	for(;;)
	{
		char ebuf[1024];
		const int c = client_getc_cached(cd, ebuf, sizeof(ebuf), &cdc);

		if(c < 0)
		{
			if(c != -ENODATA) ats_logf(ATSLOG(0), "%s,%d: %s", __FILE__, __LINE__, ebuf);
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
			send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "command is too long\n");
			continue;
		}

		CommandBuffer cb;
		init_CommandBuffer(&cb);
		const char* err;

		if((err = gen_arg_list(cmd.c_str(), cmd.length(), &cb)))
		{
			ats_logf(ATSLOG(0), "%s,%d: gen_arg_list failed (%s)", __FILE__, __LINE__, err);
			send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "gen_arg_list failed: %s\n", err);
			cmd.clear();
			continue;
		}

		ats_logf(ATSLOG(2), "%s,%d:%s: Command received: %s", __FILE__, __LINE__, __FUNCTION__, cmd.c_str());

		cmd.clear();

		if(cb.m_argc <= 0)
		{
			continue;
		}

		{
			const ats::String cmd(cb.m_argv[0]);
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

void MyData::start_server()
{
	ServerData& sd = m_command_server;
	init_ServerData(&sd, 32);
	sd.m_hook = this;
	sd.m_cs = local_command_server;
	::start_redstone_ud_server(&sd, g_app_name.c_str(), 1);
	signal_app_unix_socket_ready(g_app_name.c_str(), g_app_name.c_str());
	signal_app_ready(g_app_name.c_str());
}

int main(int argc, char* argv[])
{
	g_log.set_global_logger(&g_log);
	g_log.set_level(g_dbg);
	g_log.open_testdata(g_app_name);

	MyData md;
	md.set("debug", "1");
	md.set("can_dev", "can0");
	md.set("can_dev_loc", "/sys/devices/platform/FlexCAN.0");
	md.set_from_args(argc - 1, argv + 1);

	g_dbg = md.get_int("debug");

	ats::String version;
	ats::get_file_line(version, "/version", 1);

	md.setversion(version);

	const ats::String& ss = "can-j1939-monitor-" + version;
	openlog(ss.c_str(), LOG_PID, LOG_USER);
	ats_logf(ATSLOG(0), "can-j1939-monitor started");

	db_monitor::ConfigDB db;

	// ensure that the Murphy and Cummins default XML are in place
	ats::String strTemplate;
	strTemplate = db.GetValue("j1939-db", "template_default");
	if (strTemplate.length() == 0)
	  system("db-config set j1939-db template_default	--file=/etc/redstone/defaultJ1939Param.xml");

	strTemplate = db.GetValue("j1939-db", "template");
	if (strTemplate.length() == 0)
	  system("db-config set j1939-db template default");

	ats::String s_sa;
	ats_sprintf(&s_sa, "%d", db.GetInt("CanJ1939Monitor", "sourceaddress", 0xF9));
	md.set("sourceaddress", s_sa);
	CAN_connect(md.m_s, md.get("can_dev").c_str(), md.get_int("sourceaddress"));
	ats_logf(ATSLOG(0), "%s,%d: J1939 SA is 0x%x", __FILE__, __LINE__, md.get_int("sourceaddress"));

	md.set("exceedStatTime", db.GetValue("CanJ1939Monitor", "exceedStatTime", "3600"));
	g_exceedStatTime = md.get_int("exceedStatTime");

	md.set("faultStatTime", db.GetValue("CanJ1939Monitor", "faultStatTime", "3600"));
	g_faultStatTime = md.get_int("faultStatTime");

	md.set("mb_periodic_overiridium_seconds", db.GetValue("CanJ1939Monitor", "periodic_overiridium_seconds", "28800")); // 8 hours default value.
	md.set("mb_overiridium_priority_periodic", db.GetValue("CanJ1939Monitor", "overiridium_priority_periodic", "8"));
	md.set("mb_overiridium_priority_exceedance", db.GetValue("CanJ1939Monitor", "overiridium_priority_exceedance", "7"));
	md.set("mb_overiridium_priority_fault", db.GetValue("CanJ1939Monitor", "overiridium_priority_fault", "6"));

	wait_for_app_ready("feature-monitor");
	FeatureQuery fq;
	g_has_message_assembler = fq.feature_on("message-assembler");

	if( g_has_message_assembler )
		wait_for_app_ready("message-assembler");

	if(pthread_create( &(md.m_reader_thread), (pthread_attr_t *)0, h_j1939reader_thread, &md))
	{
		ats_logf(ATSLOG(0), "%s,%d: Failed to create reader. (%d) %s", __FILE__, __LINE__, errno, strerror(errno));
		return -1;
	}

	if(pthread_create( &(md.m_signalmonitor_thread), (pthread_attr_t *)0, h_signalmonitor_thread, &md))
	{
		ats_logf(ATSLOG(0), "%s,%d: Failed to create signalmonitor thread. (%d) %s", __FILE__, __LINE__, errno, strerror(errno));
		return -1;
	}

	if(pthread_create( &(md.m_request_thread), (pthread_attr_t *)0, h_request_thread, &md))
	{
		ats_logf(ATSLOG(0), "%s,%d: Failed to create request thread. (%d) %s", __FILE__, __LINE__, errno, strerror(errno));
		return -1;
	}

	CanJ1939MonitorSm* w = new CanJ1939MonitorSm(md);
	w->run_state_machine(*w);

	md.start_server();
	ats::infinite_sleep();

	return 0;
}
