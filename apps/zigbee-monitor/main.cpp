#include <iostream>
#include <map>
#include <list>
#include <fstream>
#include <iomanip>

#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/file.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/ioctl.h>

#include "ats-common.h"
#include "ats-string.h"
#include "atslogger.h"
#include "timer-event.h"
#include "SingletonProcess.h"
#include "zigbee-monitor.h"

ATSLogger g_log;
MyData* g_md = NULL;

const int g_power_monitor_port = 41009;
ClientSocket g_cs_powermonitor;
static bool g_fob_sim = false;
int nFobs;

int g_KeepAliveMinutes;  // length of time to keep system alive after a zigbee communication.
int g_hazardExtensionMinutes;  // length of time to extend worker's hazard timer.
int g_shiftTimerExtensionMinutes;  // length of time to extend worker's shift timer.
int g_timerExpireMinutes;  // generate overdue before shift timer expires.
int g_AllowTimerExtensions;
int g_overdueAllow;
int g_checkinpri;
int g_checkoutpri;
int g_sospri;
int g_watStaterequestpri;
int g_staterequestMinutes;
int g_watchdog;

#define CHECKPOINTER if(fm  == NULL)\
		{\
			ats_logf(ATSLOG_ERROR, "%s,%d: Object is not FOBMessage type", __FILE__, __LINE__);\
			if(m1.ptr != NULL)\
				delete m1.ptr;\
			continue;\
		}\

static int g_dbg = 20;
static const ats::String g_app_name("zigbee-monitor");

int get_from_colon(const ats::String& str, int base = 0);

static time_t convertToTimestamp(const ats::String& dataBuf)
{
	time_t t = 0;

	if( dataBuf.size() == 12)
	{
		struct tm ts;
		memset(&ts, 0, sizeof(ts));
		sscanf(dataBuf.c_str(), "%2d%2d%2d%2d%2d%2d", &ts.tm_year, &ts.tm_mon,
	       &ts.tm_mday, &ts.tm_hour, &ts.tm_min, &ts.tm_sec);
		ts.tm_year += 100;
		ts.tm_mon -= 1;
		t = mktime(&ts);
	}

	return t;
}

static ReplyType getReplyType(const ats::String& command)
{
	ReplyType type = replyType_MANUAL;
	if(command == "PGEMCI")
		type = replyType_PGEMCI;
	else if(command == "PGEMAS")
		type = replyType_PGEMAS;
	else if(command == "PGEMAC")
		type = replyType_PGEMAC;
	else if(command == "PGEMEM")
		type = replyType_PGEMEM;
	else if(command == "PGEMEC")
		type = replyType_PGEMEC;
	else if(command == "PGEMEE")
		type = replyType_PGEMEE;
	else if(command == "PGEMHA")
		type = replyType_PGEMHA;
	else if(command == "PGEMCC")
		type = replyType_PGEMCC;
	else if(command == "Manual")
		type = replyType_MANUAL;

	return type;
}


ats::String convertHextoDec(const ats::String& s)
{
	ats::String buf;

	if(s.length() != 16)
		return ats::String();

	uint64_t pid = strtoull(s.c_str(), (char **)NULL, 16);

	ats_sprintf(&buf, "%" PRIu64, pid);

	return buf;
}

//The function is fit for xxxx:<a>,<b>,<c> format, like +PANSCAN:, or JPAN:
bool splitResponse(const ats::String& str, int& ch, int& pid, ats::String& epid)
{
	bool res = false;
	const ats::String::size_type pos = str.find_first_of(':');
	if( pos != ats::String::npos)
	{
		ats::String b = str.substr(pos + 1);

		ats::StringList spec;
		ats::split(spec, b, ",");

		ats::StringList::const_iterator i = spec.begin();

		if(spec.size() > 2)
		{
			ch = strtoll((*(i)).c_str(), 0, 16);
			pid = strtoll((*(i + 1)).c_str(), 0, 16);
			epid = (*(i + 2)).c_str();
			res = true;
		}
	}

	return res;
}

void zigbee::leftpan_fn(const zigbee& zb, const ats::String& s)
{
	ats_logf(ATSLOG_INFO, "%s,%d: enter leftpan_fn function", __FILE__, __LINE__);
	MyData& md = *(zb.m_md);
	md.m_zb->network_sem_post();
}

void zigbee::jpan_fn(const zigbee& zb, const ats::String& s)
{
	ats_logf(ATSLOG_INFO, "%s,%d: enter jpan_fn function", __FILE__, __LINE__);

	int channel, pid;
	ats::String epid;

	if(splitResponse(s, channel, pid, epid))
	{
		ats_logf(ATSLOG_INFO, RED_ON "Coordinator Create PAN: ch %x, pid %x, epid %s" RESET_COLOR, channel, pid, epid.c_str());
	}

}

