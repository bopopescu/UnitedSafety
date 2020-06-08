#include <iostream>
#include <list>
#include <vector>
#include <iomanip>

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <semaphore.h>
#include <unistd.h>
#include <math.h>

#include "ats-common.h"
#include "ats-string.h"
#include "atslogger.h"
#include "socket_interface.h"
#include "command_line_parser.h"
#include "expat.h"
#include "timer-event.h"
#include "db-monitor.h"
#include "ConfigDB.h"
#include "feature.h"

#include "modbus-monitor.h"

#define ENGINESPN_POWERVIEW 190
#define MAXFAILCOUNT 60
#define MAX_FAULTSETS_POWERVIEW 55
#define MAX_FAULTSETS_VFDPLC 255
#define MODBUSFIRSTHOLDINGREGISTER 40001
#define RETRYTIME_INSECONDS 10
#define MIN_CONFIGDB_TIMEOUT 120
#define MIN_PERIODIC_TIMEOUT 120
#define MIN_PERIODIC_FIRSTMESSAGE_TIMEOUT 60
#define PERIODIC_ITEMSPERGROUP 5

#define MAX_MODBUS_SLAVE_ADD 247

bool g_has_message_assembler = false;
bool g_ReportAverage = false;
ATSLogger g_log;
int g_dbg = 0;
int g_exceedStatTime;
int g_faultStatTime;
ats::String g_slow_down_script;

communicationportocol g_protocol;

static const ats::String g_app_name("modbus-monitor");

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

int ac_debug(AdminCommandContext &p_acc, const AdminCommand&, int p_argc, char* p_argv[])
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

int ac_help(AdminCommandContext &p_acc, const AdminCommand&, int p_argc, char* p_argv[])
{
	send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "This is the help system\n\r");

	return 0;
}

process::process(int slaveAddr, MyData& p_md):m_slaveAddr(slaveAddr), m_md(&p_md)
{
	m_running = false;
	m_cancel = false;

	m_mutex = new pthread_mutex_t;
	pthread_mutex_init(m_mutex, 0);
}

process::~process()
{
	stop();

	pthread_mutex_destroy(m_mutex);
	delete m_mutex;
}

void process::stop()
{
	lock();
	m_cancel = true;
	unlock();

	m_md->post_scan_sem(m_slaveAddr);
	m_md->unlock_periodic_mutex(m_slaveAddr);

	pthread_join(m_signal_thread, 0);
	pthread_join(m_periodic_thread, 0);

}

void process::lock() const
{
	pthread_mutex_lock(m_mutex);
}

void process::unlock() const
{
	pthread_mutex_unlock(m_mutex);
}

void process::run()
{
	lock();

	if(!m_running)
	{
		m_running = true;
		unlock();

		int ret;

		ret = pthread_create(&m_signal_thread, 0, process::process_signal_task, this);
		if( ret )
		{
			ats_logf(ATSLOG_DEBUG, "%s,%d: Failed to create signalmonitor thread. (%d) %s", __FILE__, __LINE__, errno, strerror(errno));
		}

		ret = pthread_create(&m_periodic_thread, 0, process::process_periodic_task, this);
		if( ret )
		{
			ats_logf(ATSLOG_DEBUG, "%s,%d: Failed to create periodic thread. (%d) %s", __FILE__, __LINE__, errno, strerror(errno));
		}

		return;
	}

	unlock();
}

static ats::String g_last_content = ats::String();
static ats::String g_groupName = ats::String();
static ats::String g_type_name = ats::String();
static int g_current_reg = 0;
static int g_slaveAddr = 0;

static SignalData g_signalData;
enum registerType { commonType, faultCodeType };
static registerType g_register_type = commonType;
static priorityType g_priority_type = IPPriorityType;
static bool	g_begin_definition = false;
static bool	g_begin_config = false;

template <typename T, size_t N>
void getFaultCode(const ats::String& str, T (&faultCode)[N])
{
	ats::StringList l;
	ats::split(l, str, ",");

	if(l.size() != N )
		return;

	int i = 0;
	ats::StringList::const_iterator it = l.begin();
	for(; it != l.end(); ++it)
	{
		int a = (int)(strtol((*it).c_str(), 0, 0));
		faultCode[i++] = a;
	}
}


void XMLCALL startElement(void *p, const char* element, const char** attribute)
{
	MyData &md = *((MyData *)p);

	nodeContentList<SignalMonitor>* b = md.m_config_manager.get_node(g_groupName);

	if( b == 0 )
		return;

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
				ats_logf(ATSLOG(5), "type %s", attribute[i + 1]);
				g_type_name = attribute[i+1];
				if( g_type_name == "powerview")
				{
					md.m_slaveMap[g_slaveAddr]->setType("powerview");
				}
				else if ( g_type_name == "powercommand" )
				{
					md.m_slaveMap[g_slaveAddr]->setType("powercommand");
				}
				else if ( g_type_name == "PLC" )
				{
					md.m_slaveMap[g_slaveAddr]->setType("VFD_PLC");
				}
			}
		}
	}
	else if ( strcmp("register", element) == 0 )
	{
		g_priority_type = IPPriorityType;
		for (size_t i = 0; attribute[i]; i += 2 )
		{
			if ( strcmp("name", attribute[i]) == 0 )
			{
				g_current_reg = (int)(strtol( attribute[i+1], 0, 0));
			}
			else if ( strcmp("type", attribute[i]) == 0 )
			{
				if ( strcmp("fault", attribute[ i + 1 ]) == 0 )
				{
					g_register_type = faultCodeType;
				}
				else
				{
					g_register_type = commonType ;
				}
			}
			else if( strcmp( "priority", attribute[i] ) == 0 )
			{
				if ( strcmp("Iridium", attribute[ i + 1 ]) == 0 )
				{
					g_priority_type = IridiumPriorityType;
				}
			}
		}
	}
	else if ( strcmp("definition", element) == 0 )
	{
		g_begin_definition = true;
		g_signalData.clean();
	}
	else if ( strcmp("config", element) == 0 )
	{
		g_begin_config = true;
	}
}

