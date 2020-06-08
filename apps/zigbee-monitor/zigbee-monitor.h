#pragma once

#include <semaphore.h>
#include <boost/algorithm/string.hpp>
#include <sys/time.h>

#include "timer-event.h"
#include "socket_interface.h"
#include "event_listener.h"
#include "state_machine.h"
#include "state-machine-data.h"
#include "command_line_parser.h"
#include "ClientMessageManager.h"
#include "zigbee-base.h"
#include "messages.h"
#include "ConfigDB.h"
#include "NMEA_Client.h"

#include "calampdecode.h"
#include "iridiumdecode.h"

#define refDateGMT2008 1199145600
#define MAXUNIXTIMESTAMP 0x7FFFFFFF
#define MINUNIXTIMESTAMP 946728000 //use J2000.0 timestamp as minumum timestamp.

extern int g_timerExpireMinutes;
extern int g_overdueAllow;
extern int g_AllowTimerExtensions;
class MyData;
class fobContent;
class Command;
class commonMessage;

const int FOBBRIEFFLASHREG = 28;
const int FOBBRIEFFLASHREGDATA = 6;
const int FOBVIBRATORREPRE = 30;
const int FOBVIBRATORREPREGDATA = 0;
const int FOBTIMERPERIODIC=5;

enum FOB_DISPLAY_STATE
{
// For FOB Display State 1 
  FDS_LOWER_GREEN_ON = 0x80,
  FDS_LOWER_RED_ON = 0x40,
  FDS_BLOCK_SOS = 0x02,
// For FOB Display State 2
  FDS_BUZZER_ON = 0x80,
  FDS_VIBE_ON = 0x40,
  FDS_GREEN_ON = 0x20,
  FDS_RED_ON = 0x10,
  FDS_OFF = 0x00,
  FDS_FAST = 0x01,
  FDS_MED = 0x02,
  FDS_SLOW = 0x03,
  FDS_BRIEF = 0x04,
  FDS_SOLID = 0x08,
  FDS_ALL_OFF = (FDS_BUZZER_ON | FDS_VIBE_ON | FDS_GREEN_ON | FDS_RED_ON |FDS_OFF),
  FDS_ORANGE_FLASH = (FDS_GREEN_ON | FDS_RED_ON | FDS_BRIEF),
  FDS_ORANGE_FLASH_AND_VIBE = (FDS_VIBE_ON | FDS_GREEN_ON | FDS_RED_ON | FDS_BRIEF)
};

class myTimer: public ats::TimerEvent
{
public:

	myTimer(int p_sec) : ats::TimerEvent(p_sec, 0), running(false)
	{
		m_key = ats::String();
		m_msg = ats::String();
	}

	void setKey(const ats::String& key){m_key = key;}
	void setMsg(const ats::String& msg){m_msg = msg;}
	const ats::String& getKey()const {return m_key;}
	const ats::String& getMsg()const {return m_msg;}
	void setstate(bool state) {running = state;}
	bool checkstate()const {return running;}

protected:
	virtual void on_timeout();

private:
	ats::String m_key;
	ats::String m_msg;
	bool running;
};

class panContent
{
public:

	panContent(uint16_t channel, uint16_t pid, ats::String epid):m_epid(epid), m_pid(pid), m_channel(channel) { }

	ats::String getEPID()const{return m_epid;}
	uint16_t getpid()const{return m_pid;}
	uint16_t getchannel()const{return m_channel;}

private:

	ats::String m_epid;
	uint16_t m_pid;
	uint16_t m_channel;
};

typedef std::map <const ats::String, Command*> CommandMap;
typedef std::pair <const ats::String, Command*> CommandPair;

typedef ats::ClientMessageQueue<struct messageFrame> message_queue;


class zigbee
{

public:

	typedef void (*process_fn)(const zigbee&, const ats::String&);
	typedef std::map <const int, process_fn> fnMap;

	enum deviceType
	{
		nopan,
		coo,
		ffd,
		zed,
		sed,
		med
	};

	virtual ~zigbee()
	{
		delete m_mutex;
		delete m_send_mutex;
		sem_destroy(m_network_sem);
		delete m_network_sem;
	}

	zigbee(MyData& p_data)
	{
		m_md = &p_data;
		m_smfnMap[newnode_event] = newnode_fn;
		m_smfnMap[at_event] = at_fn;
		m_smfnMap[ucast_event] = ucast_fn;
		m_smfnMap[jpan_event] = jpan_fn;
		m_smfnMap[leftpan_event] = leftpan_fn;

		m_mutex = new pthread_mutex_t;
		pthread_mutex_init(m_mutex, 0);

		m_send_mutex = new pthread_mutex_t;
		pthread_mutex_init(m_send_mutex, 0);

		m_network_sem = new sem_t;
		sem_init(m_network_sem, 0, 0);
	}

	pthread_t m_reader_thread;
	pthread_t m_msgcenter_thread;
	pthread_t m_ucast_thread;
	pthread_t m_fobstate_thread;
	pthread_t m_watchdog_thread;

	virtual void run()
	{
		pthread_create(&m_reader_thread, 0, zigbee::reader_thread, this);
		pthread_create(&m_msgcenter_thread, 0, zigbee::messagecenter_thread, this);
		pthread_create(&m_ucast_thread, 0, zigbee::ucast_thread, this);
		pthread_create(&m_fobstate_thread, 0, zigbee::fobstate_thread, this);

		zigbee::setup();
	}

	void runWatchdog_thread()
	{
		pthread_create(&m_watchdog_thread, 0, zigbee::watchdog_thread, this);
	}

	virtual void setup()
	{
		getInfo();
		getNetworkInfo();
	}

	virtual void answer(const ats::String&) = 0;
	virtual void createNetwork() = 0;

	void lock() const
	{
		pthread_mutex_lock(m_mutex);
	}

	void unlock() const
	{
		pthread_mutex_unlock(m_mutex);
	}

	ats::String sendMessage(const ats::String& msg, const int timeout = 5);

	void getInfo()
	{
		sendMessage("ati");
	}

	ats::String getNetworkInfo()
	{
		return sendMessage("at+n?");
	}

	void getPan()
	{
		sendMessage("at+panscan");
	}