void zigbee::newnode_fn(const zigbee& zb, const ats::String& s)
{
	MyData& md = *(zb.m_md);
	ats_logf(ATSLOG_INFO, "%s,%d: enter newnode_fn function", __FILE__, __LINE__);

	const ats::String::size_type pos = s.find_first_of(':');
	if( pos != ats::String::npos )
	{
		ats::String b = s.substr(pos + 1);
		ats::StringList spec;
		ats::split(spec, b, ",");

		const ats::String& key = spec[0];
		uint16_t pid = strtoll(spec[1].c_str(), 0, 16);

		if(!md.m_fob_manager.get_node(key))
		{
			fobContent *cc = md.m_fob_manager.add_node(key, new fobContent(md, key, pid, 0));
			ats_logf(ATSLOG_DEBUG, "Insert into client map %s cc: %x, %s",key.c_str() ,cc->getNodeID(), (cc->getEUI()).c_str());
			cc->run();
			cc->setState(FOB_STATE_UNKNOWN);
			send_cmd(g_cs_powermonitor.m_fd, MSG_NOSIGNAL, "set_work key=SLP:%s expire=%d\r", key.c_str(), (g_KeepAliveMinutes * 60));
			ats_logf(ATSLOG_DEBUG, "Sent zigbee keep alive to power-monitor : %d", (g_KeepAliveMinutes * 60));

			//set up register.
			if( g_overdueAllow )
			{
				cc->setupRegister();
			}
		}

		ats_logf(ATSLOG_ERROR, "New node %s",key.c_str());
		nFobs++;
		md.m_db.Update("devices", convertHextoDec(key), g_app_name);
		(md.m_fob_manager.get_node(key))->sendWatStateRequest(true);
		messageFrame m(newNodeEvent, ats::String(), key);
		md.post_message(md.get_fobstate_key(), m);
		md.set_LEDs();
	}
}

void zigbee::at_fn(const zigbee& zb, const ats::String& s)
{
	MyData& md = *(zb.m_md);

	//ats_logf(ATSLOG_INFO, "%s,%d: enter at_fn function", __FILE__, __LINE__);

	const int cmd = md.m_res_manager.getcmd();

	if(cmd < 0)
		return;

	ats::String::const_iterator it = s.begin();

	if(boost::ifind_first(s, "ACK:").begin() == it)
	{
		if(!g_fob_sim)
		{
			if(md.m_res_manager.get_wait_acknumber() == get_from_colon(s.c_str(), 16))
			{
				md.m_res_manager.signal_client("ACK");
			}
			else
			{
				ats_logf(ATSLOG_DEBUG, "Got ack, but nobody waiting for it");
			}
		}
		else
			md.m_res_manager.signal_client("ACK");
	}
	else if(boost::ifind_first(s, "NACK:").begin() == it)
	{
		if(!g_fob_sim)
		{
			if(md.m_res_manager.get_wait_acknumber() == get_from_colon(s.c_str(), 16))
			{
				md.m_res_manager.signal_client("NACK");
			}
			else
			{
				ats_logf(ATSLOG_DEBUG, "Got nack, but nobody waiting for it");
			}
		}
		else
			md.m_res_manager.signal_client("ACK");
	}
	else if (boost::ifind_first(s, "OK").begin() == it)
	{
		if(!g_fob_sim)
		{
			if(md.m_res_manager.get_wait_acknumber() == -1)
				md.m_res_manager.signal_client("OK");
		}
		else
			md.m_res_manager.signal_client("OK");
	}
	else if (boost::ifind_first(s, "SEQ:").begin() == it)
	{
		md.m_res_manager.set_wait_acknumber(get_from_colon(s.c_str(), 16));
	}
	else if (boost::ifind_first(s, "ERROR:").begin() == it)
	{
		//error = get_from_colon(s.c_str());
		md.m_res_manager.signal_client("ERROR");
	}
	else if (boost::ifind_first(s, "+N=").begin() == it)
	{
		if(cmd == 6) //at+n? command
		{
			const ats::String::size_type pos = s.find_first_of(',');

			deviceType type = nopan;

			{
				if( pos != ats::String::npos)
				{
					int len = pos - 3;
					if(len > 0 )
					{
						ats::String b = (s.substr(3, len));

						if(boost::iequals(b,"coo")) type = coo;
						else if(boost::iequals(b,"ffd")) type = ffd;

					}
				}
			}

			md.m_zb->setDeviceType(type);

			ats_logf(ATSLOG_INFO, RED_ON "device type %d" RESET_COLOR, type);
		}

	}
	else if (boost::ifind_first(s, "+PANSCAN:").begin() == it)
	{
		int channel, pid;
		ats::String epid;
		if(splitResponse(s, channel, pid, epid))
		{
			panContent* cc = new panContent(channel, pid, epid);
			md.m_pan_manager.add_node(epid, cc);
			ats_logf(ATSLOG_INFO, "+PANSCAN ch %x, pid %x, epid %s", channel, pid, epid.c_str());
		}

	}
}

