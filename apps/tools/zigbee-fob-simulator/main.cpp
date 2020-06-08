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
#include "ats-timer.h"
#include "socket_interface.h"
#include "event_listener.h"
#include "state_machine.h"
#include "state-machine-data.h"
#include "command_line_parser.h"
#include "ConfigDB.h"
#include "NMEA_Client.h"


class MyData : public StateMachineData
{
public:
	MyData()
	{
		m_mutex = new pthread_mutex_t;
		pthread_mutex_init(m_mutex, 0);

		m_send_sem = new sem_t;
		sem_init(m_send_sem, 0, 0);

	}

	~MyData()
	{
		delete m_send_sem;
	}

	void lock() const
	{
		pthread_mutex_lock(m_mutex);
	}

	void unlock() const
	{
		pthread_mutex_unlock(m_mutex);
	}

	bool send_sem_post()
	{
		sem_post(m_send_sem);
		return true;
	}

	bool send_sem_wait()
	{
		sem_wait(m_send_sem);
		return true;
	}

	ats::StringMap m_config;

	db_monitor::ConfigDB m_db;
	void start_server();

private:
	ServerData m_command_server;
	pthread_mutex_t* m_mutex;
	sem_t* m_send_sem;
	pthread_t m_send_thread;

	ats::String m_eui64;
};

ATSLogger g_log;
static int g_dbg = 0;
MyData* g_md = NULL;
NMEA_Client nmea_client;
static int seqnum=1;
static const ats::String& eui64="000D6F00023EBB73";

static const ats::String g_app_name("fob-sim");
void getLocalGPSPosition(ats::String& gpsStr);

static void sendAck(const ats::String& key)
{
	g_md->lock();
	send_redstone_ud_msg("zigbee-monitor", 0, "test %s\r\n", key.c_str());
	ats_logf(ATSLOG(0), "send ack %s", key.c_str());
	g_md->unlock();
}

static void* send_thread(void* p)
{
	MyData &md = *((MyData *)p);

	for(;;)
	{
		md.send_sem_wait();
		ats::String gps;
		getLocalGPSPosition(gps);
		ats::String buf;
		ats_sprintf(&buf, "UCAST:%s,08=E%.2d00000,%s", eui64.c_str(),seqnum, gps.c_str());
		sendAck(buf);

		seqnum++;
		if(seqnum > 99)
			seqnum = 1;

		ats_logf(ATSLOG(0), "head beat");
		sleep(30);
	}
	return 0;
}