void XMLCALL endElement(void* p, const char* element)
{
	MyData &md = *((MyData *)p);

	nodeContentList<SignalMonitor>* b = md.m_config_manager.get_node(g_groupName);
	nodeContentList<SignalMonitor>* f = md.m_fault_manager.get_node(g_groupName);

	if( b == 0 || f == 0 )
		return;

	if(strcmp(element, "config") == 0)
	{
		g_begin_config = false;
	}
	else if(strcmp(element, "definition") == 0)
	{
		g_begin_definition = false;
	}
	else if( g_begin_definition )
	{
		if (g_current_reg)
		{
			if (strcmp(element, "desc") == 0)
			{
				g_signalData.desc = g_last_content;
			}
			else if (strcmp(element, "pgn") == 0)
			{
				g_signalData.pgn = (int)(strtol(g_last_content.c_str(), 0, 0));
			}
			else if (strcmp(element, "spn") == 0)
			{
				g_signalData.spn = (int)(strtol(g_last_content.c_str(), 0, 0));
			}
			else if (strcmp(element, "size") == 0)
			{
				g_signalData.size = (int)(strtol(g_last_content.c_str(), 0, 0));
			}
			else if (strcmp(element, "multiplier") == 0)
			{
				g_signalData.multiplier = (float)(strtof(g_last_content.c_str(), NULL));
			}
			else if (strcmp(element, "offset") == 0)
			{
				g_signalData.offset= (int)(strtol(g_last_content.c_str(), 0, 0));
			}
			else if (strcmp(element, "units") == 0)
			{
				g_signalData.unit = g_last_content;
			}
			else if (strcmp(element, "hrange") == 0)
			{
				if((int)(g_signalData.multiplier) == 1)
					g_signalData.hrange =  (int)(strtol(g_last_content.c_str(), 0, 0));
				else
					g_signalData.hrange = (float)(strtof(g_last_content.c_str(), NULL));
			}
			else if (strcmp(element, "lrange") == 0)
			{
				if((int)(g_signalData.multiplier) == 1 )
					g_signalData.lrange =  (int)(strtol(g_last_content.c_str(), 0, 0));
				else
					g_signalData.lrange = (float)(strtof(g_last_content.c_str(), NULL));
			}
			else if (strcmp(element, "nadata") == 0)
			{
				g_signalData.nadata = (int)(strtoul(g_last_content.c_str(), 0, 16));
			}
			else if (strcmp(element, "faultcode") == 0)
			{
				const ats::String& fc = g_last_content;
				getFaultCode(fc, g_signalData.faultCode);

				if( md.m_slaveMap[g_slaveAddr]->getType() == "powercommand" )
				{
					for( int i = 0; i < CUMMINS_FAULTCODE_BITMAPSIZE; i++)
					{
						ats_logf(ATSLOG(5), "Reg %d fault code %d ", g_current_reg, g_signalData.faultCode[i]);
					}

					SignalMonitor* s = b->get(g_current_reg);
					if(s == NULL)
					{
						g_signalData.slaveAddr = g_slaveAddr;
						g_signalData.reg = g_current_reg;
						g_signalData.type = g_register_type;
						g_signalData.typeName = g_type_name;
						f->add(new SignalMonitor(md, g_signalData, g_slaveAddr));
					}
				}
			}
		}
	}
	else if (g_begin_config)
	{
		if(g_current_reg)
		{
			SignalMonitor* s = b->get(g_current_reg);
			if(s == NULL)
			{
				g_signalData.slaveAddr = g_slaveAddr;
				g_signalData.reg = g_current_reg;
				g_signalData.type = g_register_type;
				g_signalData.priType = g_priority_type;
				g_signalData.typeName = g_type_name;
				b->add(new SignalMonitor(md, g_signalData, g_slaveAddr));
				s = b->get(g_current_reg);
			}

			SignalData& signalData = s->get();

			int data = (int)(strtol(g_last_content.c_str(), 0, 0));
			if (strcmp(element, "tlimit") == 0)
			{
				if( (int)(signalData.multiplier) == 1 )
					signalData.HiExLimit =  (int)(strtol(g_last_content.c_str(), 0, 0));
				else
					signalData.HiExLimit = (float)(strtof(g_last_content.c_str(), NULL));
			}
			else if (strcmp(element, "llimit") == 0)
			{
				if( (int)(signalData.multiplier) == 1 )
					signalData.LowExLimit =  (int)(strtol(g_last_content.c_str(), 0, 0));
				else
					signalData.LowExLimit = (float)(strtof(g_last_content.c_str(), NULL));
			}
			else if (strcmp(element, "hduration") == 0)
			{
				ats_logf(ATSLOG_DEBUG, "%s,%d: HiExDuration found and set to %d", __FILE__, __LINE__, data);				
				signalData.HiExDuration = data;
			}
			else if (strcmp(element, "hrecovery") == 0)
			{
				ats_logf(ATSLOG_DEBUG, "%s,%d: HiExRecoveryDuration found and set to %d", __FILE__, __LINE__, data);				
				signalData.HiExRecovery = (data > 10) ? data: 10;  // must be at least 10 seconds
			}
			else if (strcmp(element, "lduration") == 0)
			{
				ats_logf(ATSLOG_DEBUG, "%s,%d: LowExDuration found and set to %d", __FILE__, __LINE__, data);				
				signalData.LowExDuration = (data > 10) ? data: 10;  // must be at least 10 seconds
			}
			else if (strcmp(element, "lrecovery") == 0)
			{
				ats_logf(ATSLOG_DEBUG, "%s,%d: LowExRecoveryDuration found and set to %d", __FILE__, __LINE__, data);				
				signalData.LowExRecovery = data;
			}
			else if (strcmp(element, "hrepeat") == 0)
			{
				signalData.hrepeat = data;
			}
			else if (strcmp(element, "lrepeat") == 0)
			{
				signalData.lrepeat = data;
			}
			else if (strcmp(element, "type") == 0)
			{
				signalData.type = data;
			}
		}
	}
}

static void handle_data(void* p, const char* content, int length)
{
	g_last_content = ats::String(content).substr(0,length);
}

static void h_xml_parse( void* p, const ats::String& buf, const ats::String& groupName, int slaveAddr )
{
	if( slaveAddr < 1 ) return;

	MyData &md = *((MyData *)p);
	g_groupName = groupName;
	g_last_content = ats::String();
	g_current_reg = 0;
	g_slaveAddr = slaveAddr;

	XML_Parser parser = XML_ParserCreate(NULL);
	if (parser == NULL)
	{
		ats_logf(ATSLOG_DEBUG, "%s,%d: Xml parser not created", __FILE__, __LINE__);
		return ;
	}

	XML_SetUserData(parser, &md);

	XML_SetElementHandler(parser, startElement, endElement);

	XML_SetCharacterDataHandler(parser, handle_data);

	if ( XML_STATUS_ERROR == XML_Parse( parser, buf.c_str(), buf.size(), 0 ) )
	{
		ats_logf(ATSLOG_DEBUG, "failed to parser: %s( line:%lu, column:%lu )", XML_ErrorString( XML_GetErrorCode( parser ) ),
				XML_GetCurrentLineNumber( parser ), XML_GetCurrentColumnNumber( parser ));
	}

	XML_ParserFree(parser);
}

static void xml_process(void* p)
{
	MyData &md = *((MyData *)p);
	db_monitor::ConfigDB db;

	std::map<int, slaveDevice*>::iterator it = md.m_slaveMap.begin();
	for(; it != md.m_slaveMap.end(); ++it)
	{
		const ats::String& dataName = ((*it).second)->getTemplateName();

		//chop attached address string in the template name.
		size_t found = dataName.find_last_of('_');
		if( found == ats::String::npos )
		{
			ats_logf(ATSLOG_DEBUG, "%s,%d: Failed to parse template name: %s", __FILE__, __LINE__, dataName.c_str());
			continue;
		}

		const ats::String& tempName = dataName.substr(0, found);
		const ats::String& templateName = "template_" + tempName;
		const ats::String& info = db.GetValue( "modbus-db", templateName);
		const int slaveAddr = (*it).first;

		if(info.empty()) continue;

		md.m_config_manager.add_node(dataName, new nodeContentList<SignalMonitor>());
		md.m_fault_manager.add_node(dataName, new nodeContentList<SignalMonitor>());

		h_xml_parse( &md, info, dataName, slaveAddr );

		//check xml
		{
			nodeContentList<SignalMonitor>* q = md.m_config_manager.get_node( dataName );
			std::vector<SignalMonitor*>::const_iterator i = ((q)->get()).begin();
			for(; i != ((q)->get()).end(); ++i)
			{
				SignalData& signalData = (*i)->get();

				ats_logf(ATSLOG_DEBUG, "reg:%d, desc:%s, pgn:%d, spn:%d, hrange:%.1f, lrange:%.1f, nadata:%x",
						signalData.reg, signalData.desc.c_str(), signalData.pgn, signalData.spn, signalData.hrange, signalData.lrange, signalData.nadata);

			}

			q = md.m_fault_manager.get_node( dataName );
			i = ((q)->get()).begin();
			for(; i != ((q)->get()).end(); ++i)
			{
				SignalData& signalData = (*i)->get();

				ats_logf(ATSLOG_DEBUG, "reg:%d, desc:%s, pgn:%d, spn:%d, hrange:%.1f, lrange:%.1f, nadata:%x, faultCode 0: %d, faultCode 15: %d",
						signalData.reg, signalData.desc.c_str(), signalData.pgn, signalData.spn, signalData.hrange, signalData.lrange, signalData.nadata, signalData.faultCode[0], signalData.faultCode[15]);

			}
		}
	}

	return;

}