void zigbee::h_ucast(const ats::String& str)
{
	//const ats::String& s = str.substr(6);

	ats_logf(ATSLOG_INFO, RED_ON "receive %s" RESET_COLOR, str.c_str());

	ats::StringList spec;
	ats::split(spec, str, ",", 1);

	ats::StringList::const_iterator i = spec.begin();

	if(spec.size() >= 2)
	{
		//Up to 82 bytes are received.
		//int data[82]  ;

		const ats::String& key = (*(i)).c_str();

		const ats::String& s = *(i + 1);
		const ats::String::size_type pos = s.find("=");
		if(pos != ats::String::npos)
		{

			const ats::String& datastr = s.substr(pos + 1);

			// New message from FOB.
			{
				messageFrame m(messageEvent, datastr, key);
				m_md->post_message(m_md->get_fobstate_key(), m);
			}

			ats_logf(ATSLOG_DEBUG, RED_ON "receive bytes data %s from %s" RESET_COLOR, datastr.c_str(), key.c_str());
		}
	}

}

void zigbee::ucast_fn(const zigbee& zb, const ats::String& s)
{
	MyData& md = *(zb.m_md);
	ats_logf(ATSLOG_INFO, "%s,%d: enter ucast_fn function", __FILE__, __LINE__);

	md.m_zb->h_ucast(s.substr(6));
}

ats::String zigbee::sendMessage(const ats::String& msg, const int timeout)
{
	lock();

	const ats::String& cmd = msg + "\r\n";
	ats_logf(ATSLOG_DEBUG, "sendMessage:"RED_ON " %s" RESET_COLOR,msg.c_str());
	write(m_md->m_fd, cmd.c_str(), cmd.size());
	
	if( g_fob_sim)
	{
		ats_logf(ATSLOG_DEBUG, "send message to fob sim: %s" ,msg.c_str());
		send_redstone_ud_msg("fob-sim", 0, cmd.c_str());
	}

	int c;
	const ats::String::const_iterator it = msg.begin();
	if(boost::ifind_first(msg, "ati").begin() == it)
		c = 0;
	else if (boost::ifind_first(msg, "at+en").begin() == it)
		c = 1;
	else if (boost::ifind_first(msg, "at+jn").begin() == it)
		c = 2;
	else if (boost::ifind_first(msg, "at+jpan").begin() == it)
		c = 3;
	else if (boost::ifind_first(msg, "at+dassl").begin() == it)
		c = 4;
	else if (boost::ifind_first(msg, "at+dassr").begin() == it)
		c = 5;
	else if (boost::ifind_first(msg, "at+n").begin() == it)
		c = 6;
	else if (boost::ifind_first(msg, "at+ntable").begin() == it)
		c = 7;
	else if (boost::ifind_first(msg, "at+sn").begin() == it)
		c = 8;
	else if (boost::ifind_first(msg, "at+ucast").begin() == it)
		c = 9;
	else if (boost::ifind_first(msg, "at+ucastb").begin() == it)
		c = 10;
	else if (boost::ifind_first(msg, "at+mcast").begin() == it)
		c = 11;
	else if (boost::ifind_first(msg, "at+mcastb").begin() == it)
		c = 12;
	else if (boost::ifind_first(msg, "at+panscan").begin() == it)
		c = 13;
	else
		c = 14;

	int retry = 2;
	ats::String r;
	while(retry)
	{
		response<ats::String>* res = new response<ats::String>(c);
		ats_logf(ATSLOG_INFO, RED_ON "retry sendMessage %p" RESET_COLOR, res);

		m_md->m_res_manager.add_client(res);

		if(!res->wait(timeout))
		{
			ats_logf(ATSLOG_DEBUG, RED_ON "Timeout for waiting response" RESET_COLOR);
			m_md->m_res_manager.remove_client();
			retry--;
			continue;
		}

		retry = 0;

		r = *(res->getresponse());

		m_md->m_res_manager.remove_client();
	}

	unlock();

	return r;
}

void MyData::post_message(const ClientData* p_client, const messageFrame& p_msg)
{
	m_msg_manager.post_msg(p_msg, ats::toStr(p_client));
}

bool MyData::get_message(const ClientData* p_client, messageFrame& p_msg)
{
	return m_msg_manager.wait_msg(ats::toStr(p_client), p_msg);
}

int get_from_colon(const ats::String& str, int base)
{
	int result = -1;

	const ats::String::size_type pos = str.find_first_of(':');
	if( pos != ats::String::npos)
	{
		ats::String b = str.substr(pos + 1);
		result = strtol(b.c_str(), 0, base);
	}

	return result;
}

void zigbee::processEvent(const statusEvent event, const ats::String& s)
{
	fnMap::const_iterator it = m_smfnMap.find(event);
	if(it != m_smfnMap.end())
	{
		(*it).second(*this, s);
	}

	ats_logf(ATSLOG_INFO, GREEN_ON "%s,%d: leave processEvent" RESET_COLOR, __FILE__, __LINE__);
}