	ats::String getNeighbourTable()
	{
		return sendMessage("AT+ntable:00,ff");
	}

	ats::String ucast(const ats::String& pid, const ats::String& content)
	{
		const ats::String& c = "at+ucast:" + (pid) + "=" + content;

		return sendMessage(c);
	}

	void disconnectNetwork()
	{
		if(getDeviceType() != nopan)
		{
			sendMessage("at+dassl");
			network_sem_wait();
		}
	}

	bool network_sem_post()
	{
		sem_post(m_network_sem);
		return true;
	}

	bool network_sem_wait()
	{
		sem_wait(m_network_sem);
		return true;
	}

	deviceType getDeviceType(){ return m_deviceType;}
	void setDeviceType(deviceType mode){ m_deviceType = mode;}

	void h_ucast(const ats::String& str);

	static void newnode_fn(const zigbee&, const ats::String&);
	static void at_fn(const zigbee&, const ats::String&);
	static void response_fn(const zigbee&, const ats::String&);
	static void ucast_fn(const zigbee&, const ats::String&);
	static void jpan_fn(const zigbee&, const ats::String&);
	static void leftpan_fn(const zigbee&, const ats::String&);
	void processEvent(const statusEvent, const ats::String&);

private:

	MyData* m_md;

	static void* reader_thread(void* p);
	static void* messagecenter_thread(void* p);
	static void* ucast_thread(void* p);
	static void* fobstate_thread(void* p);
	static void* watchdog_thread(void* p);

	pthread_mutex_t* m_mutex;
	pthread_mutex_t* m_send_mutex;

	fnMap m_smfnMap;

	deviceType m_deviceType;

	sem_t* m_network_sem;

};

class coordinator : public zigbee
{

public:

	virtual ~coordinator()
	{
		m_stop_timer = true;
	};

	coordinator(MyData& p_data):zigbee(p_data)
	{
		m_md = &p_data;
		m_stop_timer = false;
		m_timeout = 0;
	}

	virtual void run()
	{
		zigbee::run();
		setup();

	}

	void createNetwork()
	{
		sendMessage("AT+EN");
	}

	void setup()
	{
		disconnectNetwork();
		createNetwork();
	}

	pthread_t m_clientpollingtimer_thread;

	ats::String timeout_thread_key() const
	{
		return ats::toStr(m_clientpollingtimer_thread);
	}

	void answer(const ats::String&);
	bool isrunning(){return !m_stop_timer;}

private:

	MyData* m_md;

	int m_timeout;
	bool m_stop_timer;
};

class MyData : public StateMachineData
{

public:

	MyData()
	{

		m_fd = -1;
		m_common_message = new ClientData();
		m_ucast_message = new ClientData();
		m_fobstate_message = new ClientData();
		m_mutex = new pthread_mutex_t;
		pthread_mutex_init(m_mutex, 0);

		m_fobrequest_manager = new FOBRequestManager(*this);

		m_testSocket = 0;

		pthread_create(&m_fobmanage_thread, 0, MyData::fobmanage_thread, this);
	}

	~MyData()
	{
		if (m_fd != -1)
			close(m_fd);

		delete m_common_message;
		delete m_ucast_message;
		delete m_fobstate_message;
		delete m_fobrequest_manager;
	}

	static void* fobmanage_thread(void* p);

	void lock() const
	{
		pthread_mutex_lock(m_mutex);
	}

	void unlock() const
	{
		pthread_mutex_unlock(m_mutex);
	}

	zigbee* m_zb;

	CommandMap m_cmd;
	ats::StringMap m_config;

	ats::ClientMessageManager<struct messageFrame > m_msg_manager;
	responseManager<ats::String > m_res_manager;
	nodeManager<fobContent> m_fob_manager;
	nodeManager<panContent> m_pan_manager;
	FOBRequestManager* m_fobrequest_manager;

	int getFobRegSize(int reg);
	int m_fd;

	db_monitor::ConfigDB m_db;
	const ClientData* get_common_key() const {return m_common_message;}
	const ClientData* get_ucast_key() const {return m_ucast_message;}
	const ClientData* get_fobstate_key() const {return m_fobstate_message;}

	void post_message(const ClientData* p_client, const messageFrame& msg);
	bool get_message(const ClientData* p_client, messageFrame& p_msg);

	void start_server();
	void fob_status_request(const ats::String& key, int s1, int s2)const;
	void fob_remove(const ats::String& key);
	void devicesIniDBConfigCleanup();
	void set_LEDs();
	
	void settestSocket(int socket)
	{
		lock();
		m_testSocket = socket;
		unlock();
	}

	int gettestSocket()const {return m_testSocket;}

private:
	ServerData m_command_server;
	pthread_mutex_t* m_mutex;
	pthread_t m_fobmanage_thread;

	int m_mode;
	ats::String m_eui64;

	ClientData* m_common_message;
	ClientData* m_ucast_message;
	ClientData* m_fobstate_message;
	int m_testSocket;
};

class Command
{
public:
	virtual ats::String fn(MyData&, ClientData&, const CommandBuffer&) = 0;
};

class testCommand : public Command
{
public:

	virtual ats::String fn(MyData& p_md, ClientData& p_cd, const CommandBuffer& p_cb)
	{
		ats::String buf;
		
		if(p_cb.m_argc < 2)
		{
			ats_sprintf(&buf, "error: usage: %s \"command\"", p_cb.m_argv[0]);
			send_cmd(p_cd.m_sockfd, MSG_NOSIGNAL, "%s\n\r", buf.c_str());
			return ats::String();
		}

		ats_logf(ATSLOG_INFO, "%s,%d: enter test command", __FILE__, __LINE__);
		const ats::String& cmd = ats::String(p_cb.m_argv[1]);

		messageFrame m(0, cmd, ats::String()) ;
		p_md.post_message(p_md.get_common_key(), m);

		ats_sprintf(&buf, "<resp atcmd=\"%s\" status=\"OK\">TEST OK</resp>",cmd.c_str());
		send_cmd(p_cd.m_sockfd, MSG_NOSIGNAL, "%s\n\r", buf.c_str());

		return ats::String();
	}
};

//Format: "ucast pid=\"000D6F000220F374\" msg="E0100009""
class ucastCommand : public Command
{
public:

