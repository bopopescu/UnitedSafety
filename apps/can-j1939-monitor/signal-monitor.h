#pragma once

#include <stdio.h>

#include <map>
#include <string>

#include "AFS_Timer.h"

class MyData;

extern ATSLogger g_log;
enum exceedcondition { exceedcondition_startup, exceedcondition_operation }; 

class SignalData
{
public:
		SignalData(): value(0), 
		size(0), offset(0.0), scale(1.0), 
		spnnum(0), pgnnum(0), hlimit(0.0), 
		LowExLimit(0.0), 
		type(0x3F), startposition(0), span(0), 
		periodic(false),operation(false),hrange(0.0), 
		lrange(0.0), condition(exceedcondition_startup),
		desc(ats::String()), 
		unit(ats::String()){}

		void clean()
		{
			spnnum = 0;
			pgnnum = 0;
			value = 0;
			size = 1;
			hlimit= 0.0;
			LowExLimit= 0.0;
			type = 0x3F;
			startposition = 0;
			span = 0;
			periodic = false;
			operation = false;
			hrange = 0.0;
			lrange = 0.0;
			condition = exceedcondition_startup;
			offset = 0.0;
			desc.clear();
			unit.clear();
		}

  int value;
  int size;
  float offset;
  float scale;
  int spnnum;
  int pgnnum;
  float hlimit;
  float LowExLimit;
  int type;
  uint8_t startposition;
	uint8_t span;
	bool periodic;
	bool operation;
	float hrange;
	float lrange;
	exceedcondition condition;
  ats::String desc;
  ats::String unit;
};

typedef enum
{
	EVENT_TYPE_HIGH = 0,
	EVENT_TYPE_LOW = 1,
	EVENT_TYPE_HIGH_COUNT = 3,
	EVENT_TYPE_LOW_COUNT = 4,
	EVENT_TYPE_HIGH_RECOVERY = 5,
	EVENT_TYPE_LOW_RECOVERY = 6
}EVENT_TYPE;

struct periodicdata
{
	int pgn;
	int spn;
	float maxvalue;
	float minvalue;
	float currentvalue;
};

enum signalStatus
{
	stayMiddleArea,
	stayHighArea,
	stayLowArea,
	enterHighArea,
	enterLowArea,
	leaveHighArea,
	leaveLowArea
};

class SignalMonitor
{
public:
	SignalMonitor(MyData &p_data, SignalData& sset)
	{
		m_data = &p_data;
		m_touch = false;
		m_set = sset;
		m_prevalue = 0;
		m_minvalue = m_maxvalue = m_currentvalue = 0;
		m_initdata = false;
		m_HiExState = false;
		m_LowExState = false;
		m_HiExCount   = 0;
		m_LowExCount    = 0;
		m_maxExceedanceValue   = 0.0;
		m_minExceedanceValue   = 0.0;
		m_signalStatus = 0;
		m_mutex = new pthread_mutex_t;
		pthread_mutex_init(m_mutex, 0);

	}

	SignalMonitor& operator =(const SignalMonitor& that);
	SignalMonitor(const SignalMonitor& that)
	{
		m_data = that.m_data;
		m_touch = false;
		m_set = that.m_set;
		m_prevalue = 0;
		m_minvalue = m_maxvalue = m_currentvalue = 0;
		m_initdata = false;
		m_HiExState = false;
		m_LowExState = false;
		m_HiExCount   = 0;
		m_LowExCount    = 0;
		m_maxExceedanceValue   = 0.0;
		m_minExceedanceValue   = 0.0;
		m_signalStatus = 0;
		m_mutex = new pthread_mutex_t;
		pthread_mutex_init(m_mutex, 0);
	}

	virtual~ SignalMonitor()
	{
		pthread_mutex_destroy(m_mutex);
		delete m_mutex;
	}

	void lock() const
	{
		pthread_mutex_lock(m_mutex);
	}

	void unlock() const
	{
		pthread_mutex_unlock(m_mutex);
	}

	MyData& my_data() const
	{
		return *m_data;
	}

	void set(unsigned int value);
	void getperiodicdata(periodicdata& data);
	void setlimit(float llimit, float hlimit){m_set.LowExLimit = llimit; m_set.hlimit = hlimit;}

	SignalData& getset() { return m_set; }

	int getspnnum(){return m_set.spnnum;}
	bool scan();
	void sendMessage(float value, int type);
	bool IsInExceedence()const;

private:
	bool            m_touch;
	unsigned int    m_prevalue;
	AFS_Timer       m_HiExTimer;
	AFS_Timer       m_LowExTimer;
	SignalData   m_set;
	pthread_mutex_t *m_mutex;
	unsigned int    m_minvalue;
	unsigned int    m_maxvalue;
	unsigned int    m_currentvalue;
	bool            m_initdata;
	uint8_t         m_signalStatus;
	bool            m_HiExState;
	bool            m_LowExState;
	int             m_HiExCount;
	int             m_LowExCount;
	float           m_maxExceedanceValue;
	float           m_minExceedanceValue;
	MyData*         m_data;
};