void* zigbee::messagecenter_thread(void* p)
{
	zigbee& b = *((zigbee*)p);
	MyData& md = *(b.m_md);

	ats_logf(ATSLOG_INFO, "%s,%d: enter messagecenter_thread", __FILE__, __LINE__);

	for(;;)
	{
		messageFrame m1;

		if(!md.get_message(md.get_common_key(), m1))
		{
			continue;
		}

		const ats::String& t = m1.msg1;
		int status = 0;

		if(t.size() != 0 )
		{
			ats_logf(ATSLOG_INFO, "Receive %s", t.c_str());
			if(int socket = md.gettestSocket())
			{
				int ret = send_cmd(socket, MSG_NOSIGNAL, "%s\n", t.c_str());
				if(ret < 1)
					md.settestSocket(0);
			}

			if(boost::ifind_first(t, "at").begin() == t.begin())
			{
				ats_logf(ATSLOG_INFO, "AT command");
				status = at_event;
			}
			else if( boost::ifind_first(t, "FFD:").begin() == t.begin())
			{
				ats_logf(ATSLOG_INFO, "NEWNODE command");
				status = newnode_event;
			}
			else if(boost::ifind_first(t, "ucast:").begin() == t.begin())
			{
				ats_logf(ATSLOG_INFO, "UCAST command");
				status = ucast_event;
			}
			else if(boost::ifind_first(t, "LeftPAN").begin() == t.begin())//local node has left Pan.
			{
				ats_logf(ATSLOG_INFO, "Left PAN Prompt");
				status = leftpan_event;
			}
			else if(boost::ifind_first(t, "JPAN:").begin() == t.begin())//local node has joined Pan with given parameters and not waiting for at+jn result.
			{
				ats_logf(ATSLOG_INFO, "JPAN Prompt");
				status = jpan_event;
			}
			else
			{
				ats_logf(ATSLOG_INFO, "response");
				status = at_event;
			}

		}

		{
			b.processEvent((statusEvent)status, m1.msg1);
		}

	}

	return 0;
}

void* zigbee::ucast_thread(void* p)
{
	zigbee& b = *((zigbee*)p);
	MyData& md = *(b.m_md);

	ats_logf(ATSLOG_INFO, "%s,%d: enter ucast_thread", __FILE__, __LINE__);

	md.m_msg_manager.add_client(ats::toStr(md.get_ucast_key()), new message_queue());
	md.m_msg_manager.start_client(ats::toStr(md.get_ucast_key()));

	for(;;)
	{
		messageFrame m1;

		if(!md.get_message(md.get_ucast_key(), m1))
		{
			continue;
		}

		const int seq = m1.data;
		const ats::String& pid = m1.msg1;
		const ats::String& msg = m1.msg2;

		ats::String result;
		if((result = b.ucast(pid, msg)) == "nack")
		{
			ats_logf(ATSLOG_INFO, "Got UCAST nack: %s", pid.c_str());
		}

		int a = 0;
		if(msg[0] == 'E')
		{
			a = ucastButtonAckEvent;
		}
		else if(msg[0] == 'S')
		{
			a = ucastStatusAckEvent;
		}

		if(a)
		{
			messageFrame m(a, ats::toStr(seq), pid) ;
			md.post_message(md.get_fobstate_key(), m);
		}
	}

	return 0;
}

void* zigbee::reader_thread(void* p)
{
	zigbee& b = *((zigbee*)p);
	MyData& md = *(b.m_md);

	ats_logf(ATSLOG_INFO, "%s,%d: enter reader_thread", __FILE__, __LINE__);

	md.m_msg_manager.add_client(ats::toStr(md.get_common_key()), new message_queue());
	md.m_msg_manager.start_client(ats::toStr(md.get_common_key()));

	ReadDataCache cache;
	init_ReadDataCache(&cache);
	ats::String line;

	for(;;)
	{
		const int c = read_cached(md.m_fd, &cache);

		if(c < 0)
		{
			if(c != (-ENODATA))
				ats_logf(ATSLOG_ERROR, "%s,%d: Failed to read: (%d) %s", __FILE__, __LINE__, c, strerror(c));
			sleep(1);
			break;
		}

		if('\n' == c)
		{

			for(;;)
			{
				if(line.empty()) break;
				if('\r' == line[line.length() - 1]) line.erase(--(line.end()));
				else break;
			}

			{
				if(boost::ifind_first(line, "NEWNODE:").begin() == line.begin() ||
					boost::ifind_first(line, "SR:").begin() == line.begin())
				{
					line.clear();
					continue;
				}
			}

			messageFrame m(0, line, ats::String()) ;
			md.post_message(md.get_common_key(), m);

			line.clear();
		}
		else
		{
			line += c;
		}
	}

	close(md.m_fd);
	ats_logf(ATSLOG_INFO, "%s,%d: reader thread closed", __FILE__, __LINE__);

	return 0;
}