	virtual ats::String fn(MyData& p_md, ClientData& p_cd, const CommandBuffer& p_cb)
	{
		ats::String buf;
		if(p_cb.m_argc < 3)
		{
			ats_sprintf(&buf, "error: usage: %s pid=\"pid\" msg=\"msg\"", p_cb.m_argv[0]);
			send_cmd(p_cd.m_sockfd, MSG_NOSIGNAL, "%s\n\r", buf.c_str());
			return ats::String();
		}

		ats_logf(ATSLOG_INFO, "%s,%d: enter ucast command", __FILE__, __LINE__);

		ats::StringMap m;
		m.from_args(p_cb.m_argc - 1, p_cb.m_argv + 1);

		const ats::String& pid = m.get("pid");
		const ats::String& msg = m.get("msg");

		ats::String response = "FAIL";
		if(!pid.empty() && !msg.empty())
		{
			if(p_md.m_zb->ucast(pid, msg) == "ACK")
				response = "OK";
		}

		ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "ucast: %s\n", response.c_str());
		ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "\r");

		return ats::String();
	}
};

//Format: linkkey key=00000000000000000000000000000000 [password=password] [power=-7] [mainfunction=011C]
//Format: linkkey fobid=0000000000000000000 fobkey=00000000000000000000000000000000 [version=E0] // version is zigbee module config version, requested by fob, we can set "E0" as default.
class linkkeyCommand : public Command
{
public:
	virtual ats::String fn(MyData& p_md, ClientData& p_cd, const CommandBuffer& p_cb)
	{
		ats::String buf;
		if(p_cb.m_argc < 2)
		{
			ats_sprintf(&buf, "error: usage: %s key=\"key\"", p_cb.m_argv[0]);
			send_cmd(p_cd.m_sockfd, MSG_NOSIGNAL, "%s\n\r", buf.c_str());
			return ats::String();
		}

		ats::StringMap m;
		m.from_args(p_cb.m_argc - 1, p_cb.m_argv + 1);

		const ats::String& key = m.get("key");
		ats::String password = m.get("password");
		ats::String power = m.get("power");
		ats::String main = m.get("mainfunction");

		const ats::String& fobid = m.get("fobid");
		const ats::String& fobkey = m.get("fobkey");
		ats::String configversion = m.get("version");

		if((key.empty() || key.size() != 32) && (fobkey.empty() || fobkey.size() != 32 || fobid.empty()))
		{
			ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "linkkey: fail\n");
			return ats::String();
		}

		ats::String response;
		if(!key.empty())
		{

			if(password.empty()) password = "password";
			if(power.empty()) power = "-7";
			if(main.empty()) main = "011C";

			ats::String cmd = "ATS09=" + key + ":" + password;
			if((response = p_md.m_zb->sendMessage(cmd)) != "OK")
				goto exit;

			cmd = "ATS01=" + power;
			if((response = p_md.m_zb->sendMessage(cmd)) != "OK")
				goto exit;

			cmd = "ATS0A=" + main + ":" + password;
			if((response = p_md.m_zb->sendMessage(cmd)) != "OK")
				goto exit;

			p_md.m_zb->sendMessage("atz");

			sleep(2);
		}
		else if(!fobid.empty())
		{
			if(configversion.empty())
				configversion = "E0";

			struct WriteConfigRequest req;
			req.key = fobid;
			req.socketfd = p_cd.m_sockfd;

			struct warg r;
			strcpy(r.reg, "38\0");
			int size = configversion.size();
			memcpy(r.data, configversion.c_str(), size);
			r.data[size] = '\0';
			req.wvt.push_back(r);

			strcpy(r.reg, "40\0");
			memcpy(r.data, fobkey.c_str(), 32);
			r.data[32] = '\0';

			req.wvt.push_back(r);
			p_md.m_fobrequest_manager->FOBWriteConfigRequest(req, true);
		}

exit:
		ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "linkkey: %s\n", response.c_str());
		ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "\r");

		return ats::String();
	}

};

//Format: "at cmd=\"command\" [timeout=\"5\"]"
class atCommand : public Command
{
public:

	virtual ats::String fn(MyData& p_md, ClientData& p_cd, const CommandBuffer& p_cb)
	{
		ats::String buf;
		if(p_cb.m_argc < 2)
		{
			ats_sprintf(&buf, "error: usage: %s cmd=\"command\" [timeout=\"5\"]\n\r", p_cb.m_argv[0]);
			send_cmd(p_cd.m_sockfd, MSG_NOSIGNAL, "%s\n\r", buf.c_str());
			return ats::String();
		}

		ats::StringMap sm;
		sm.from_args(p_cb.m_argc - 1, p_cb.m_argv + 1);

		if(sm.has_key("cmd"))
		{
			const ats::String& cmd = sm.get("cmd");

			p_md.settestSocket(p_cd.m_sockfd);

			int timeout = 0;

			if(sm.has_key("timeout"))
					timeout = std::strtol(sm.get("timeout").c_str(), 0, 0);

			if(timeout)
				p_md.m_zb->sendMessage(cmd, timeout);
			else
				p_md.m_zb->sendMessage(cmd);

			send_cmd(p_cd.m_sockfd, MSG_NOSIGNAL, "\r");
		}

		return ats::String();
	}
};

class systemCommand : public Command
{
public:

	virtual ats::String fn(MyData& p_md, ClientData& p_cd, const CommandBuffer& p_cb)
	{
		ats::String buf;
		if(p_cb.m_argc < 2)
		{
			ats_sprintf(&buf, "error: usage: %s \"command\"\n\r", p_cb.m_argv[0]);
			send_cmd(p_cd.m_sockfd, MSG_NOSIGNAL, "%s\n\r", buf.c_str());
			return ats::String();
		}

		const ats::String& cmd = ats::String(p_cb.m_argv[1]);

		if(boost::iequals(cmd,"statusrequest"))
		{
			//FORMAT: system statusrequest <key> <FOBDisplayState1> <FOBDisplayState2>
			if(p_cb.m_argc < 5)
			{
				ats_sprintf(&buf, "error: usage: %s \"statusrequest key FOBDisplayState1 FOBDisplayState2\n", p_cb.m_argv[0]);
				send_cmd(p_cd.m_sockfd, MSG_NOSIGNAL, "%s\n\r", buf.c_str());
				return ats::String();
			}

			struct StatusControl sc;
			sc.key = ats::String(p_cb.m_argv[2]);
			sc.FOBDisplayState1 = std::strtol(ats::String(p_cb.m_argv[3]).c_str(),0 ,16);
			sc.FOBDisplayState2 = std::strtol(ats::String(p_cb.m_argv[4]).c_str(),0 ,16);
			sc.socketfd = p_cd.m_sockfd;
			p_md.m_fobrequest_manager->FOBStatusRequest(sc, true);

		}
		else if(boost::iequals(cmd,"uniqueidrequest"))
		{
			//FORMAT: system uniqueidrequest <key>
			if(p_cb.m_argc < 3)
			{
				ats_sprintf(&buf, "error: usage: %s \"command\"\n\r", p_cb.m_argv[0]);
				send_cmd(p_cd.m_sockfd, MSG_NOSIGNAL, "%s\n\r", buf.c_str());
				return ats::String();
			}

			p_md.m_fobrequest_manager->FOBUniqueIDRequest(ats::String(p_cb.m_argv[2]), p_cd.m_sockfd, true);
		}
		else if(boost::iequals(cmd,"readrequest"))
		{
			//FORMAT: system readrequest <key> <reg1> [ reg2 reg3]
			//system readrequest 000D6F00023E8347 1 2 3...42
			if(p_cb.m_argc < 4)
			{
				ats_sprintf(&buf, "error: usage: %s \"readrequest <key> <reg1> [ reg2 reg3]\"\n", p_cb.m_argv[0]);
				send_cmd(p_cd.m_sockfd, MSG_NOSIGNAL, "%s\n\r", buf.c_str());
				return ats::String();
			}

			struct ReadConfigRequest req;
			req.key = ats::String(p_cb.m_argv[2]);
			req.socketfd = p_cd.m_sockfd;
			for(int i = 3; i < p_cb.m_argc; ++i)
			{
				const int arg = std::strtol(ats::String(p_cb.m_argv[i]).c_str(), 0, 0);
				ats::String buf;
				ats_sprintf(&buf, "%.2x", arg);
				struct rarg r;
				r.data[0] = buf[0];
				r.data[1] = buf[1];
				req.rvt.push_back(r);
			}

			p_md.m_fobrequest_manager->FOBReadConfigRequest(req, true);
		}
		else if(boost::iequals(cmd,"writerequest"))
		{
			//FORMAT: system writerequest <key> <reg1> [ reg2 reg3]
			//system writerequest 000D6F00023E8347 0486 0500 0600 0700 0800 0900 0a77

			if(p_cb.m_argc < 4)
			{
				ats_sprintf(&buf, "error: usage: %s \"writerequest <key> <reg1> [ reg2 reg3]\"\n", p_cb.m_argv[0]);
				send_cmd(p_cd.m_sockfd, MSG_NOSIGNAL, "%s\n\r", buf.c_str());
				return ats::String();
			}

			struct WriteConfigRequest req;
			req.key = ats::String(p_cb.m_argv[2]);
			req.socketfd = p_cd.m_sockfd;

			for(int i = 3; i < p_cb.m_argc; ++i)
			{
				const ats::String& buf = p_cb.m_argv[i];

				if(buf.size() < 4)
					continue;

				struct warg r;
				r.reg[0] = buf[0];
				r.reg[1] = buf[1];
				r.reg[2] = '\0';

				int size = p_md.getFobRegSize(atoi(r.reg));

				if(buf.size() < (unsigned int)(size + 2) || size > 128)
					continue;

				memcpy(r.data, &(buf[2]), size);
				r.data[size] = '\0';

				req.wvt.push_back(r);
			}

			p_md.m_fobrequest_manager->FOBWriteConfigRequest(req, true);
		}
		return buf;
	}
};

typedef enum
{
	FOB_STATE_UNKNOWN             = -1,
	FOB_STATE_CHECKIN             = 0,
	FOB_STATE_OVERDUE_SAFETY_TIMER  = 1,
	FOB_STATE_ASSIST              = 2,
	FOB_STATE_SOS                 = 3,
	FOB_STATE_DISABLE             = 5,
	FOB_STATE_CHECKOUT            = 6,
	FOB_STATE_OVERDUE_HAZARD       = 7,
	FOB_STATE_OVERDUE_SHIFT_TIMER     = 8,
	FOB_STATE_OVERDUE_SAFETY_AND_SHIFT    = 9,
	FOB_STATE_MANDOWN             = 11,
}fobState;

typedef enum
{
	FOB_REQUEST_IDLE              = 0,
	FOB_REQUEST_CHECKIN           = 1,
	FOB_REQUEST_CHECKOUT          = 2,
	FOB_REQUEST_SOSCANCEL         = 4,
	FOB_REQUEST_SOS               = 8,
	FOB_REQUEST_MANDOWN           = 16
}fobRequest;

typedef enum
{
	FOB_TIMERREMIND_IDLE               = 0,
	FOB_TIMERREMIND_SAFETY             = 1,
	FOB_TIMERREMIND_HAZARD             = 2,
	FOB_TIMERREMIND_OFFTIME            = 4
}fobTimeRemind;

 //status waiting: checkin key pressed, waiting for response from workalone.
typedef enum
{
	FOB_TIMERREMIND_STATUS_INIT               = 0,
	FOB_TIMERREMIND_STATUS_WAITING            = 1
}TimeRemindStatus;

class fobContent
{

public:

	fobContent(MyData& p_md, ats::String eui, uint16_t node, uint16_t parent = 0):
		m_md(&p_md),
		m_running(false),
		m_mac(eui),
		m_nodeid(node),
		m_parent(parent),
		m_timer(NULL),
		m_statusReqcommID(0),
		m_uniIDReqcommID(0),
		m_writeconfigcommID(0),
		m_readconfigcommID(0),
		m_sosSignalTransmitLoop(false),
		m_state(FOB_STATE_UNKNOWN),
		m_request(FOB_REQUEST_IDLE),
		m_timeRemind(FOB_TIMERREMIND_IDLE),
		m_statusRequestTime(0),
		m_watStatusRequestTime(0),
		m_watStatusRequestHourlyTime(0),
		m_receiveMessageTime(0),
		m_offMonitorTime(0),
		m_hazardTime(0),
		m_safetyTime(0),
		m_offtimeRemindStatus(FOB_TIMERREMIND_STATUS_INIT),
		m_safetytimeRemindStatus(FOB_TIMERREMIND_STATUS_INIT),
		m_hazardtimeRemindStatus(FOB_TIMERREMIND_STATUS_INIT),
		m_hazardFlashingOnly(false),
		m_shifttimeFlashingOnly(false),
		m_timerRunning(false),
		needModifyForHazard(false),
		needModifyForShift(false)
	{
		m_mutex = new pthread_mutex_t;
		pthread_mutex_init(m_mutex, 0);

		m_cmdmutex = new pthread_mutex_t;
		pthread_mutex_init(m_cmdmutex, 0);

		m_cmdcond = new pthread_cond_t;
		pthread_cond_init(m_cmdcond, 0);

		ats_logf(ATSLOG_DEBUG, RED_ON "%s,%d: %s - added fobContent object" RESET_COLOR, __FILE__, __LINE__, m_mac.c_str());
		
		if(!m_md->m_msg_manager.get_client(m_mac))
		{
			ats_logf(ATSLOG_INFO, "%s,%d: %s creating queue", __FILE__, __LINE__, m_mac.c_str());
			m_md->m_msg_manager.add_client(m_mac , new message_queue());
			m_md->m_msg_manager.start_client(m_mac);
		}

		m_lasthbTimestamp = time(NULL);
		m_gps.valid = false;

	}

  void stop()
  {
    lock();
    m_running = false;
    unlock();

    if( g_overdueAllow)
    {
      ats::String cancelEvent = "FOBTIMEREVENT" + m_mac;
      m_md->post_cancel_event(cancelEvent);
			pthread_join(m_timer_thread, 0);
    }
  }

	~fobContent()
	{
    stop();

		if(m_timer)
			delete m_timer;

		pthread_mutex_destroy(m_mutex);
		delete m_mutex;

		pthread_mutex_destroy(m_cmdmutex);
		delete m_cmdmutex;

    ats_logf(ATSLOG_DEBUG, RED_ON "%s,%d: %s - removed fobContent object" RESET_COLOR, __FILE__, __LINE__, m_mac.c_str());
	}

	static void* timer_thread(void* p)
{
		fobContent& b = *((fobContent*)p);
		const ats::String& uwi = b.getEUI();
		MyData& m_md = *(b.m_md);

    EventListener listener(m_md);
    const ats::String eventName = "FOBTIMEREVENT"+uwi;

    while(b.m_running)
    {
      ats::TimerEvent* timer = new ats::TimerEvent(FOBTIMERPERIODIC);
      timer->set_default_event_name(eventName);
      m_md.add_event_listener(timer, __PRETTY_FUNCTION__, &listener);

      AppEventHandler e(listener.wait_event());

      if(e.m_event->m_reason.is_cancelled())
      {
        ats_logf(ATSLOG_DEBUG, "%s,%d: fobContent - fob %s timer cancelled", __FILE__, __LINE__, uwi.c_str());
        break;
      }
      else if(e.m_event == timer )
      {

        fobState currentState = b.getState();
        int request = b.getRequest();
        int remind = b.getTimeRemind();

				if(currentState != FOB_STATE_CHECKOUT && ( currentState != FOB_STATE_SOS ) && !(request&FOB_REQUEST_SOS) && !(request&FOB_REQUEST_MANDOWN))
				{
					time_t now = time(NULL);
					int diffOffTimer = b.offMonitorTime() - now;
					int diffsafetyTimer = b.safetyTime() - now;
					int diffhazardTimer = b.hazardTime() - now;

					if( b.getState() != FOB_STATE_OVERDUE_SHIFT_TIMER)
					{
						TimeRemindStatus s = b.fobTimeRemindStatus(FOB_TIMERREMIND_OFFTIME);
						if( diffOffTimer > 0 && diffOffTimer < g_timerExpireMinutes*60 )
						{
							if( s == FOB_TIMERREMIND_STATUS_INIT && !b.timerEnabled())
							{
								ats_logf(ATSLOG_DEBUG, RED_ON "%s,%d: %s Shift Timer overdue warning" RESET_COLOR, __FILE__, __LINE__, uwi.c_str());
								if(!(remind&FOB_TIMERREMIND_OFFTIME) || b.needModifyForShift)
								{
									m_md.fob_status_request(b.m_mac, FDS_LOWER_GREEN_ON, (!b.isShifttimeFlashingOnly())?FDS_ORANGE_FLASH_AND_VIBE:FDS_ORANGE_FLASH);
									b.needModifyForShift = false;
								}

								b.setTimeRemind(FOB_TIMERREMIND_OFFTIME);
							}
						}
						else
						{
							if( remind & FOB_TIMERREMIND_OFFTIME )
							{
								m_md.fob_status_request(uwi, FDS_LOWER_GREEN_ON, FDS_ALL_OFF);
							}
							b.setTimeRemind(FOB_TIMERREMIND_OFFTIME,false);
							b.fobTimeRemindStatus(FOB_TIMERREMIND_OFFTIME, FOB_TIMERREMIND_STATUS_INIT);
							if( b.getState() == FOB_STATE_CHECKIN )
							{
								b.setShifttimeFlashingFlag(false);
							}
						}
					}
					else
					{
						b.setTimeRemind(FOB_TIMERREMIND_OFFTIME,false);
						b.fobTimeRemindStatus(FOB_TIMERREMIND_OFFTIME, FOB_TIMERREMIND_STATUS_INIT);
					}

					if( b.getState() != FOB_STATE_OVERDUE_SAFETY_TIMER)
					{
						TimeRemindStatus s = b.fobTimeRemindStatus(FOB_TIMERREMIND_SAFETY);
						if( diffsafetyTimer> 0 && diffsafetyTimer < g_timerExpireMinutes*60 && !b.timerEnabled())
						{
							if( s == FOB_TIMERREMIND_STATUS_INIT )
							{
								ats_logf(ATSLOG_DEBUG, RED_ON "%s,%d: %s SAFETY Timer overdue warning" RESET_COLOR, __FILE__, __LINE__, uwi.c_str());
								if( !(remind&FOB_TIMERREMIND_SAFETY))
									b.sendOverduetoSLP();
								b.setTimeRemind(FOB_TIMERREMIND_SAFETY);
							}
						}
						else
						{
							if( remind & FOB_TIMERREMIND_SAFETY)
							{
								m_md.fob_status_request(uwi, FDS_LOWER_GREEN_ON, FDS_ALL_OFF);
							}

							b.setTimeRemind(FOB_TIMERREMIND_SAFETY,false);
							b.fobTimeRemindStatus(FOB_TIMERREMIND_SAFETY, FOB_TIMERREMIND_STATUS_INIT);
						}
					}
					else
					{
						b.setTimeRemind(FOB_TIMERREMIND_SAFETY,false);
						b.fobTimeRemindStatus(FOB_TIMERREMIND_SAFETY, FOB_TIMERREMIND_STATUS_INIT);
					}

					if( b.getState() != FOB_STATE_OVERDUE_HAZARD)
					{
						TimeRemindStatus s = b.fobTimeRemindStatus(FOB_TIMERREMIND_HAZARD);
						if( diffhazardTimer> 0 && diffhazardTimer< g_timerExpireMinutes*60 )
						{
							if( s == FOB_TIMERREMIND_STATUS_INIT && !b.timerEnabled())
							{
								ats_logf(ATSLOG_DEBUG, RED_ON "%s,%d: %s Hazard Timer overdue warning" RESET_COLOR, __FILE__, __LINE__, uwi.c_str());
								if(!(remind&FOB_TIMERREMIND_HAZARD) || b.needModifyForHazard)
								{
									m_md.fob_status_request(b.m_mac, FDS_LOWER_GREEN_ON, (!b.isHazardFlashingOnly())?FDS_ORANGE_FLASH_AND_VIBE:FDS_ORANGE_FLASH);
									b.needModifyForHazard = false;
								}

								b.setTimeRemind(FOB_TIMERREMIND_HAZARD);
							}
						}
						else
						{
							if( remind & FOB_TIMERREMIND_HAZARD)
							{
								m_md.fob_status_request(uwi, FDS_LOWER_GREEN_ON, FDS_ALL_OFF);
							}

							b.setTimeRemind(FOB_TIMERREMIND_HAZARD,false);
							b.fobTimeRemindStatus(FOB_TIMERREMIND_HAZARD, FOB_TIMERREMIND_STATUS_INIT);
							if( b.getState() == FOB_STATE_CHECKIN )
							{
								b.setHazardFlashingFlag(false);
							}
						}
					}
					else
					{
						b.setTimeRemind(FOB_TIMERREMIND_HAZARD,false);
						b.fobTimeRemindStatus(FOB_TIMERREMIND_HAZARD, FOB_TIMERREMIND_STATUS_INIT);
					}
				}
			}
		}

		return 0;
	}