static void* h_tcpwd_thread(void* p)
{
	MyData &md = *((MyData *)p);
	EventListener listener(md);
	int m_polling_seconds = 2;
	const int maxfailcount = 5;
	int failcount = maxfailcount;

	if( g_protocol != com_protocol_tcp)
		return 0;

	for(;;)
	{
		ats::TimerEvent* timer = new ats::TimerEvent(m_polling_seconds);
		md.add_event_listener(timer, __PRETTY_FUNCTION__, &listener);

		AppEventHandler e(listener.wait_event());

		if(e.m_event->m_reason.is_cancelled())
		{
			ats_logf(ATSLOG_DEBUG, "%s,%d: timer cancelled", __FILE__, __LINE__);
			break;
		}
		else if(e.m_event == timer)
		{
			int wd = md.get_tcp_wd();
			if( wd > -1 )
			{
				if( wd == 0 )
				{
					--failcount;
					if( failcount<0)
					{
						ats_logf(ATSLOG_DEBUG, "%s,%d: tcp watchdog working", __FILE__, __LINE__);
						{
							std::map<int, slaveDevice*>::iterator it = md.m_slaveMap.begin();
							for(; it != md.m_slaveMap.end(); ++it)
							{
								int slaveAddr = ((*it).second)->getSlaveAddr();
								md.setModbusState(false, slaveAddr);
								ats::String cancelEvent = "PERIODICTIMER_" + ats::toStr(slaveAddr);
								md.post_cancel_event(cancelEvent);
								cancelEvent = "PERIODICTIMERIRIDIUM_" + ats::toStr(slaveAddr);
								md.post_cancel_event(cancelEvent);
							}
						}
						md.reset_tcp_wd(-1);
					}
				}
				else
				{
					md.reset_tcp_wd();
					failcount = maxfailcount;
				}
			}
		}
	}

	return 0;

}