void coordinator::answer(const ats::String& key)
{
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

	for(;;)
	{
		char ebuf[1024];
		const int c = client_getc_cached(cd, ebuf, sizeof(ebuf), &cdc);

		if ( c < 0 )
		{
			if (c != -ENODATA)
				ats_logf(ATSLOG_INFO, "%s,%d: %s", __FILE__, __LINE__, ebuf);
				
			break;
		}

		if (c != '\r' && c != '\n')
		{
			if (cmd.length() >= max_command_length)
				command_too_long = true;
			else
				cmd += c;

			continue;
		}

		if(command_too_long)
		{
			ats_logf(ATSLOG_ERROR, "%s,%d: command is too long", __FILE__, __LINE__);
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
			ats_logf(ATSLOG_ERROR, "%s,%d: gen_arg_list failed (%s)", __FILE__, __LINE__, err);
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
				g_log.set_level(g_dbg);
				send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "%s: debug=%d\r\n", cmd.c_str(), g_dbg);
			}
			else
			{
				send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "%s: debug=%d\r\n", cmd.c_str(), g_dbg);
			}

		}
		else if("routemode" == cmd)
		{
			md.m_zb->disconnectNetwork();
			md.m_zb->sendMessage("AT+JN", 10);
			md.m_zb->getNetworkInfo();

			if(md.m_zb->getDeviceType() == zigbee::ffd)
				send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "routemode: OK\r\n");
			else
				send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "routemode: FAIL\r\n");
		}
		else if("help" == cmd)
		{
		  send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "\
zigbee-monitor help:\n\
    debug/dbg [debug level] - Get/Set debug logging level\n\
    at cmd=[at command] - Send an AT command\n\
    ucast cmd=[at command] - Send an AT command\n\
    test cmd=[at command] - Send an AT command\n\
    system cmd=[at command] - Send an AT command\n\
    linkkey cmd=[at command] - Send an AT command\n\
    calamp cmd=[at command] - Send an AT command\n\
    iridium cmd=[at command] - Send an AT command\n");
		}
		else
		{
			CommandMap::const_iterator i = md.m_cmd.find(cmd);

			if(i != md.m_cmd.end())
			{
				Command& c = *(i->second);
				c.fn(md, *cd, cb);
			}
			else
			{
				send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "Invalid command \"%s\"\r\n", cmd.c_str());
			}

		}

	}
	return 0;
}

int MyData::getFobRegSize(int reg)
{
	if(reg == 2 || reg == 39 || reg == 41)
		return 4;
	else if(reg == 40)
		return 32;

	return 2;
}

void MyData::fob_status_request(const ats::String& key, int s1, int s2)const
{
	ats_logf(ATSLOG_INFO, "%s,%d: enter fob_status_request %s", __FILE__, __LINE__, key.c_str());
	struct StatusControl sc;
	sc.key = key;
	sc.FOBDisplayState1 = s1;
	sc.FOBDisplayState2 = s2;
	m_fobrequest_manager->FOBStatusRequest(sc, false);
}

void MyData::start_server()
{
	ServerData& sd = m_command_server;
	init_ServerData(&sd, 32);
	sd.m_hook = this;
	sd.m_cs = client_command_server;
	::start_redstone_ud_server(&sd, "zigbee-monitor", 1);
	signal_app_unix_socket_ready("zigbee-monitor", "zigbee-monitor");
	signal_app_ready("zigbee-monitor");
}

void MyData::fob_remove(const ats::String& key)
{
	ats_logf(ATSLOG_ERROR, "%s,%d: Removing fob with key %s ", __FILE__, __LINE__, key.c_str());
	messageFrame m(quitEvent, ats::String(), key);
	post_message(get_fobstate_key(), m);

	m_db.Unset("devices", convertHextoDec(key));
	send_cmd(g_cs_powermonitor.m_fd, MSG_NOSIGNAL, "unset_work key=SLP:%s\r", key.c_str());
	nFobs > 1 ? nFobs-- :  nFobs = 0;  // reduce count on FOBs - don't go below zero.
	set_LEDs();
}

void* zigbee::watchdog_thread(void* p)
{
	zigbee& b = *((zigbee*)p);
	MyData& md = *(b.m_md);

	ats_logf(ATSLOG_INFO, "%s,%d: enter watchdogThread", __FILE__, __LINE__);
	
	while(g_watchdog)
	{
		ats::String response;
		if((response = md.m_zb->sendMessage("ATS0B?")) != "OK")
		{
			ats_logf(ATSLOG_ERROR, RED_ON "%s,%d: Zigbee Inactive, reset zigbee module..." RESET_COLOR, __FILE__, __LINE__);
			send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=zigbee-monitor.g led=zigbee script=\"0,1000000\" \r");  
			send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=zigbee-monitor.r led=zigbee.r script=\"1,1000000\" \r");  
			exit(1);
		}
		md.set_LEDs();
		sleep(60);
	}
	
	ats_logf(ATSLOG_INFO, "%s,%d: leave watchdogThread", __FILE__, __LINE__);
	return 0;
}