void getLocalGPSPosition(ats::String& gpsStr)
{
	ats::String fixTime;
	ats_sprintf(&fixTime, "%.2d%.2d%.2d.000", nmea_client.Hour(),nmea_client.Minute(),(short)(nmea_client.Seconds()));

	double lat = nmea_client.Lat();
	double lon = nmea_client.Lon(); 

	char dlat = (lat>0)?'N':'S';
	char dlon = (lon>0)?'E':'W';

	lat=fabs(lat);
	lon=fabs(lon);

	double intpart,fractpart;
	fractpart=modf(lat, &intpart);
	intpart=intpart*100;
	fractpart=fractpart*60;

	ats::String latStr;
	ats_sprintf(&latStr, "%.04f", intpart+fractpart);

	fractpart=modf(lon, &intpart);
	intpart=intpart*100;
	fractpart=fractpart*60;

	ats::String lonStr;
	ats_sprintf(&lonStr, "%.04f", intpart+fractpart);

	ats_sprintf(&gpsStr, "%s,%s,%c,%s,%c,%d,%d,%.2f", fixTime.c_str(), latStr.c_str(), dlat, lonStr.c_str(), dlon, nmea_client.GPS_Quality(), nmea_client.NumSVs(), nmea_client.HDOP());
}

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

		if(cmd.empty()) continue;
		ats::String cmdd=cmd;
		cmd.clear();

		if(cb.m_argc <= 0)
		{
			continue;
		}

		ats::String gps;
		getLocalGPSPosition(gps);
		const ats::String cmd(cb.m_argv[0]);

		if(("debug" == cmd) || ("dbg" == cmd))
		{

			if(cb.m_argc >= 2)
			{
				g_dbg = strtol(cb.m_argv[1], 0, 0);
				g_log.set_level(g_dbg);
				send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "%s: debug=%d\r\n", cmdd.c_str(), g_dbg);
			}
			else
			{
				send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "%s: debug=%d\r\n", cmdd.c_str(), g_dbg);
			}

		}
		else
		{
			ats_logf(ATSLOG(0), "cmd: %s", cmdd.c_str());
		 if( cmdd == "AT+EN" )
		 {
			 sleep(10);
			 ats::String buf;
			 ats_sprintf(&buf, "FFD:%s,774C", eui64.c_str());
			 sendAck(buf);
			 sendAck("OK");
			 sendAck("ACK:1");
		 }
		 else if( cmdd.find("at+ucast") != std::string::npos)
		 {
			 ats::String buf;
			 ats_sprintf(&buf, "%.2d", seqnum);
			 sendAck("SEQ:"+buf);
			 sendAck("OK");
			 sendAck("ACK:"+buf);
			 const ats::String& s = "at+ucast:"+eui64+"=S";
			 if(cmdd.find(s) != std::string::npos && cmdd.size() > 29)
			 {
				 int d = atoi(cmdd.substr(27, 2).c_str());
				 ats_sprintf(&buf, "UCAST:%s,08=S%.2d12100", eui64.c_str(), d);
				 sendAck(buf);
			 }

			 seqnum++;
			 if(seqnum > 99)
				 seqnum = 1;
			 md.send_sem_post();
		 }
		 else if(cmdd.find("checkin") != std::string::npos)
		 {
			 ats::String buf;
			 ats_sprintf(&buf, "UCAST:%s,08=E%.2d00009,%s", eui64.c_str(), seqnum, gps.c_str());
			 sendAck(buf);
			 ats_logf(ATSLOG(0), "checkin");
		 }
		 else if(cmdd.find("checkout") != std::string::npos)
		 {
			 ats::String buf;
			 ats_sprintf(&buf, "UCAST:%s,08=E%.2d0102A,%s", eui64.c_str(), seqnum, gps.c_str());
			 sendAck(buf);
			 ats_logf(ATSLOG(0), "checkout");
		 }
		 else if(cmdd.find("sos cancel") != std::string::npos)
		 {
			 ats::String buf;
			 ats_sprintf(&buf, "UCAST:%s,08=E%.2d00107,%s", eui64.c_str(),seqnum, gps.c_str());
			 sendAck(buf);
			 ats_logf(ATSLOG(0), "SOS CANCEL");
		 }
		 else if(cmdd.find("sos") != std::string::npos)
		 {
			 ats::String buf;
			 ats_sprintf(&buf, "UCAST:%s,08=E%.2d00016,%s", eui64.c_str(), seqnum, gps.c_str());
			 sendAck(buf);
			 ats_logf(ATSLOG(0), "SOS");
		 }
		 else if(cmdd.find("power off") != std::string::npos)
		 {
			 ats::String buf;
			 ats_sprintf(&buf, "UCAST:%s,08=E%.2d1003E,%s", eui64.c_str(), seqnum, gps.c_str());
			 sendAck(buf);
			 ats_logf(ATSLOG(0), "power off");
			 exit(0);
		 }
		 else
		 {
			 send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "Invalid command %s\r\n", cmdd.c_str());
			 ats_logf(ATSLOG(0), "Invalid command: %s", cmdd.c_str());
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
	sd.m_cs = client_command_server;
	::start_redstone_ud_server(&sd, g_app_name.c_str(), 1);
	signal_app_unix_socket_ready(g_app_name.c_str(), g_app_name.c_str());
	signal_app_ready(g_app_name.c_str());
}

int main(int argc, char* argv[])
{
	g_log.set_global_logger(&g_log);
	g_log.set_level(g_dbg);
	g_log.open_testdata(g_app_name);
	pthread_t m_send_thread;

	static MyData md;
	g_md = &md;

	ats::StringMap &config = md.m_config;

	config.from_args(argc - 1, argv + 1);

	g_dbg = config.get_int("debug");

	ats_logf(ATSLOG(0), "zigbee fob sim started");
	ats::touch("/tmp/flags/fob-sim");
	ats::system("killall zigbee-monitor");

	md.start_server();
	const int retval = pthread_create(&m_send_thread, 0, send_thread, &md);
	if( retval )
	{
		ats_logf(ATSLOG(0), "%s,%d: Failed to create read thread. (%d) %s", __FILE__, __LINE__, errno, strerror(errno));
		return -1;
	}

	ats::infinite_sleep();

	return 0;
}