	static void* fobsetupregister_thread(void* p);
	bool setupRegister();

	void messageHandle(const ats::String& msg);
	int handleButtonEvent(const ats::String&);
	int handleStatusReply(const ats::String&);
	int handleWriteConfigAck(const ats::String& msg);
	int handleReadConfigAck(const ats::String& msg);
	int handleUniqueIDReply(const ats::String& msg);

	void safetyTime(time_t value ){ m_safetyTime = value;}
	time_t safetyTime() const {return m_safetyTime; }
	void hazardTime(time_t value ){ m_hazardTime = value;}
	time_t hazardTime() const {return m_hazardTime; }
	void offMonitorTime(time_t value ){ m_offMonitorTime = value;}
	time_t offMonitorTime() const {return m_offMonitorTime; }
	void receiveTime(time_t value ){ m_receiveMessageTime = value;}
	time_t receiveTime() const {return m_receiveMessageTime; }
	time_t statusRequestSendTime() const { return m_statusRequestTime; }
	void statusRequestSendTime(time_t t) { lock(); m_statusRequestTime = t; unlock(); }
	time_t watStatusRequestTime() const { return m_watStatusRequestTime; }
	void watStatusRequestTime(time_t t) { lock(); m_watStatusRequestTime = t; unlock(); }
	time_t watStatusRequestHourlyTime() const { return m_watStatusRequestHourlyTime; }
	void watStatusRequestHourlyTime(time_t t)
	{
		lock();
		m_watStatusRequestHourlyTime = t;
		m_watStatusRequestTime = t;
		unlock();
	}

	MyData& getMyData() { return *m_md; }
	void lock() const
	{
		pthread_mutex_lock(m_mutex);
	}

	void unlock() const
	{
		pthread_mutex_unlock(m_mutex);
	}

	void cmdlock() const
	{
		pthread_mutex_lock(m_cmdmutex);
	}

	void cmdunlock() const
	{
		pthread_mutex_unlock(m_cmdmutex);
	}

	int waitCmdreply(int timeseconds = 0) const
	{
		int ret;
		cmdlock();
		if(!timeseconds)
		{
			ret = pthread_cond_wait(m_cmdcond, m_cmdmutex);
		}
		else
		{
			/* get the current time */ 
			struct timeval now; 
			gettimeofday(&now, NULL); 

			/* add the offset to get timeout value */ 
			struct timespec abstime;
			abstime.tv_nsec = now.tv_usec * 1000 + (timeseconds) * 1000000; 
			abstime.tv_sec = now.tv_sec + timeseconds;

			ret = pthread_cond_timedwait(m_cmdcond, m_cmdmutex, &abstime);
		}

		cmdunlock();

		return ret;
	}

	void sendCmdreply() const
	{
		cmdlock();
		pthread_cond_signal(m_cmdcond);
		cmdunlock();
	}

	ats::String getEUI()const{return m_mac;}
	uint16_t getNodeID()const{return m_nodeid;}
	uint16_t getParent()const{return m_parent;}

	void setup_timer(const int v = 2)
	{
		if(m_timer)
			return;

		m_timer = new myTimer(v);
		m_timer->setKey(m_mac);
	}