void* zigbee::fobstate_thread(void* p)
{
	zigbee& b = *((zigbee*)p);
	MyData& md = *(b.m_md);

	ats_logf(ATSLOG_INFO, "%s,%d: enter fobStateThread", __FILE__, __LINE__);

	md.m_msg_manager.add_client(ats::toStr(md.get_fobstate_key()), new message_queue());
	md.m_msg_manager.start_client(ats::toStr(md.get_fobstate_key()));

  for(;;)
	{
		messageFrame m1;

		if(!md.get_message(md.get_fobstate_key(), m1))
		{
			continue;
		}

		const int event = m1.data;
		const ats::String& s1 = m1.msg1;
		const ats::String& eui = m1.msg2;

		fobContent* b = md.m_fob_manager.get_node(eui);
		
    if( !b ) 
    	continue;

		if(event == newNodeEvent)
		{
			md.fob_status_request(eui, FDS_LOWER_GREEN_ON, 0xF0);//Brief flash in fob.
		}
		else if(event == ucastStatusAckEvent)
		{
			ats_logf(ATSLOG_INFO, "%s,%d: %s get status request result %s seq: %s", __FILE__, __LINE__, eui.c_str(), eui.c_str(), s1.c_str() );
		}
		else if(event == ucastButtonAckEvent)
		{
			ats_logf(ATSLOG_INFO, "%s,%d: %s get button ack result %s seq: %s", __FILE__, __LINE__, eui.c_str(), eui.c_str(), s1.c_str() );
		}
		else if(event == messageEvent)
		{
			ats_logf(ATSLOG_DEBUG, "%s,%d: %s message event %s", __FILE__, __LINE__, eui.c_str(), s1.c_str() );
			b->messageHandle(s1);
		}
		else if( event == quitEvent)
		{
			ats_logf(ATSLOG_INFO, RED_ON "%s,%d: %s receive quit message" RESET_COLOR, __FILE__, __LINE__, eui.c_str());
			md.m_fob_manager.remove_node(eui);
		}
		else if(event == watMessageEvent)
		{
			int state;
			bool control;
			int interval;
			int overdue;
			ReplyType reply;
			time_t sendtime;
			time_t offtime;
			time_t hazardtime;
			time_t safetytime;

			ats::StringList a;
			ats::split(a, s1, ",");
			state = atoi(a[1].c_str());
			control = atoi(a[2].c_str());
			interval = atoi(a[3].c_str());
			overdue = atoi(a[4].c_str());
			reply = getReplyType(a[5]);
			sendtime = convertToTimestamp(a[6]);
			offtime = convertToTimestamp(a[7]);
			hazardtime = convertToTimestamp(a[8]);
			ats::String token = a[9];
			if (token.find("*") != ats::String::npos)
			{
				token = token.substr(0, token.size() - 3);
			}
			safetytime = convertToTimestamp(token);

			ats_logf(ATSLOG_DEBUG, RED_ON "%s,%d: %s wat message event state:%d %lu, %lu, %lu %lu" RESET_COLOR, __FILE__, __LINE__, eui.c_str(), state, sendtime, offtime, hazardtime, safetytime);

			if( sendtime >= b->receiveTime())
			{
				b->receiveTime(sendtime);
				b->safetyTime(safetytime);
				b->hazardTime(hazardtime);
				b->offMonitorTime(offtime);
			}

			switch(state)
			{
				case 0:
					b->setState(FOB_STATE_CHECKIN);
					break;
				case 1:
					if (sendtime > safetytime) // make sure the safety timer has been exceeded - sometimes it gets updated without a state change - in that case - no warning
						b->setState(FOB_STATE_OVERDUE_SAFETY_TIMER);
					break;
				case 2:
					b->setState(FOB_STATE_ASSIST);
					break;
				case 3:
					b->setState(FOB_STATE_SOS);
					break;
				case 5:
					b->setState(FOB_STATE_DISABLE);
					break;
				case 6:
					b->setState(FOB_STATE_CHECKOUT);
					break;
				case 7:
					if (sendtime > hazardtime) // make sure the hazard timer has been exceeded - sometimes it gets updated without a state change - in that case - no warning
						b->setState(FOB_STATE_OVERDUE_HAZARD);
					break;
				case 8:
					if (sendtime > offtime) // make sure the off timer has been exceeded - sometimes it gets updated without a state change - in that case - no warning
						b->setState(FOB_STATE_OVERDUE_SHIFT_TIMER);
					break;
				case 9:
					b->setState(FOB_STATE_OVERDUE_SAFETY_AND_SHIFT);
					break;
				case 4:
				default:
					break;
			}
		}
	}

	ats_logf(ATSLOG_INFO, RED_ON "%s,%d: leave state machine" RESET_COLOR, __FILE__, __LINE__);
	return 0;
}
void MyData::devicesIniDBConfigCleanup()
{
	if(!m_db.Get("devices").empty())
	{
		ats::StringList list;
		db_monitor::ResultTable& m_table = m_db.Table();

		for(unsigned int i = 0; i < m_table.size(); ++i)
		{
			const db_monitor::ResultRow& row = m_table[i];

			if(row.size() > 3 && row[1] == "zigbee-monitor")
			{
				list.push_back(row[3]);
			}
		}

		ats::StringList::const_iterator it = list.begin();
		while(it != list.end())
		{
			m_db.Unset("devices", (*it));
			it++;
		}
	}
}