static void* h_read_thread(void* p)
{
	MyData &md = *((MyData *)p);

	xml_process(&md);

	int m_polling_seconds = 1;
	EventListener listener(md);
	int scanfail[255] = {0};

	int rpm[255] = {0};

	std::map< int, AFS_Timer*> timerMap;
	std::map<int, slaveDevice*>::iterator it = md.m_slaveMap.begin();
	for(; it != md.m_slaveMap.end(); ++it)
	{
		int slaveAddr = ((*it).second)->getSlaveAddr();
		timerMap[slaveAddr] = (new AFS_Timer);

		if(((*it).second)->getType() == "powerview")
			rpm[slaveAddr] = 1;
	}

	//ATS FIXME: too much code in loop, refactoring later.
	for(;;)
	{
		ats::TimerEvent* timer = new ats::TimerEvent(m_polling_seconds);
		md.add_event_listener(timer, __PRETTY_FUNCTION__, &listener);

		AppEventHandler e(listener.wait_event());

		if(e.m_event->m_reason.is_cancelled())
		{
			ats_logf(ATSLOG_DEBUG, "%s,%d: timer cancelled", __FILE__, __LINE__);
			break;
		}
		else if(e.m_event == timer)
		{
			std::vector<nodeContentList<SignalMonitor>*> vn;
			md.m_config_manager.get_node( vn );
			std::vector<nodeContentList<SignalMonitor>*>::const_iterator it = vn.begin();
			for(; it != vn.end(); ++it)
			{
				std::vector<SignalMonitor*>::const_iterator i = ((*it)->get()).begin();
				for(; i != ((*it)->get()).end(); ++i)
				{
					SignalData& signalData = (*i)->get();

					if(!md.getModbusState(signalData.slaveAddr))
					{
						if(timerMap[signalData.slaveAddr]->DiffTime() < RETRYTIME_INSECONDS )
						{
							continue;
						}
					}

					uint8_t dest[1024]; //setup memory for data
					uint16_t * dest16 = (uint16_t *) dest;
					memset(dest, 0, sizeof(dest));

					int number = signalData.size;
					unsigned int value = signalData.nadata;
					int ret = md.modbusReadData(signalData.reg - MODBUSFIRSTHOLDINGREGISTER, number, dest16, signalData.slaveAddr);

					if(ret == number)
					{
						if(signalData.size == 2)
						{
							value = ((dest16[1] << 16) | dest16[0]);
						}
						else if(signalData.size == 1)
						{
							value = dest16[0];
						}

						if( value != signalData.nadata)
						{
							if( (int)(signalData.multiplier) == 1 )
							{
								ats_logf(ATSLOG(5), "%s: Reg:%d, origin data 0x%x, real data:%d, slave %d", signalData.desc.c_str(), signalData.reg, value, (int)(value), signalData.slaveAddr);
							}
							else
							{
								float a = signalData.multiplier;
								float fdata = (float)(value*(float)a);
								ats_logf(ATSLOG(5), "%s: Reg:%d, origin data %x, real data:%.2f, slave %d ", signalData.desc.c_str(), signalData.reg, value, fdata, signalData.slaveAddr);
							}

							(*i)->set(value);

							scanfail[signalData.slaveAddr] = 0;
							md.setModbusState(true, signalData.slaveAddr);
							md.post_scan_sem(signalData.slaveAddr);
							md.unlock_periodic_mutex(signalData.slaveAddr);
						}

						if(rpm[signalData.slaveAddr] == 1 && (( signalData.spn ) == ENGINESPN_POWERVIEW ))
						{
						  const unsigned int rpm = value;
							md.m_RedStoneData.RPM(rpm);

							struct timespec updatetime;
							int res = clock_gettime(CLOCK_MONOTONIC, &updatetime);
							if(res == 0)
							{
								md.m_RedStoneData.ObdLastUpdated(updatetime.tv_sec);
							}
						}
					}
					else
					{
						//Invalid data;
						if(ret < 0)
						{
							ats_logf(ATSLOG(5), "%s,%d: Slave threw exception err:%s in slave %d ", __FILE__, __LINE__,
									modbus_strerror(errno), signalData.slaveAddr);
						}
						else
						{
							ats_logf(ATSLOG(5), "%s,%d: Number of registers returned does not match number of registers requested!.err:%s",
									__FILE__, __LINE__, modbus_strerror(errno));
						}

						if( scanfail[signalData.slaveAddr]++ > MAXFAILCOUNT )
						{
//							if(md.getModbusState(signalData.slaveAddr))
							{
								ats_logf(ATSLOG(5), "%s,%d: Scan of address %d failed too many times (%d) - ", __FILE__, __LINE__, signalData.slaveAddr, MAXFAILCOUNT);
								ats::String strTemp;
								
								if (g_slow_down_script.length() > 0)
								{
									strTemp = "/bin/sh /mnt/nvram/config/" + g_slow_down_script + "&";
									system(strTemp.c_str());
								}

							}
							scanfail[signalData.slaveAddr] = 0;
							md.setModbusState(false, signalData.slaveAddr);
							timerMap[signalData.slaveAddr]->SetTime();
							ats::String cancelEvent = "PERIODICTIMER_" + ats::toStr(signalData.slaveAddr);
							md.post_cancel_event(cancelEvent);
							cancelEvent = "PERIODICTIMERIRIDIUM_" + ats::toStr(signalData.slaveAddr);
							md.post_cancel_event(cancelEvent);
						}
					}
				}
			}

			//handle fault codes
			{
				std::map<int, slaveDevice*>::iterator it = md.m_slaveMap.begin();
				for(; it != md.m_slaveMap.end(); ++it)
				{
					if( ((*it).second)->getType() == "powerview" )
					{
						int slaveAddr = (*it).first;
						int actftreg = md.get_int("mb_actft_register");

						if(!md.getModbusState(slaveAddr))
							continue;

						int8_t dest[1024]; //setup memory for data
						uint16_t * dest16 = (uint16_t *) dest;
						memset(dest, 0, sizeof(dest));
						int number = 1;

						int ret = md.modbusReadData( actftreg - MODBUSFIRSTHOLDINGREGISTER, number, dest16, slaveAddr );
						if( number == ret)
						{
							int count = dest16[0];
							if( count > MAX_FAULTSETS_POWERVIEW )
								continue;
							else if( !count )
								((*it).second)->clearFaultCodeMap();

							for( int i = 0; i < count; i++ )
							{
								int startAddress = md.get_int("mb_actft_startreg") - MODBUSFIRSTHOLDINGREGISTER + i * MURPHY_ACTFT_SIZE;
								uint16_t * dest16 = (uint16_t *) dest;
								memset(dest, 0, sizeof(dest));

								int number = MURPHY_ACTFT_SIZE;
								int ret = md.modbusReadData(startAddress, number, dest16, slaveAddr);
								
								if(ret == number)
								{
									int spn = dest16[3];
									int fm = dest16[1] ;
									int oc = dest16[2] ;
									{
										bool needSendMessage = true;

										if( ((*it).second)->checkFaultCodeSpn(spn))
										{
											int ocinmap = ((*it).second)->getFaultOC(spn);
											if( ocinmap >= oc || ((*it).second)->faultStatTimer.DiffTime() < g_faultStatTime)
											{
												needSendMessage = false;
											}
										}

										if( needSendMessage )
										{
											ats::String s_buf;
											ats_sprintf(&s_buf, "$PATSFT,%d,%d,%d,%d",slaveAddr,spn, fm, oc);
											const int pri = md.get_int("mb_overiridium_priority_fault");
											const ats::String& sbuf = s_buf + Checksum(s_buf);

											ats_logf(ATSLOG_DEBUG, "Murphy PowerView Fault Code: id %d, spn %x, fmi %x, occuCount %x",
													slaveAddr, spn, fm, oc);
											IF_PRESENT(message_assembler, send_app_msg(g_app_name.c_str(), "message-assembler", 0,
														"msg calamp_user_msg msg_priority=%d usr_msg_data=\"%s\"\r", pri, ats::to_hex(sbuf).c_str()))

											((*it).second)->insertFaultMap(spn, oc);
											((*it).second)->faultStatTimer.SetTime();
										}
									}
								}
							}
						}
					}
					else if( ((*it).second)->getType() == "powercommand" )
					{
						int slaveAddr = (*it).first;

						if(!md.getModbusState(slaveAddr))
							continue;

						nodeContentList<SignalMonitor>* q = md.m_fault_manager.get_node( ((*it).second)->getTemplateName() );
						std::vector<SignalMonitor*>::const_iterator i = ((q)->get()).begin();
						for(; i != ((q)->get()).end(); ++i)
						{
							SignalData& signalData = (*i)->get();
							int8_t dest[128]; //setup memory for data
							uint16_t * dest16 = (uint16_t *) dest;
							memset(dest, 0, sizeof(dest));
							int number = 1;

							int ret = md.modbusReadData( signalData.reg - MODBUSFIRSTHOLDINGREGISTER , number, dest16, slaveAddr );
							if( number == ret)
							{
								int value = dest16[0];
								if( !value)
								{
									for( int i = 0; i < CUMMINS_FAULTCODE_BITMAPSIZE; i++)
									{
										int code = signalData.faultCode[i];
										if( code )
										{
											((*it).second)->clearFaultCodeSet(code);
										}
									}
									continue;
								}
								else
								{
									for( int i = 0; i < CUMMINS_FAULTCODE_BITMAPSIZE; i++)
									{
										int code = signalData.faultCode[i];
										if( code && ((value >> i) & 0x01))
										{
											if(((*it).second)->insertFaultCode(code))
											{
												ats::String s_buf;
												ats_sprintf(&s_buf, "$PATSFC,%d,%d",slaveAddr,code);
												const ats::String& sbuf = s_buf + Checksum(s_buf);
												ats_logf(ATSLOG_DEBUG, "Cummins PowerCommand Fault Code: id %d, faultCode %d", slaveAddr, code);
												IF_PRESENT(message_assembler, send_app_msg(g_app_name.c_str(), "message-assembler", 0,
															"msg calamp_user_msg usr_msg_data=\"%s\"\r", ats::to_hex(sbuf).c_str()))
											}
										}
									}
								}
							}
						}
					}
					else if( ((*it).second)->getType() == "VFD_PLC" )
					{
						int slaveAddr = (*it).first;

						if(!md.getModbusState(slaveAddr))
							continue;

						int ftreg = md.get_int("mb_plc_ft_register");
						int alarmreg = md.get_int("mb_plc_alarm_register");

						if(!md.getModbusState(slaveAddr))
							continue;

						int8_t dest[1024]; //setup memory for data
						uint16_t * dest16 = (uint16_t *) dest;
						memset(dest, 0, sizeof(dest));
						int number = 1;

						int address[2] = {ftreg, alarmreg};
						for( int i = 0; i < 2 ; ++i)
						{
							int ret = md.modbusReadData( address[i] - MODBUSFIRSTHOLDINGREGISTER, number, dest16, slaveAddr );
							if( number == ret)
							{
								int count = dest16[0];
								if( count > MAX_FAULTSETS_VFDPLC)
									continue;

								else if( !count )
									((*it).second)->clearFaultCodeMap();

								for( int j = 0; j < count; j++ )
								{
									int startAddress = address[i] + VFDPLC_FT_OFFSET - MODBUSFIRSTHOLDINGREGISTER + j * VFDPLC_FT_SIZE;
									uint16_t * dest16 = (uint16_t *) dest;
									memset(dest, 0, sizeof(dest));

									int number = VFDPLC_FT_SIZE;
									int ret = md.modbusReadData(startAddress, number, dest16, slaveAddr);
									if(ret == number)
									{
										int spn = dest16[0];
										int oc = dest16[1] ;
										{
											bool needSendMessage = true;

											if( ((*it).second)->checkFaultCodeSpn(spn))
											{
												int ocinmap = ((*it).second)->getFaultOC(spn);
												if( ocinmap >= oc || ((*it).second)->faultStatTimer.DiffTime() < g_faultStatTime)
												{
													needSendMessage = false;
												}
											}

											if( needSendMessage )
											{
												ats::String s_buf;
												ats_sprintf(&s_buf, "$PATSFT,%d,%d,0,%d",slaveAddr,spn, oc);
												const int pri = md.get_int("mb_overiridium_priority");
												const ats::String& sbuf = s_buf + Checksum(s_buf);

												ats_logf(ATSLOG_DEBUG, "VFD PLC Fault Code: id %d, spn %x, occuCount %x",
														slaveAddr, spn, oc);

												if( i == 0)
												{
													IF_PRESENT(message_assembler, send_app_msg(g_app_name.c_str(), "message-assembler", 0,
																"msg calamp_user_msg msg_priority=%d usr_msg_data=\"%s\"\r", pri, ats::to_hex(sbuf).c_str()))
												}

												else
												{
													IF_PRESENT(message_assembler, send_app_msg(g_app_name.c_str(), "message-assembler", 0,
																"msg calamp_user_msg usr_msg_data=\"%s\"\r", ats::to_hex(sbuf).c_str()))
												}

												((*it).second)->insertFaultMap(spn, oc);
												((*it).second)->faultStatTimer.SetTime();
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	// clear timer.
	{
		std::map< int, AFS_Timer*>::const_iterator i =  timerMap.begin();
		while(i != timerMap.end())
		{
			delete (*i).second;
			++i;
		}
	}

	return 0;
}

int MyData::modbusReadData(int startRegAddress, int numberofReg, uint16_t* data, int slaveAddr)
{
	ats_logf(ATSLOG(5), "%s,%d: enter modbusReadData", __FILE__, __LINE__);

	int ret = 0; //return value from read functions

	if(m_mb == NULL || numberofReg < 0 || numberofReg > 50 || data == NULL ) return ret;

	lock();

	modbus_set_slave(m_mb, slaveAddr);

	ret = modbus_read_registers(m_mb, startRegAddress, numberofReg, data);

	if( g_protocol == com_protocol_tcp && ret > 0 )
	{
		kick_tcp_wd();
	}

	ats_logf(ATSLOG(5), "%s,%d: ret = %d", __FILE__, __LINE__, ret);

	unlock();

	return ret;
}

bool MyData::modbusConnectTCP()
{
	bool ret = true;

	const int port = get_int("mb_portovertcp");
	db_monitor::ConfigDB db;
	struct sockaddr_in sa;
	ats::String ip;
	while(1)
	{
		ip = db.GetValue("modbus", "ipovertcp");
		if(ip.empty())
		{
			ats_logf(ATSLOG_DEBUG, "%s,%d: No Modbus Server IP Specified", __FILE__, __LINE__);
		}
		else if ( inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr)) != 1)
		{
			ats_logf(ATSLOG_DEBUG, "%s,%d: Invalid Modbus Server IP", __FILE__, __LINE__);
		}
		else
		{
			break;
		}

		sleep(10);
	}

	m_mb = modbus_new_tcp(ip.c_str(), port);
	if(!m_mb)
	{
		ats_logf(ATSLOG_DEBUG, "%s,%d: Unable to allocate modbus context", __FILE__, __LINE__);
		exit(1);
	}

	if(get_int("mb_debug")  == 1)
		modbus_set_debug(m_mb, TRUE);
	else
		modbus_set_debug(m_mb, FALSE);

	modbus_set_error_recovery(m_mb,modbus_error_recovery_mode(
				MODBUS_ERROR_RECOVERY_LINK |
				MODBUS_ERROR_RECOVERY_PROTOCOL));

	//Response timeout
	struct timeval response_timeout;
	response_timeout.tv_sec = 0;
	response_timeout.tv_usec = 200000;
	modbus_set_response_timeout(m_mb, &response_timeout);

	while(1)
	{
		if (modbus_connect(m_mb) == -1)
		{
			ats_logf(ATSLOG_DEBUG, "%s,%d: Connection Failed: %s", __FILE__, __LINE__, modbus_strerror(errno));
			sleep(10);
			continue;
		}
		break;
	}

	return ret;
}

bool MyData::modbusConnectRTU(const ats::String& dev, int baudrate, char parity, int start_bits, int stop_bits)
{
	bool ret = true;

	m_mb = modbus_new_rtu(dev.c_str(), baudrate, parity, start_bits, stop_bits);
	if(!m_mb)
	{
		ats_logf(ATSLOG_DEBUG, "%s,%d: Unable to allocate modbus context", __FILE__, __LINE__);
		exit(1);
	}

	if(get_int("mb_debug")  == 1)
		modbus_set_debug(m_mb, TRUE);
	else
		modbus_set_debug(m_mb, FALSE);

	modbus_set_error_recovery(m_mb,modbus_error_recovery_mode(
				MODBUS_ERROR_RECOVERY_LINK |
				MODBUS_ERROR_RECOVERY_PROTOCOL));

	//Response timeout
	struct timeval response_timeout;
	response_timeout.tv_sec = 0;
	response_timeout.tv_usec = 100000;
	modbus_set_response_timeout(m_mb, &response_timeout);

	if (modbus_connect(m_mb) == -1)
	{
		ats_logf(ATSLOG_DEBUG, "%s,%d: Connection Failed: %s", __FILE__, __LINE__, modbus_strerror(errno));
		exit(1);
	}

	return ret;
}

bool h_qualify_signal_task(nodeContentList<SignalMonitor >* p_q, std::vector<J1939Parameter*> p_pList)
{
	std::vector<J1939Parameter*>::iterator sc_it;
	std::vector<SignalMonitor*>::const_iterator q_i = (p_q->get()).begin();
	while(q_i != (p_q->get()).end())
	{
		sc_it = p_pList.begin();
		while(sc_it != p_pList.end())
		{
			if(((*sc_it)->pgn == (*q_i)->getpgn()) &&((*sc_it)->spn == (*q_i)->getspn()))
			{
				if(!(*q_i)->IsInExceedence())
				{
					ats_logf(ATSLOG(5), "%s,%d:[SIGNAL TASK]PGN:%d SPN:%d failed the qualification process",
							__FILE__, __LINE__, (*q_i)->getpgn(), (*q_i)->getspn());
					return false;
				}
			}
			++sc_it;
		}
		++q_i;
	}
	ats_logf(ATSLOG(5), "%s,%d: Qualification process passed.", __FILE__, __LINE__);
	return true;
}

void *process::process_signal_task(void* p)
{
	process& pr = *((process*) p );
	MyData& md = *(pr.m_md);
	int addr = pr.m_slaveAddr;

	REDSTONE_IPC redstone_ipc_data;
	db_monitor::ConfigDB db;
	ats::String app = "modbus";
	std::vector<J1939Parameter *> pList;
	ats_logf(ATSLOG_DEBUG, "%s,%d:[SIGNAL TASK%d]Loading qualification PGNs", __FILE__, __LINE__,addr);
	{
		for(int i = 1; i < 4; i++)
		{
			ats::String pgn_str,spn_str;
			ats_sprintf(&pgn_str, "PGN%u", i );
			ats_sprintf(&spn_str, "SPN%u", i );
			const int pgn = db.GetInt(app,pgn_str,0);
			const int spn = db.GetInt(app, spn_str, 0);
			if((pgn != 0) && (spn != 0))
			{
				pList.push_back(new J1939Parameter(pgn,spn));
			}
		}
	}
	int q_delay_seconds = db.GetInt(app, "q_delay_seconds",0);
	AFS_Timer qual_timer;
	ats_logf(ATSLOG_DEBUG, "%s,%d:[SIGNAL TASK%d]Loading limits for messaging", __FILE__, __LINE__, addr);
	int retry = md.get_int("mb_configdb_timeout");
	if( retry < MIN_CONFIGDB_TIMEOUT ) retry = MIN_CONFIGDB_TIMEOUT;

retry:
	const ats::String name(md.m_slaveMap[addr]->getTemplateName());
	nodeContentList<SignalMonitor>* q = md.m_config_manager.get_node(name);

	if(!q)
	{
		retry--;
		if(retry > 0)
		{
			sleep(1);
			goto retry;
		}

		ats_logf(ATSLOG_DEBUG, "%s,%d:[SIGNAL TASK%d]No such config found: slave addr %d", __FILE__, __LINE__, addr, addr);
		return NULL;
	}

	bool isQualified = false;
	bool tempQualified = false;
	ats_logf(ATSLOG_DEBUG, "%s,%d:[SIGNAL TASK%d]Start scanning register values", __FILE__, __LINE__, addr);
	while(!pr.m_cancel)
	{
		// wait sema
		md.wait_scan_sem(addr);
		{
			if(!redstone_ipc_data.IgnitionOn())
			{
				ats_logf(ATSLOG_DEBUG, "%s,%d:[SIGNAL TASK%d]Ignition Off Condition detected. Reseting qualification process for defined signals.",
						__FILE__, __LINE__,addr);
				isQualified = false;
				sleep(5);
			}
			if(isQualified)
			{
				std::vector<SignalMonitor*>::const_iterator i = (q->get()).begin();
				while(i != ((q)->get()).end())
				{
					(*i)->scan();
					++i;
				}
			}
			else
			{
				if(h_qualify_signal_task(q, pList))
				{
					if(!tempQualified)
					{
						qual_timer.SetTime();
						tempQualified = true;
					}

					if(qual_timer.DiffTime() > q_delay_seconds)
					{
						ats_logf(ATSLOG_DEBUG, "%s,%d:[SIGNAL TASK%d]Switching to Normal mode. Qualification of defined signals passed.",
								__FILE__, __LINE__, addr);
						isQualified = true;
					}
				}
				else
				{
					tempQualified = false;
				}
			}
		}
	}

	return 0;
}

void resetPeriodicData(MyData* p, int addr)
{
	MyData &md = *((MyData *)p);
	std::vector<nodeContentList<SignalMonitor>*> vn;
	md.m_config_manager.get_node(vn);

	std::vector<nodeContentList<SignalMonitor>*>::const_iterator it = vn.begin();
	while(it != vn.end())
	{
		std::vector<SignalMonitor*>::const_iterator i = ((*it)->get()).begin();
		while(i != ((*it)->get()).end())
		{
				SignalData& signalData = (*i)->get();
				if(signalData.slaveAddr == addr)
				{
					(*i)->resetPeriodicData();
				}

			++i;
		}

		++it;
	}
}

void* process::process_periodic_task(void* p)
{
	process& pr = *((process*) p );
	MyData& md = *(pr.m_md);
	bool overiridiumOff = false;

	int addr = pr.m_slaveAddr;

	int periodic_seconds = md.get_int("mb_periodic_seconds");
	if( periodic_seconds < MIN_PERIODIC_TIMEOUT ) periodic_seconds = MIN_PERIODIC_TIMEOUT;

	int periodic_firstmessage_seconds = md.get_int("mb_periodic_firstmessage_seconds");
	if( periodic_firstmessage_seconds < MIN_PERIODIC_FIRSTMESSAGE_TIMEOUT ) periodic_firstmessage_seconds = MIN_PERIODIC_FIRSTMESSAGE_TIMEOUT;

	if( periodic_firstmessage_seconds > periodic_seconds) periodic_firstmessage_seconds = periodic_seconds;

	int periodic_overiridium_seconds = md.get_int("mb_periodic_overiridium_seconds");
	if( periodic_overiridium_seconds == 0 )
	{
		overiridiumOff = true;
	}
	else if( periodic_overiridium_seconds < periodic_seconds ) periodic_overiridium_seconds = periodic_seconds;

	int retry = md.get_int("mb_configdb_timeout");
	if( retry < MIN_CONFIGDB_TIMEOUT ) retry = MIN_CONFIGDB_TIMEOUT;

	bool send_first_message = false;

	typedef std::map <int, std::list<periodicRecord> > periodicRecordMap;
	periodicRecordMap m_periodicRecordMap;

retry:

	const ats::String name( md.m_slaveMap[addr]->getTemplateName());
	const ats::String type( md.m_slaveMap[addr]->getType());
	nodeContentList<SignalMonitor>* q = md.m_config_manager.get_node(name);

	if(!q)
	{
		retry--;
		if(retry > 0)
		{
			sleep(1);
			goto retry;
		}

		ats_logf(ATSLOG_DEBUG, "%s,%d: No such config found: slave addr %d", __FILE__, __LINE__, addr);
		return NULL;
	}

  	while(!pr.m_cancel)
	{
		if(!md.getModbusState(addr))
		{
			resetPeriodicData(&md, addr);
			send_first_message = false;
			sleep(5);
			continue;
		}

		EventListener listener(md);

		//The first message always set priority to highest.
		ats::TimerEvent* timer = new ats::TimerEvent((send_first_message == true)?periodic_seconds:periodic_firstmessage_seconds);
		timer->set_default_event_name("PERIODICTIMER_" + ats::toStr(addr));

		ats::TimerEvent* timer_overiridium = NULL;
		if( overiridiumOff == false)
		{
			timer_overiridium = new ats::TimerEvent(periodic_overiridium_seconds);
			timer_overiridium->set_default_event_name("PERIODICTIMERIRIDIUM_" + ats::toStr(addr));
		}

		md.add_event_listener(timer, __PRETTY_FUNCTION__, &listener);

		if( overiridiumOff == false)
			md.add_event_listener(timer_overiridium, __PRETTY_FUNCTION__, &listener);

		for(;;)
		{
			if(!md.getModbusState(addr))
			{
				break;
			}

			AppEventHandler e(listener.wait_event());

			if(e.m_event->m_reason.is_cancelled())
			{
				ats_logf(ATSLOG_DEBUG, "%s,%d: Slave %d timer cancelled", __FILE__, __LINE__, addr);
				break;
			}
			else if(e.m_event == timer || e.m_event == timer_overiridium)
			{
				if(!md.getModbusState(addr))
				{
					break;
				}

				bool overiridium = false;
				if( e.m_event == timer_overiridium && overiridiumOff == false)
				{
					overiridium = true;
				}

				md.lock_periodic_mutex(addr);
				{
					int interval;
					if( !send_first_message )
					{
						interval = periodic_firstmessage_seconds/60;
					}
					else if( overiridium )
					{
						interval = periodic_overiridium_seconds/60;
					}
					else
						interval = periodic_seconds/60;

					std::vector<SignalMonitor*>periodicMessageOverIridiumList;
					for(std::vector<SignalMonitor*>::const_iterator i = ((q)->get()).begin(); i != ((q)->get()).end(); ++i)
					{
						SignalData signalData = (*i)->get();
						if( signalData.priType == IridiumPriorityType )
						{
							periodicMessageOverIridiumList.push_back(*i);
						}
					}

					ats::String buf = "$PATSPM," + ats::toStr(addr) + "," + ats::toStr(interval);

					ats::String s_buf_1;
					ats::String s_buf_3;
					int count = 0;
					int splitcount = (((q)->get()).size())/2;
					std::vector<SignalMonitor*>::const_iterator i;
					std::vector<SignalMonitor*>::const_iterator i_end;
					i = ((q)->get()).begin();
					i_end = ((q)->get()).end();

					if( overiridium || ( !send_first_message && overiridiumOff == false ))
					{
						splitcount = periodicMessageOverIridiumList.size()/2;
						i = periodicMessageOverIridiumList.begin();
						i_end = periodicMessageOverIridiumList.end();
					}

					while(i != i_end )
					{
						SignalData signalData = (*i)->get();
						if(( signalData.type & (1 << EVENT_TYPE_PERIODIC)))
						{
							periodicdata pd;
							(*i)->getperiodicdata(pd);
							periodicRecord r;
							r.m_min = pd.minvalue;
							r.m_max = pd.maxvalue;
							int timeStamp = r.m_timestamp = time(NULL);

							int spn = pd.spn;
							if( type == "VFD_PLC" )
								spn = pd.pgn;
							std::list<periodicRecord>& list = m_periodicRecordMap[spn];
							list.push_back(r);
							list.sort(compareTimeStamp);

							{
								std::list<periodicRecord>::iterator it = list.begin();
								while( it != list.end())
								{
									if(timeStamp - (*it).m_timestamp > (2 * periodic_overiridium_seconds)) //over two times of periodic_overiridium_seconds.
									{
										list.erase(it++);
										continue;
									}
									++it;
								}
							}

							ats::String s_buf_2;
						  if(overiridium)
							{
								std::list<periodicRecord>::iterator it = list.begin();
								while( it != list.end())
								{
									if(timeStamp - (*it).m_timestamp > periodic_overiridium_seconds)
									{
										++it;
										continue;
									}

									float min = (*it).m_min;
									float max = (*it).m_max;
									if( pd.minvalue > min ) pd.minvalue = min;
									if( pd.maxvalue < max ) pd.maxvalue = max;

									++it;
								}
							}

							//If add more items here, must update  PERIODIC_ITEMSPERGROUP macro.
							if (g_ReportAverage)
								ats_sprintf(&s_buf_2, ",%d,%d,%.1f,%.1f,%.1f", pd.pgn, pd.spn, pd.minvalue, pd.maxvalue, pd.averagevalue);
							else
							ats_sprintf(&s_buf_2, ",%d,%d,%.1f,%.1f,%.1f", pd.pgn, pd.spn, pd.minvalue, pd.maxvalue, pd.currentvalue);
								
							++count;
							if( count <= splitcount )
							{
								s_buf_1.append(s_buf_2);
							}
							else
							{
								s_buf_3.append(s_buf_2);
							}
						}
						++i;
					}

					//send splited periodic messages.
					int downcount = 2;
					while(downcount--)
					{
						ats::String buff;
						if( downcount == 1)
						{
							if( s_buf_1.empty())
								continue;
							buff = buf + "," + ats::toStr(splitcount) + "," + ats::toStr(PERIODIC_ITEMSPERGROUP) + s_buf_1;
						}
						else if( downcount == 0 )
						{
							if( s_buf_3.empty())
								continue;
							buff = buf + "," + ats::toStr(count - splitcount) + "," + ats::toStr(PERIODIC_ITEMSPERGROUP) + s_buf_3;
						}

						const ats::String& sbuf = buff + Checksum(buff);
						if( overiridium || ( !send_first_message && overiridiumOff == false ))
						{
							const int pri = md.get_int("mb_overiridium_priority_periodic");
							ats_logf(ATSLOG_DEBUG, "%s,%d: Periodic message %s with Priority %d", __FILE__, __LINE__, sbuf.c_str(), pri);
							IF_PRESENT(message_assembler, send_app_msg(g_app_name.c_str(), "message-assembler", 0,
										"msg calamp_user_msg msg_priority=%d usr_msg_data=\"%s\"\r", pri, ats::to_hex(sbuf).c_str()))
						}
						else
						{
							ats_logf(ATSLOG_DEBUG, "%s,%d: Periodic message %s", __FILE__, __LINE__, sbuf.c_str());
							IF_PRESENT(message_assembler, send_app_msg(g_app_name.c_str(), "message-assembler", 0,
										"msg calamp_user_msg usr_msg_data=\"%s\"\r", ats::to_hex(sbuf).c_str()))
						}
					}

					if( overiridium )
					{
						listener.destroy_event(timer);
						timer = new ats::TimerEvent(periodic_seconds);
						timer->set_default_event_name("PERIODICTIMER_" + ats::toStr(addr));

						timer_overiridium = new ats::TimerEvent(periodic_overiridium_seconds);
						timer_overiridium->set_default_event_name("PERIODICTIMERIRIDIUM_" + ats::toStr(addr));

						md.add_event_listener(timer, __PRETTY_FUNCTION__, &listener);
						md.add_event_listener(timer_overiridium, __PRETTY_FUNCTION__, &listener);
					}
					else
					{
						timer = new ats::TimerEvent(periodic_seconds);
						timer->set_default_event_name("PERIODICTIMER_" + ats::toStr(addr));
						md.add_event_listener(timer, __PRETTY_FUNCTION__, &listener);
					}

					if(!send_first_message) send_first_message = true;
				}
			}
		}
	}

	return 0;
}

MyData::MyData():m_mb(NULL),
	m_responseTimeout(1),
	m_startRegAddress(0),
	m_numberofReg(1),
	m_devicetype(murphy_device),
	m_tcp_watchdog(-1)
{
	memset(&m_modbusState, 1, sizeof(m_modbusState));

	m_exit_sem = new sem_t;
	sem_init(m_exit_sem, 0, 0);

	m_mutex = new pthread_mutex_t;
	pthread_mutex_init(m_mutex, 0);

	m_tcpwd_mutex = new pthread_mutex_t;
	pthread_mutex_init(m_tcpwd_mutex, 0);

	m_command.insert(AdminCommandPair("help", AdminCommand(ac_help, "Displays help information")));
	m_command.insert(AdminCommandPair("h", AdminCommand(ac_help, "Displays help information")));
	m_command.insert(AdminCommandPair("?", AdminCommand(ac_help, "Displays help information")));

	m_command.insert(AdminCommandPair("debug", AdminCommand(ac_debug, "Displays debug information")));
}

MyData::~MyData()
{
	pthread_join(m_read_thread, 0);
	if( g_protocol == com_protocol_tcp)
		pthread_join(m_tcpwd_thread, 0);

	pthread_mutex_destroy(m_mutex);
	delete m_mutex;
	pthread_mutex_destroy(m_tcpwd_mutex);
	delete m_tcpwd_mutex;

	modbus_close(m_mb);
	modbus_free(m_mb);
}

//-------------------------------------------------------------------------------------------------
//
int main(int argc, char* argv[])
{
  int dbg_level;
	{  // scoping is required to force closure of db-config database
		db_monitor::ConfigDB db;
		dbg_level = db.GetInt(argv[0], "LogLevel", 0);  // 0:Error 1:Debug 2:Info
	}
	
	g_log.set_global_logger(&g_log);
	g_log.set_level(dbg_level);	
	g_log.open_testdata(g_app_name);

	MyData md;
	md.set("debug", "1");

	ats::String version;
	ats::get_file_line(version, "/version", 1);

	md.setversion(version);

	db_monitor::ConfigDB db;

	const ats::String& ss = "modbus-monitor-" + version;
	openlog(ss.c_str(), LOG_PID, LOG_USER);
	ats_logf(ATSLOG_ERROR, "---------- MODBUS MONITOR Started ----------");

	ats::String strTemp;
	strTemp = db.GetValue("modbus", "startup_script");

	if (strTemp.length() > 0)
	{
		strTemp = "/bin/sh /mnt/nvram/config/" + strTemp + "&";
		system(strTemp.c_str());
		ats_logf(ATSLOG_DEBUG, "startup script %s", strTemp.c_str());
	}

	g_slow_down_script = db.GetValue("modbus", "slow_down_script");
	
	g_ReportAverage = db.GetBool("modbus", "ReportAverage", "1");
	// ensure that the Murphy and Cummins default XML are in place
	ats::String strTemplate;
	strTemplate = db.GetValue("modbus-db", "template_Murphy");
	if (strTemplate.length() == 0)
	  system("db-config set modbus-db template_Murphy	--file=/etc/redstone/defaultMurphy.xml");
	strTemplate = db.GetValue("modbus-db", "template_Cummins");
	if (strTemplate.length() == 0)
	  system("db-config set modbus-db template_Cummins	--file=/etc/redstone/defaultCummins.xml");
	strTemplate = db.GetValue("modbus-db", "template_VFD");
	if (strTemplate.length() == 0)
	  system("db-config set modbus-db template_VFD --file=/etc/redstone/defaultVFD.xml");

	md.set("mb_slaveaddress", db.GetValue("modbus", "slaveaddress", "1"));
	md.set("mb_responseTimeout", db.GetValue("modbus", "responseTimeout", "1"));
	md.set("mb_baudrate", db.GetValue("modbus", "baudrate", "9600"));
	md.set("mb_dev", db.GetValue("modbus", "dev", "/dev/ttySP1"));
	md.set("mb_periodic_seconds", db.GetValue("modbus", "periodic_seconds", "900")); //15 mins default value.
	md.set("mb_periodic_firstmessage_seconds", db.GetValue("modbus", "periodic_firstmessage_seconds", "60")); //1 min default value.
	md.set("mb_periodic_overiridium_seconds", db.GetValue("modbus", "periodic_overiridium_seconds", "3600")); // 60 mins default value.
	md.set("mb_overiridium_priority_periodic", db.GetValue("modbus", "overiridium_priority_periodic", "8"));
	md.set("mb_overiridium_priority_exceedance", db.GetValue("modbus", "overiridium_priority_exceedance", "7"));
	md.set("mb_overiridium_priority_fault", db.GetValue("modbus", "overiridium_priority_fault", "6"));
	md.set("mb_actft_register", db.GetValue("modbus", "actft_register", "41001")); //default register which provide number of sets of active fault codes.
	md.set("mb_actft_startreg", db.GetValue("modbus", "actft_startreg", "41006")); //Fault Code: start address of all fault sets
	md.set("mb_plc_ft_register", db.GetValue("modbus", "plc_ft_register", "41000")); //default register which provide number of sets of fault codes In VFD PlC.
	md.set("mb_plc_alarm_register", db.GetValue("modbus", "plc_alarm_register", "41600")); //default register which provide number of sets of fault codes In VFD PlC.
	md.set("mb_actft_startreg", db.GetValue("modbus", "actft_startreg", "41006")); //Fault Code: start address of all fault sets
	md.set("mb_data_bits", db.GetValue("modbus", "data_bits", "8"));
	md.set("mb_stop_bits", db.GetValue("modbus", "stop_bits", "1"));
	md.set("mb_debug", db.GetValue("modbus", "debug", "0"));
	md.set("mb_configdb_timeout", db.GetValue("modbus", "configdb_timeout", "120"));
	md.set("mb_exceedStatTime", db.GetValue("modbus", "exceedStatTime", "3600"));
	g_exceedStatTime = md.get_int("mb_exceedStatTime");

	md.set("mb_faultStatTime", db.GetValue("modbus", "faultStatTime", "3600"));
	g_faultStatTime = md.get_int("mb_faultStatTime");

	//TCP or RTU
	md.set("mb_protocol", db.GetValue("modbus", "protocol", "rtu"));
	md.set("mb_portovertcp", db.GetValue("modbus", "portovertcp", "502"));
	md.set("mb_ipovertcp", db.GetValue("modbus", "ipovertcp", ""));

	const ats::String& p = md.get("mb_protocol");
	g_protocol = com_protocol_rtu;
	if( p == "tcp" || p == "TCP")
	{
		g_protocol = com_protocol_tcp;
	}

	md.set_from_args(argc - 1, argv + 1);

	wait_for_app_ready("feature-monitor");
	FeatureQuery fq;
	g_has_message_assembler = fq.feature_on("message-assembler");

	if( g_has_message_assembler)
		wait_for_app_ready("message-assembler");

	{
		std::vector<ats::String> keyList;
		db.GetKeyList( "modbus-db", keyList );

		std::vector<ats::String>::const_iterator it = keyList.begin();
		for( ; it != keyList.end(); ++it )
		{
			const ats::String& str = (*it);
			if( str.size() <= 5) continue;
			size_t found = str.find("slave");

			if ( found != 0 ) continue;

			const int addr = atoi(str.substr(5).c_str());
			if( addr > MAX_MODBUS_SLAVE_ADD || addr < 0 )
			{
				ats_logf(ATSLOG_DEBUG, "Invalid modbus slave address" );
				break;
			}

			const ats::String& templateName = db.GetValue( "modbus-db", str);
			if( templateName.empty() ) continue;

			//attach templateName with its own address for handling multi slave point to same template file.
			md.m_slaveMap[addr] = new slaveDevice(md, addr, templateName + "_" + str.substr(5));
		}
	}

	const int count = md.m_slaveMap.size();
	if(count > 0)
	{
		md.modbusSetResponseTimeout(md.get_int("mb_responseTimeout"));

		ats::String parity(db.GetValue("modbus", "parity", "N")); //Parity: none: 'N'; even: 'E'; odd: 'O'
		const char c = toupper(parity[0]);
		switch( c )
		{
			case 'E':
			case 'O':
				parity = c;
				break;
			default:
				parity = 'N';
		}

		md.set("mb_parity", parity);

		ats::write_file("/dev/set-gpio", "D");

		if( g_protocol == com_protocol_rtu)
		{
			md.modbusConnectRTU(md.get("mb_dev"), md.get_int("mb_baudrate"),
					(md.get("mb_parity"))[0], md.get_int("mb_data_bits"), md.get_int("mb_stop_bits"));
		}
		else
		{
			md.modbusConnectTCP();
			const int retval = pthread_create(
					&(md.m_tcpwd_thread),
					(pthread_attr_t *)0,
					h_tcpwd_thread,
					&md);
			if( retval )
			{
				ats_logf(ATSLOG_DEBUG, "%s,%d: Failed to create read thread. (%d) %s", __FILE__, __LINE__, errno, strerror(errno));
				return -1;
			}
		}

		{
			const int retval = pthread_create(
					&(md.m_read_thread),
					(pthread_attr_t *)0,
					h_read_thread,
					&md);
			if( retval )
			{
				ats_logf(ATSLOG_DEBUG, "%s,%d: Failed to create read thread. (%d) %s", __FILE__, __LINE__, errno, strerror(errno));
				return -1;
			}
		}

		std::map<int, slaveDevice*>::iterator it = md.m_slaveMap.begin();
		for(; it != md.m_slaveMap.end(); ++it)
		{
			((*it).second)->run();
		}
	}

	ats::infinite_sleep();
	return 0;
}