	void start_timer()
	{
		setup_timer(5);
		m_timer->stop_monitor();
		// AWARE360 FIXME: Rename "stop_monitor" to "start_timer".
		m_timer->enable_timer(true);
		m_timer->start_monitor();
		unlock();
	}

	void run()
	{
		lock();

		if(!m_running)
		{
			m_running = true;

      if( g_overdueAllow )
      {
        pthread_create(&m_timer_thread, 0, fobContent::timer_thread, this);
      }
		}

		unlock();
	}

	int getStatusReqID()
	{
		lock();
		m_statusReqcommID++;
		if(m_statusReqcommID > 99) m_statusReqcommID = 1;

		unlock();
		return m_statusReqcommID;
	}

	uint64_t getLastHbTimeStamp()const{return m_lasthbTimestamp;}
	void setLastHbTimeStamp(uint64_t ts){lock(); m_lasthbTimestamp = ts; unlock();}

	int getUniqueReqID()
	{
		lock();
		m_uniIDReqcommID++;
		if(m_uniIDReqcommID > 99) m_uniIDReqcommID = 1;

		unlock();
		return m_uniIDReqcommID;
	}

	int getWriteConfigID()
	{
		lock();
		m_writeconfigcommID++;
		if(m_writeconfigcommID > 99) m_writeconfigcommID = 1;

		unlock();
		return m_writeconfigcommID;
	}

	int getReadConfigID()
	{
		lock();
		m_readconfigcommID++;
		if(m_readconfigcommID > 99) m_readconfigcommID = 1;

		unlock();
		return m_readconfigcommID;
	}

	bool sosSignalTransmitLoop()
	{
		return m_sosSignalTransmitLoop;
	}

	void sosSignalTransmitLoop(bool v)
	{
		lock();
		m_sosSignalTransmitLoop = v;
		unlock();
	}
	void setState(fobState newState);


	void clearRequest()
	{
		m_request = FOB_REQUEST_IDLE;
	}

	void setRequest(fobRequest s, bool on = true)
	{
		ats_logf(ATSLOG_INFO, GREEN_ON "enter request %d %d %s" RESET_COLOR, s, m_request, (on)?"true":"false");

		if( on == true )
			m_request |= 0xFF&s;
		else
		{
			m_request &= 0xFF&(~s);
		}

		if( on && g_overdueAllow )
		{
//			resetRemind();
//			turnOffOverdueLight();
		}

		ats_logf(ATSLOG_INFO, GREEN_ON "leave request %d" RESET_COLOR, m_request);
	}

	int getTimeRemind() const
	{
		return m_timeRemind;
	}

	void resetRemind()
	{
		if(m_timeRemind)
		{
			m_md->fob_status_request(m_mac, FDS_LOWER_GREEN_ON, FDS_ALL_OFF);
		}
	}

	void clearTimeRemind()
	{
		m_timeRemind = FOB_TIMERREMIND_IDLE;
	}

	void setTimeRemind(fobTimeRemind s, bool on = true)
	{
		if( on == true )
		{
			m_timeRemind|= 0xFF&s;
		}
		else
		{
			m_timeRemind&= 0xFF&(~s);
		}
	}

	fobState getState() const
	{
		return m_state;
	}

	int getRequest() const
	{
		return m_request;
	}

	void fobTimeRemindStatus(fobTimeRemind r, TimeRemindStatus s)
	{
		switch(r)
		{
			case FOB_TIMERREMIND_OFFTIME:
				m_offtimeRemindStatus = s;
				break;
			case FOB_TIMERREMIND_SAFETY:
				m_safetytimeRemindStatus = s;
				break;
			case FOB_TIMERREMIND_HAZARD:
				m_hazardtimeRemindStatus = s;
				break;
			default:
				break;
		}
	}
	
	bool timerEnabled()const
	{
	  if( m_timer )
		return m_timer->is_enabled();
	  else return false;
	}

	TimeRemindStatus fobTimeRemindStatus(fobTimeRemind r)
	{
		TimeRemindStatus s = FOB_TIMERREMIND_STATUS_INIT;
		switch(r)
		{
			case FOB_TIMERREMIND_OFFTIME:
				s = m_offtimeRemindStatus;
				break;
			case FOB_TIMERREMIND_SAFETY:
				s = m_safetytimeRemindStatus;
				break;
			case FOB_TIMERREMIND_HAZARD:
				s = m_hazardtimeRemindStatus;
				break;
			default:
				break;
		}
		return s;
	}

	void fob_set_timer(const ats::String& pid, const int valut = FDS_GREEN_ON | FDS_SOLID);//default value is Top Green Light.
	void sendAck(const ats::String& sender, const ats::String& key, int seqNum, int ack);
	void sendWatStateRequest(bool highPri = false);
	void sendWatSosCancel();

	struct gps
	{
		int tm_year;
		int tm_mon;
		int tm_mday;
		int tm_hour;
		int tm_min;
		int tm_sec;

		double ddLat;
		double ddLon;
		short gps_quality;
		short num_svs;
		float hdop;
		bool valid;
		ats::String timeStampStr;

	};

	void getSystemTime();
	short DecodeGGA (const ats::StringList& strList);
	void updateTimeStamp();
	void sendOverduetoSLP();

protected:
	void getLocalGPSPosition(ats::String& gpsStr);
	void sendCheckinMessage(const ats::String& cmd, const ats::String& id, const ats::String& gpsStr);
	void turnOffOverdueLight();
	bool isHazardFlashingOnly() const;
	bool isShifttimeFlashingOnly() const;
	void setHazardFlashingFlag(bool b = false);
	void setShifttimeFlashingFlag(bool b = false);

private:
	MyData* m_md;
	pthread_mutex_t* m_mutex;
	pthread_mutex_t* m_cmdmutex;
	pthread_cond_t* m_cmdcond;
	bool m_running;

	ats::String m_mac;
	uint16_t m_nodeid;
	uint16_t m_parent;
	myTimer* m_timer;

	int m_statusReqcommID;
	int m_uniIDReqcommID;
	int m_writeconfigcommID;
	int m_readconfigcommID;
	uint64_t m_lasthbTimestamp;
	bool m_sosSignalTransmitLoop;
	fobState m_state;
	int m_request;
	int m_timeRemind;
	gps m_gps;
	NMEA_Client nmea_client;
	time_t m_statusRequestTime;
	time_t m_watStatusRequestTime;
	time_t m_watStatusRequestHourlyTime;
	time_t m_receiveMessageTime;
	time_t m_offMonitorTime;
	time_t m_hazardTime;
	time_t m_safetyTime;