//Check the last heartbeat time of every fob in every one min, if more than 5 mins, delete the fob resource.
void* MyData::fobmanage_thread(void* p)
{
	MyData& md = *((MyData*)(p));

	for(;;)
	{
		std::vector<fobContent*> v;
		md.m_fob_manager.get_nodes(v);

		std::vector<fobContent*>::const_iterator it = v.begin();
		
		while(it != v.end())
		{
			fobContent* q = *(it);
			if(!q)
			{
				++it;
				continue;
			}

			time_t now = time(NULL);
			time_t last = q->getLastHbTimeStamp();
			ats_logf(ATSLOG_INFO, "%s,%d: check fob now %lu, last %lu", __FILE__, __LINE__, now, last);

			if(( now - last) > (5 * 60))  // remove FOB after 5 minutes of no comms - do NOT use the KeepAliveMinutes time here!
			{
				md.fob_remove(q->getEUI());
			}
			else
			{
				if((now - q->statusRequestSendTime())  > (g_staterequestMinutes * 60))
				{
					ats_logf(ATSLOG_INFO, "%s,%d: %s: sending periodic status request to FOB", __FILE__, __LINE__, q->getEUI().c_str());
					md.fob_status_request(q->getEUI(), FDS_LOWER_GREEN_ON, FDS_OFF);//send periodic status request to fob for updating GPS position.
				}

				if((now - q->watStatusRequestHourlyTime())  > (60 * 60))
				{
					//send status request to WAT hourly with high priority
					ats_logf(ATSLOG_INFO, "%s,%d: %s: sending hourly status request to WAT", __FILE__, __LINE__, q->getEUI().c_str());
					q->sendWatStateRequest(true);
				}
				else if((now - q->watStatusRequestTime())  > (5 * 60))
				{
					//send status request to WAT every 5 mins with cell priority 
					ats_logf(ATSLOG_INFO, "%s,%d: %s: sending regular status request to WAT", __FILE__, __LINE__, q->getEUI().c_str());
					q->sendWatStateRequest();
				}
			}

			++it;
		}
				
		sleep(60);
	}

	return 0;
}

void MyData::set_LEDs()
{
	if ( nFobs == 0 )  // flashing if no fobs attached
	{
		send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=zigbee-monitor.g led=zigbee script=\"1,500000;0,500000\" \r");  
		send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=zigbee-monitor.r led=zigbee.r script=\"0,1000000\" \r");  
	}
	else    // solid if fobs attached
	{
		send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=zigbee-monitor.g led=zigbee script=\"1,1000000\" \r");  
		send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=zigbee-monitor.r led=zigbee.r script=\"0,1000000\" \r");  
	}
}

