/*-------------------------------------------------------------------------------------------
	FobContent class.
-------------------------------------------------------------------------------------------*/


#pragma once
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/ioctl.h>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <math.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <boost/circular_buffer.hpp>

#include "timer-event.h"
#include "socket_interface.h"
#include "event_listener.h"
#include "state_machine.h"
#include "state-machine-data.h"
#include "command_line_parser.h"
#include "ClientMessageManager.h"
#include "ConfigDB.h"
#include "ats-common.h"
#include "messageFormatter.h"
#include "../zigbee-monitor/zigbee-base.h"

class MyData;
extern int g_defaultperiodictime;
extern int g_alarmperiodictime;

const ats::String unitLookup[] = {"unknown", "PPM", "%VOL", "%LEL"};

typedef enum
{
	FOB_STATE_DOCKED         = 0x00,
	FOB_STATE_NORMAL         = 0x01,
	FOB_STATE_CALIBRATING    = 0x02,
	FOB_STATE_WARMINGUP      = 0x03,
	FOB_STATE_NA             = 0x05, 
	FOB_STATE_ZEROING        = 0x06,
	FOB_STATE_SYSTEM_ALARM   = 0x08,
	FOB_STATE_LOCAL_ALARM    = 0x09,
	FOB_STATE_ERROR          = 0x0A,
	FOB_STATE_SHUTDOWN       = 0x1A,
	FOB_STATE_LEAVING_GROUP  = 0x1B,
	FOB_STATE_CHARGING       = 0x1D,
	FOB_STATE_BUMP_TESTING   = 0x1F,
	FOB_STATE_SPARE          = 0xFF,
}fobState;

typedef enum
{
	ALARM_DETAIL_SENSOR_ALARM          = 0x01,
	ALARM_DETAIL_PANIC                 = 0x02,
	ALARM_DETAIL_FALL                  = 0x04,
	ALARM_DETAIL_MANDOWN              = 0x08,
	ALARM_DETAIL_PUMPFAULT             = 0x10,
}alarmDetail;

typedef enum
{
	SENSORSTATUS_NORMAL               = 0X00,
	SENSORSTATUS_LOW_ALARM            = 0X01,
	SENSORSTATUS_HIGH_ALARM           = 0X02,
	SENSORSTATUS_NEGATIVE_OVER_RANGE  = 0X03,
	SENSORSTATUS_OVER_RANGE           = 0X04,
	SENSORSTATUS_CALLIBRATION_FAULT   = 0X05,
	SENSORSTATUS_ZERO_FAULT           = 0X06,
	SENSORSTATUS_USER_DISABLED        = 0X08,
	SENSORSTATUS_BUMP_FAULT           = 0X09,
	SENSORSTATUS_CALLIBRATION_OVERDUE = 0X0B,
	SENSORSTATUS_DATA_FAIL            = 0X0D,
	SENSORSTATUS_TWA_ALARM            = 0X0F,
	SENSORSTATUS_STEL_ALARM           = 0X10,
}sensorStatus;


class AlarmEvent: public AppEvent
{
public:
	COMMON_EVENT_DECLARATION(AlarmEvent)
};


class FobContent : public StateMachineData
{
public:
	struct sensor  // Page 29 - this is the structure of the sensor date in the Identify Sensor Configuration message (up to 8 sensors in the message)
	{
		unsigned char position;
		unsigned char sType; // sensor type
		unsigned char sgType; // sensor Gas Type
		unsigned char measureUnit;
		unsigned char displayUnit;
		unsigned char measureDecimalPlace;
		unsigned char displayDecimalPlace;
		unsigned short highAlarm;
		unsigned short lowAlarm;
		unsigned short twaAlarm;
		unsigned short stelAlarm;
	};
private:
	MyData* m_md;
	pthread_mutex_t* m_mutex;
	ats::String m_mac;
	boost::circular_buffer<string> m_status_cb;
	ats::String m_sn;
	ats::String m_usrName;
	ats::String m_site;
	int sensorCount;
	std::map<int, sensor> sensorMap;
	char m_eventSequence;
	unsigned char twaTimeBase;
	pthread_t m_toinet_thread;
	fobState m_state;
	bool m_running;
public:
	FobContent(MyData& p_md, ats::String eui, ats::String sn = ats::String()):
		m_md(&p_md),
		m_mac(eui),
		m_sn(sn),
		m_usrName(ats::String()),
		m_site(ats::String()),
		m_eventSequence(0),
		m_state(FOB_STATE_SPARE),
		m_running(false)
	{
		m_mutex = new pthread_mutex_t;
		pthread_mutex_init(m_mutex, 0);
		m_status_cb.set_capacity(64);
	}

	virtual ~FobContent()
	{
		stop();
		pthread_mutex_destroy(m_mutex);
		delete m_mutex;
		ats_logf(ATSLOG_DEBUG, RED_ON "%s,%d: %s - removed FobContent object" RESET_COLOR, __FILE__, __LINE__, m_mac.c_str());
	}

	static void* toinet_thread(void* p);

	void stop()
	{
		lock();
		m_running = false;
		unlock();

		ats::String cancelEvent = "FOBTIMEREVENT";
		post_cancel_event(cancelEvent);
		pthread_join(m_toinet_thread, 0);
	}


	void run()
	{
		lock();

		if(!m_running)
		{
			m_running = true;
			pthread_create(&m_toinet_thread, 0, FobContent::toinet_thread, this);
		}

		unlock();
	}


	void push_status_data(const string & str)
	{
		lock();
		m_status_cb.push_front(str);
		unlock();
	}

	void pop_status_data(string& str)
	{
		if(!m_status_cb.size()) 
			return;
			
		lock();
		str = m_status_cb.front();
		m_status_cb.clear();
		unlock();
	}
	ats::String getMac() const { return m_mac; }
	ats::String getSn() const { return m_sn; }
	ats::String getUsrName() const { return m_usrName; }
	ats::String getSite() const { return m_site; }
	void setSn(const ats::String& sn)
	{
		ats_logf(ATSLOG_NONE, "sn is %s", sn.c_str());
		m_sn = sn;
	}
	void setUsrName(const ats::String& name)
	{
		ats_logf(ATSLOG_NONE, "Usr name is %s", name.c_str());
		m_usrName = name;
	}
	void setSite(const ats::String& site)
	{
		ats_logf(ATSLOG_NONE, "Site is %s", site.c_str());
		m_site = site;
	}
	char getEventSequence()
	{
		char ret = m_eventSequence++;
		return ret;
	}
	void add_sensor(const sensor& sor, unsigned char position)
	{
		if (position <= 8 && sensorMap.find(position) == sensorMap.end())
		{
			sensorMap[position] = sor;
		}
	}

	fobState getState() const
	{
		return m_state;
	}

	void setState(fobState newState)
	{
		m_state = newState;
	}

protected:
	void lock() const
	{
		pthread_mutex_lock(m_mutex);
	}

	void unlock() const
	{
		pthread_mutex_unlock(m_mutex);
	}

};