	TimeRemindStatus m_offtimeRemindStatus;
	TimeRemindStatus m_safetytimeRemindStatus;
	TimeRemindStatus m_hazardtimeRemindStatus;

	pthread_t m_timer_thread;
	pthread_t m_setRegister_thread;

	responseManager<ats::String > m_readconfigManager;
	bool m_hazardFlashingOnly; //this flag only available when overdue allow and extension not allow
	bool m_shifttimeFlashingOnly; //this flag only available when shift timer overdue allow and extension not allow
	bool m_timerRunning;
	bool needModifyForHazard;
	bool needModifyForShift;
};

//Format: "calamp data=<binary> sender=<sender>"
class calampCommand : public Command
{
public:

	virtual ats::String fn(MyData& p_md, ClientData& p_cd, const CommandBuffer& p_cb)
	{
		ats::String buf;
		if(p_cb.m_argc < 3)
		{
			ats_sprintf(&buf, "error: usage: %s data=\"msg\" sender=\"sender\"", p_cb.m_argv[0]);
			send_cmd(p_cd.m_sockfd, MSG_NOSIGNAL, "%s\n\r", buf.c_str());
			return ats::String();
		}

		ats_logf(ATSLOG_INFO, "%s,%d: enter calamps command", __FILE__, __LINE__);

		ats::StringMap m;
		m.from_args(p_cb.m_argc - 1, p_cb.m_argv + 1);

		const ats::String& msg = m.get("data");
		//const ats::String& sender = m.get("sender");

		if(msg.empty())
		{
			ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "calamp command: fail\n");
			return ats::String();
		}

		ats_logf(ATSLOG_INFO, "%s,%d: enter calamps command %s", __FILE__, __LINE__, msg.c_str());
		calampDecode dec;
		if(!dec.decodePacket(msg))
		{
			calampDecode::userMsgData msgData;

			if(!dec.parseUserPacket(msgData))
			{
				const ats::String& id = dec.getMobileIDStr();
				ats_logf(ATSLOG_DEBUG, "%s,%d: MobileIDstr is %s", __FILE__, __LINE__, id.c_str());
				if(msgData.type == WAT_MESSAGE_STATE_RESPONSE && !id.empty())
				{
					//int state = msgData.mdata.state;

					fobContent *cc = p_md.m_fob_manager.get_node(id);
					if(!cc)
					{
						return ats::String();
					}

					fobEvent fEvent = watMessageEvent;
					if(fEvent != noEvent)
					{
						ats_logf(ATSLOG_DEBUG, "%s,%d: Message is %s", __FILE__, __LINE__, msgData.mdata.str);
						messageFrame m(fEvent, msgData.mdata.str, id);
						p_md.post_message(p_md.get_fobstate_key(), m);
					}

				}
			}
		}

		return ats::String();
	}

protected:

	bool sendSOSsignal(fobContent *cc, const ats::String& id)
	{
		if(cc->sosSignalTransmitLoop())
			return false;

		cc->sosSignalTransmitLoop(true);

		while(true)
		{
			cc->fob_set_timer(id);

			if(!(cc->waitCmdreply(5)))
			{
				cc->sosSignalTransmitLoop(false);
				return true;
			}

			ats_logf(ATSLOG_INFO, "%s,%d: keep sending admin command", __FILE__, __LINE__);
			sleep(1);
		}

		return false; 
	}

};


//Format: "iridium data=<binary> sender=<sender>"
class iridiumCommand : public Command
{
public:
	virtual ats::String fn(MyData& p_md, ClientData& p_cd, const CommandBuffer& p_cb)
	{
		ats::String buf;
		if(p_cb.m_argc < 3)
		{
			ats_sprintf(&buf, "error: usage: %s data=\"msg\" sender=\"sender\"", p_cb.m_argv[0]);
			send_cmd(p_cd.m_sockfd, MSG_NOSIGNAL, "%s\n\r", buf.c_str());
			return ats::String();
		}

		ats_logf(ATSLOG_DEBUG, "%s,%d: entering iridium command", __FILE__, __LINE__);

		ats::StringMap m;
		m.from_args(p_cb.m_argc - 1, p_cb.m_argv + 1);

		const ats::String& msg = m.get("data");
		//const ats::String& sender = m.get("sender");

		if(msg.empty())
		{
			ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "iridium command: fail\n");
			return ats::String();
		}

		iridiumDecode dec;
		ats::String msgData;
		const ats::String& imei = m.get("id");

		if(!dec.decodePacket(msg, msgData, imei))
		{
			ats::String id = dec.getMobileIDStr();
			
			if(!id.empty())
			{
				fobContent *cc = p_md.m_fob_manager.get_node(id);

				if(!cc)
				{
					ats_logf(ATSLOG_ERROR, "%s,%d: No node found for id %s", __FILE__, __LINE__, id.c_str());
					return ats::String();
				}

				fobEvent fEvent = watMessageEvent;
				if(fEvent != noEvent)
				{
					ats_logf(ATSLOG_DEBUG, "%s,%d: Sending message Frame (GOOD THING!)", __FILE__, __LINE__);
					messageFrame m(fEvent, msgData, id);
					p_md.post_message(p_md.get_fobstate_key(), m);
				}
			}
			else
				ats_logf(ATSLOG_ERROR, "%s,%d: No MobileID string in decoded packet", __FILE__, __LINE__);
			
		}

		return ats::String();
	}
protected:
	bool sendSOSsignal(fobContent *cc, const ats::String& id)
	{
		if(cc->sosSignalTransmitLoop())
			return false;

		cc->sosSignalTransmitLoop(true);

		while(true)
		{
			cc->fob_set_timer(id);

			if(!(cc->waitCmdreply(5)))
			{
				cc->sosSignalTransmitLoop(false);
				return true;
			}

			ats_logf(ATSLOG_INFO, "%s,%d: keep sending admin command", __FILE__, __LINE__);
			sleep(1);
		}

		return false; 
	}
};