int main(int argc, char* argv[])
{
	//preventing multiple process instance.
	SingletonProcess singleton(64001); // pick a port number to use that is specific to this app
   
	if (!singleton())
	{
		cerr << "process running already. See " << singleton.GetLockFileName() << endl;
		return 1;
	}
	
	// now get the log level
	{
		db_monitor::ConfigDB db;
		g_dbg = db.GetInt("zigbee", "LogLevel", 0);
	}
	
	g_log.set_global_logger(&g_log);
	g_log.set_level(g_dbg);
	g_log.open_testdata(g_app_name);

	if(ats::testmode() && (!ats::file_exists("/mnt/nvram/config/testmode-mtu")))
	{
		ats_logf(ATSLOG_ERROR, "Device Test-Mode detected, disabling Zigbee-Monitor for this power-up/runtime");
		return 0;
	}

	if(ats::file_exists("/tmp/flags/fob-sim"))
	{
		ats_logf(ATSLOG_ERROR, "Enter fob simulation support Mode");
		g_fob_sim = true;
	}

	init_ClientSocket(&g_cs_powermonitor);

	if(!ats::testmode())
	{

		if(connect_client(&g_cs_powermonitor, "127.0.0.1", g_power_monitor_port))
		{
			ats_logf(&g_log, "%s,%d:g_cs_powermonitor %s", __FILE__, __LINE__, g_cs_powermonitor.m_emsg);
			exit(1);
		}

	}

	static MyData md;
	g_md = &md;

	ats::StringMap &config = md.m_config;

	config.set("zigbee-dev-address", "/dev/ttySP2");
	config.set("zigbee-dev-baudrate", "19200");
	config.set("user", "applet");
	config.from_args(argc - 1, argv + 1);

	g_dbg = config.get_int("debug");

	ats_logf(ATSLOG_ERROR, "================ zigbee-monitor started ================");

	md.devicesIniDBConfigCleanup();

	const ats::String zigbee_dev_address(config.get("zigbee-dev-address"));

	ats::system("echo -e -n sS > /dev/set-gpio");

	const ats::String port(config.get("zigbee-dev-address"));

	if(!((access(port.c_str(), F_OK) != -1) ?  1: 0))
	{
		ats_logf(ATSLOG_DEBUG, "No zigbee device %s", port.c_str());
		exit(1);
	}

	ats::String cmd("test -e \"" + port + "\" && stty -F \"" + port + "\" " + config.get("zigbee-dev-baudrate") +
			" -brkint"
			" -icrnl"
			" -imaxbel"
			" -opost"
			" -isig"
			" -icanon"
			" -iexten"
			" -echo");

	const int ret = ats::system(cmd.c_str());

	if(!(WIFEXITED(ret) && (0 == WEXITSTATUS(ret))))
	{
		ats_logf(ATSLOG_ERROR, "Failed to configure zigbee device \"%s\" with stty", port.c_str());
	}

	int& fd = md.m_fd = open(zigbee_dev_address.c_str(), O_RDWR);

	if(fd < 0)
	{
		ats_logf(ATSLOG_ERROR, "ERR: Failed to open %s: (%d) %s", zigbee_dev_address.c_str(), errno, strerror(errno));
		return 1;
	}

	{
		md.m_zb = new coordinator(md);
		ats_logf(ATSLOG_INFO, "Enter coordinator mode");
	}

	md.m_cmd.insert(CommandPair("at", new atCommand()));
	md.m_cmd.insert(CommandPair("ucast", new ucastCommand()));
	md.m_cmd.insert(CommandPair("test", new testCommand()));
	md.m_cmd.insert(CommandPair("system", new systemCommand()));
	md.m_cmd.insert(CommandPair("linkkey", new linkkeyCommand()));
	md.m_cmd.insert(CommandPair("calamp", new calampCommand()));
	md.m_cmd.insert(CommandPair("iridium", new iridiumCommand()));

	md.m_zb->run();
	md.start_server();

	{
		db_monitor::ConfigDB db;
		const ats::String& linkKey = db.GetValue("zigbee", "link-key");
		const ats::String& lastKey = db.GetValue("zigbee", "written-link-key");
		g_KeepAliveMinutes = db.GetInt("zigbee", "AliveTimeoutMinutes", 5);
		g_hazardExtensionMinutes = db.GetInt("zigbee", "hazardExtensionMinutes", HAZARDDEFAULTEXTENSION );
		g_shiftTimerExtensionMinutes = db.GetInt("zigbee", "shiftExtensionMinutes", 60);
		g_timerExpireMinutes = db.GetInt("zigbee", "timerExpireMinutes", 5);
		g_AllowTimerExtensions = db.GetInt("zigbee", "timerExtensionAllow", 0);
		g_overdueAllow = db.GetInt("zigbee", "overdueAllow", 0);
		g_checkinpri = db.GetInt("zigbee", "Check_In_Priority", 20);
		g_checkoutpri = db.GetInt("zigbee", "Check_Out_Priority", 20);
		g_sospri = db.GetInt("zigbee", "SOS_Priority", 1);
		g_watStaterequestpri = db.GetInt("zigbee", "State_Request_Priority", 2);
		g_staterequestMinutes = db.GetInt("zigbee", "FOB_State_Request_Time", 10);
		g_watchdog = db.GetInt("zigbee", "watchdog", 1);

		if( !linkKey.empty() && linkKey.size() == 32 && linkKey != lastKey )
		{
			ats::String response;
			ats_logf(ATSLOG_ERROR, "Setting zigbee linkkey to: %s", linkKey.c_str());

			ats::String cmd = "ATS09=" + linkKey + ":password" ;
			if((response = md.m_zb->sendMessage(cmd)) != "OK")
			{
				ats_logf(ATSLOG_ERROR, "Setting zigbee linkkey failed: %s", response.c_str());
			}
			else
			{
				cmd = "ATS01=-7";
				if((response = md.m_zb->sendMessage(cmd)) != "OK")
				{
					ats_logf(ATSLOG_ERROR, "Setting zigbee power level register failed: %s", response.c_str());
				}

				cmd = "ATS0A=011C:password";
				if((response = md.m_zb->sendMessage(cmd)) != "OK")
				{
					ats_logf(ATSLOG_ERROR, "Setting zigbee main function register failed: %s", response.c_str());
				}

				db.Update("zigbee", "written-link-key", linkKey);
				md.m_zb->sendMessage("atz");
				ats_logf(ATSLOG_ERROR, "Set linkkey success, reset zigbee module" );

				sleep(2);

				exit(1);
			}
		}

		if(g_watchdog)
		{
			md.m_zb->runWatchdog_thread();
		}
	}

	if(!g_fob_sim)
	{
		if(ats::su(config.get("user")))
		{
			ats_logf(ATSLOG_ERROR, "Could not become user \"%s\": ERR (%d): %s", md.get("user").c_str(), errno, strerror(errno));
			return 1;
		}
	}

	ats::infinite_sleep();

	return 0;
}

void myTimer::on_timeout()
{
	g_md->fob_status_request(getKey(), FDS_LOWER_GREEN_ON, 0xF0); //cancel.
	enable_timer(false);
}
