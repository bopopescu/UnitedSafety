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


typedef std::map <const ats::String, Command*> CommandMap;
typedef std::pair <const ats::String, Command*> CommandPair;

typedef ats::ClientMessageQueue<struct messageFrame> message_queue;


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
		m_bInSOS = false;
ats_logf(ATSLOG(0), "MyData CTOR");
	}

	~MyData()
	{
		if (m_fd != -1)
			close(m_fd);

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


	CommandMap m_cmd;
	ats::StringMap m_config;

	ats::ClientMessageManager<struct messageFrame > m_msg_manager;

	int m_fd;

	db_monitor::ConfigDB m_db;
	void post_message(const ClientData* p_client, const messageFrame& msg);
	bool get_message(const ClientData* p_client, messageFrame& p_msg);

	void start_server();
	void fob_status_request(const ats::String& key, int s1, int s2)const{};

	void settestSocket(int socket)
	{
		lock();
		m_testSocket = socket;
		unlock();
	}

	int gettestSocket()const {return m_testSocket;}

	bool m_bInSOS;
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

		ats_logf(ATSLOG(0), RED_ON "%s,%d: %s enter fobcontent constructor" RESET_COLOR, __FILE__, __LINE__, m_mac.c_str());
		if(!m_md->m_msg_manager.get_client(m_mac))
		{
			ats_logf(ATSLOG(0), "%s,%d: %s create queue", __FILE__, __LINE__, m_mac.c_str());
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

    ats_logf(ATSLOG(0), RED_ON "%s,%d: Remove fob content object" RESET_COLOR, __FILE__, __LINE__ );
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
        ats_logf(ATSLOG(0), "%s,%d: fob %s timer cancelled", __FILE__, __LINE__, uwi.c_str());
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
								ats_logf(ATSLOG(0), RED_ON "%s,%d: %s Shift Timer overdue warning" RESET_COLOR, __FILE__, __LINE__, uwi.c_str());
								if(!(remind&FOB_TIMERREMIND_OFFTIME) || b.needModifyForShift)
								{
									m_md.fob_status_request(b.m_mac, 0x80, (!b.isShifttimeFlashingOnly())?0x74:0x34);
									b.needModifyForShift = false;
								}

								b.setTimeRemind(FOB_TIMERREMIND_OFFTIME);
							}
						}
						else
						{
							if( remind & FOB_TIMERREMIND_OFFTIME )
							{
								m_md.fob_status_request(uwi, 0x80, 0xF0);
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
								ats_logf(ATSLOG(0), RED_ON "%s,%d: %s SAFETY Timer overdue warning" RESET_COLOR, __FILE__, __LINE__, uwi.c_str());
								if( !(remind&FOB_TIMERREMIND_SAFETY))
									b.sendOverduetoSLP();
								b.setTimeRemind(FOB_TIMERREMIND_SAFETY);
							}
						}
						else
						{
							if( remind & FOB_TIMERREMIND_SAFETY)
							{
								m_md.fob_status_request(uwi, 0x80, 0xF0);
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
								ats_logf(ATSLOG(0), RED_ON "%s,%d: %s Hazard Timer overdue warning" RESET_COLOR, __FILE__, __LINE__, uwi.c_str());
								if(!(remind&FOB_TIMERREMIND_HAZARD) || b.needModifyForHazard)
								{
									m_md.fob_status_request(b.m_mac, 0x80, (!b.isHazardFlashingOnly())?0x74:0x34);
									b.needModifyForHazard = false;
								}

								b.setTimeRemind(FOB_TIMERREMIND_HAZARD);
							}
						}
						else
						{
							if( remind & FOB_TIMERREMIND_HAZARD)
							{
								m_md.fob_status_request(uwi, 0x80, 0xF0);
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

	void setState(fobState s)
	{
		fobState currentState = getState();
		int request = getRequest();

		if (g_overdueAllow && (currentState == FOB_STATE_OVERDUE_SAFETY_AND_SHIFT || currentState == FOB_STATE_OVERDUE_SAFETY_TIMER || currentState == FOB_STATE_OVERDUE_SHIFT_TIMER || currentState == FOB_STATE_OVERDUE_HAZARD ))
		{
			if( s == FOB_STATE_CHECKIN || s == FOB_STATE_CHECKOUT ||  s == FOB_STATE_SOS || s == FOB_STATE_MANDOWN || s == FOB_STATE_ASSIST )
			{
				m_md->fob_status_request(m_mac, 0x80, 0xF0);
			}
		}

		if((currentState == FOB_STATE_SOS || currentState == FOB_STATE_MANDOWN) && ( s != FOB_STATE_SOS ) && ( s!=FOB_STATE_MANDOWN ))
		{
			if( request & FOB_REQUEST_SOSCANCEL)
			{
				fob_set_timer(m_mac, 0x18);//solid red for 5 seconds
				setRequest(FOB_REQUEST_SOSCANCEL, false);
			}
			else //control by manual, no sos cancel button pressed.
				m_md->fob_status_request(m_mac, 0x80, 0xF0);
		}

		ats_logf(ATSLOG(0), GREEN_ON "enter state %d request %d" RESET_COLOR, s, request);
		switch(s)
		{
			case FOB_STATE_CHECKIN:
				{
					if(request&FOB_REQUEST_CHECKIN)
					{
						fob_set_timer(m_mac, 0x28);//solid green for 5 seconds
						setRequest(FOB_REQUEST_CHECKIN, false);
					}
				}
				break;
			case FOB_STATE_OVERDUE_SAFETY_TIMER:
				{
					if(g_overdueAllow)
						sendOverduetoSLP();
				}
				break;
			case FOB_STATE_ASSIST:
				break;
			case FOB_STATE_SOS:
				{
					m_md->fob_status_request(m_mac, 0x82, 0x18);
					setRequest(FOB_REQUEST_SOS, false);
				}
				break;
			case FOB_STATE_DISABLE:
				break;
			case FOB_STATE_CHECKOUT:
				{
					if( request&FOB_REQUEST_CHECKOUT)
					{
						fob_set_timer(m_mac, 0x38);//solid red+green for 5 seconds
						setRequest(FOB_REQUEST_CHECKOUT, false);
					}
					setHazardFlashingFlag(false);
					setShifttimeFlashingFlag(false);
				}
				break;
			case FOB_STATE_OVERDUE_HAZARD:
			case FOB_STATE_OVERDUE_SHIFT_TIMER:
			case FOB_STATE_OVERDUE_SAFETY_AND_SHIFT:
				{
					if(g_overdueAllow)
					{
						if( s == FOB_STATE_OVERDUE_HAZARD )
						{
							m_md->fob_status_request(m_mac, 0x80, (!isHazardFlashingOnly())?0x74:0x34);
							setHazardFlashingFlag(true);
						}
						else
						{
							m_md->fob_status_request(m_mac, 0x80, (!isShifttimeFlashingOnly())?0x74:0x34);
							setShifttimeFlashingFlag(true);
						}
					}
					else
					{
						if(request&FOB_REQUEST_CHECKIN)
						{
							fob_set_timer(m_mac, 0x28);//solid green for 5 seconds
							setRequest(FOB_REQUEST_CHECKIN, false);
						}
						else if (request&FOB_REQUEST_CHECKOUT )
						{
							fob_set_timer(m_mac, 0x38);//solid red+green for 5 seconds
							setRequest(FOB_REQUEST_CHECKOUT, false);
						}
					}
				}
				break;
			default:
				break;
		}
		
		lock();
		m_state = s;
		unlock();

		int remind = getTimeRemind();
		if( remind )
		{
			TimeRemindStatus s = fobTimeRemindStatus(FOB_TIMERREMIND_HAZARD);
			if( s == FOB_TIMERREMIND_STATUS_WAITING )
			{
				fobTimeRemindStatus(FOB_TIMERREMIND_HAZARD, FOB_TIMERREMIND_STATUS_INIT);
			}

			s = fobTimeRemindStatus(FOB_TIMERREMIND_SAFETY);
			if( s == FOB_TIMERREMIND_STATUS_WAITING )
			{
				fobTimeRemindStatus(FOB_TIMERREMIND_SAFETY, FOB_TIMERREMIND_STATUS_INIT);
			}
			s = fobTimeRemindStatus(FOB_TIMERREMIND_OFFTIME);
			if( s == FOB_TIMERREMIND_STATUS_WAITING )
			{
				fobTimeRemindStatus(FOB_TIMERREMIND_OFFTIME, FOB_TIMERREMIND_STATUS_INIT);
			}
			if( remind & FOB_TIMERREMIND_OFFTIME )
			{
				if(g_overdueAllow && !g_AllowTimerExtensions )
				{
					needModifyForShift = true;
					setShifttimeFlashingFlag(true);
				}
			}

			if( remind & FOB_TIMERREMIND_HAZARD)
			{
				if(g_overdueAllow && !g_AllowTimerExtensions )
				{
					needModifyForHazard = true;
					setHazardFlashingFlag(true);
				}
			}
		}
		ats_logf(ATSLOG(0), GREEN_ON "leave state %d request %d" RESET_COLOR, getState(), getRequest());
	}

	void clearRequest()
	{
		ats_logf(ATSLOG(0), GREEN_ON "enter clear request " RESET_COLOR);
		m_request = FOB_REQUEST_IDLE;
		ats_logf(ATSLOG(0), GREEN_ON "leave clear request " RESET_COLOR);
	}

	void setRequest(fobRequest s, bool on = true)
	{
		ats_logf(ATSLOG(0), GREEN_ON "enter request %d %d %s" RESET_COLOR, s, m_request, (on)?"true":"false");

		if( on == true )
			m_request |= 0xFF&s;
		else
		{
			m_request &= 0xFF&(~s);
		}

		if( on && g_overdueAllow )
		{
			resetRemind();
			turnOffOverdueLight();
		}

		ats_logf(ATSLOG(0), GREEN_ON "leave request %d" RESET_COLOR, m_request);
	}

	int getTimeRemind() const
	{
		return m_timeRemind;
	}

	void resetRemind()
	{
		if(m_timeRemind)
		{
			m_md->fob_status_request(m_mac, 0x80, 0xF0);
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

	void fob_set_timer(const ats::String& pid, const int valut = 0x28);//default value is Top Green Light.
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

		ats::StringMap m;
		m.from_args(p_cb.m_argc - 1, p_cb.m_argv + 1);

		const ats::String& msg = m.get("data");
		//const ats::String& sender = m.get("sender");

		if(msg.empty())
		{
			ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "calamp command: fail\n");
			return ats::String();
		}
		ats_logf(ATSLOG(0), "%s,%d: processing calamp command  - %s", __FILE__, __LINE__, msg.c_str());

		calampDecode dec;

		if(!dec.decodePacket(msg))
		{
			calampDecode::userMsgData msgData;
			int state;
			dec.GetStateFromUserMsgData(msgData, state);

			if (state == -1)
			{
			  return ats::String();
			}

			if(state == FOB_STATE_SOS)
			{
				if (!p_md.m_bInSOS)
				{
					p_md.m_bInSOS = true;
					system("sh /etc/redstone/SOS.sh"); // run SOS script
					ats_logf(ATSLOG(0), "Running SOS script");
				}
			}
			else
			{
				if (p_md.m_bInSOS)
				{
					p_md.m_bInSOS = false;
					ats_logf(ATSLOG(0), "Ending SOS script");
					system("sh /etc/redstone/endSOS.sh"); 					// terminate SOS script
				}
			}
		}
		return ats::String();
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

		ats_logf(ATSLOG(5), "%s,%d: enter iridium command", __FILE__, __LINE__);

		ats::StringMap m;
		m.from_args(p_cb.m_argc - 1, p_cb.m_argv + 1);

		const ats::String& msg = m.get("data");

		if(msg.empty())
		{
			ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "iridium command: fail\n");
			return ats::String();
		}

		iridiumDecode dec;
		ats::String msgData;

		ats::String id;
		int ret = dec.decodeWATStateResponseMsg(msg, msgData, id);

		if (ret == FOB_STATE_SOS)
		{
			if (!p_md.m_bInSOS)
			{
				ats_logf(ATSLOG(0), "iridium- Starting SOS script");
				p_md.m_bInSOS = true;
				system("sh /etc/redstone/SOS.sh"); // run SOS script
			}
		}
		else
		{
			if (p_md.m_bInSOS)
			{
				p_md.m_bInSOS = false;
				ats_logf(ATSLOG(0), "iridium- Ending SOS script");
				system("sh /etc/redstone/endSOS.sh"); 					// terminate SOS script
			}
		}

		return ats::String();
	}
};
