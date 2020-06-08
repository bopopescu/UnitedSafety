#pragma once

#include <stdio.h>

#include <deque>
#include <map>
#include <string>

#include "AFS_Timer.h"

class MyData;

extern ATSLogger g_log;

//These two defines are for handling bad values for Engine Hours
#define ENGINE_HOURS_PGN 61184
#define ENGINE_HOURS_SPN 520192

#define CUMMINS_FAULTCODE_BITMAPSIZE 16

enum priorityType { IPPriorityType, IridiumPriorityType };
enum EXSTATE {ES_NORMAL, ES_WAIT, ES_SENT, ES_RECOVERY_WAIT};  // Exceedence state - either in valid range, waiting for duration to expire, or message was sent.

//ATS FIXME: all variables need m_ prefix.
class SignalData
{
public:
	int slaveAddr;
	int reg;
	unsigned int value;
	int size;
	unsigned int nadata;
	float multiplier;
	unsigned int offset;
	int spn;
	int pgn;
	float hrange;
	float lrange;
	float HiExLimit;
	int HiExDuration;  // seconds before 'Hi Ex' message is sent
	int HiExRecovery;  // seconds before 'recovery from Hi Ex' message is sent
	float LowExLimit;
	int LowExDuration;  // seconds before 'Low Ex' message is sent
	int LowExRecovery;  // seconds before 'recovery from Low Ex' message is sent
	bool hrepeat;
	bool lrepeat;
	int type;
	ats::String typeName;
	ats::String desc;
	ats::String unit;
	int faultCode[CUMMINS_FAULTCODE_BITMAPSIZE];
	priorityType priType;
	
public:
	SignalData(): slaveAddr(0), reg(0), value(0), size(1), nadata(0),
	multiplier(1.0), offset(0), spn(0), pgn(0), hrange(0.0), lrange(0.0), HiExLimit(0.0),HiExDuration(0) ,HiExRecovery(10) ,
	LowExLimit(0.0), LowExDuration(0), LowExRecovery(10), type(0), faultCode(), priType(IPPriorityType) {}

	void clean()
	{
		slaveAddr = 0;
		reg = 0;
		value = 0;
		size = 1;
		nadata = 0;
		multiplier = 1.0;
		offset = 0;
		spn = 0;
		pgn = 0;
		hrange = 0.0;
		lrange = 0.0;
		HiExLimit= 0.0;
		LowExLimit= 0.0;
		HiExDuration = 0 ;
		LowExDuration=  0;
		type = 0;
		typeName.clear();
		desc.clear();
		unit.clear();
		
		for( int i = 0; i < CUMMINS_FAULTCODE_BITMAPSIZE; i++)
		{
			faultCode[i]=0;
		}
	}
};

struct periodicRecord
{
	float m_min;
	float m_max;
	int   m_timestamp;
};

typedef enum
{
	EVENT_TYPE_HIGH = 0,
	EVENT_TYPE_LOW = 1,
	EVENT_TYPE_PERIODIC = 2,
	EVENT_TYPE_HIGH_COUNT = 3,
	EVENT_TYPE_LOW_COUNT = 4,
	EVENT_TYPE_HIGH_RECOVERY = 5,
	EVENT_TYPE_LOW_RECOVERY = 6
}EVENT_TYPE;

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

struct periodicdata
{
	int pgn;
	int spn;
	float maxvalue;
	float minvalue;
	float currentvalue;
	float averagevalue;
};


class J1939Parameter
{
public:

	int pgn;
	int spn;

	J1939Parameter () { pgn = 0; spn = 0; }
	J1939Parameter(int p_pgn,int p_spn) { pgn = p_pgn; spn = p_spn; }
};


class SignalMonitor
{
private:
	bool m_touch;
	unsigned int m_prevalue;
	AFS_Timer m_HiExTimer;
	AFS_Timer m_HiExRecoveryTimer;
	AFS_Timer m_LowExTimer;
	AFS_Timer m_LowExRecoveryTimer;
	bool m_hrepeat;
	bool m_lrepeat;
	SignalData m_signalData;
	pthread_mutex_t *m_mutex;
	unsigned int m_minvalue;
	unsigned int m_maxvalue;
	unsigned int m_currentvalue;
	double m_averageValue;
	unsigned int m_averageCount;
	bool m_initdata;
	uint8_t m_signalStatus;
	EXSTATE m_HiExState;
	EXSTATE m_LowExState;
	int  m_HiExCount;
	int  m_LowExCount;
	float m_maxExceedanceValue;
	float m_minExceedanceValue;
	MyData* m_data;
	int m_slaveAddr;
	std::deque<unsigned int> m_engine_hrs_dataset;  //used to store
	
public:
	SignalMonitor(MyData &p_data, const SignalData& sset, int slaveAddr)
	{
		m_data = &p_data;
		m_touch = false;
		m_signalData = sset;
		m_prevalue = sset.nadata;
		m_hrepeat = false;
		m_lrepeat = false;
		m_currentvalue = m_minvalue = m_maxvalue = m_averageValue = sset.nadata;
		m_averageCount = 1;
		m_initdata = false;
		m_HiExState = ES_NORMAL;
		m_LowExState = ES_NORMAL;
		m_HiExCount   = 0;
		m_LowExCount    = 0;
		m_maxExceedanceValue   = 0.0;
		m_minExceedanceValue   = 0.0;

		m_HiExTimer.SetTime();
		m_LowExTimer.SetTime();
		m_signalStatus = 0;
		m_mutex = new pthread_mutex_t;
		pthread_mutex_init(m_mutex, 0);

		m_slaveAddr = slaveAddr;
	}

	SignalMonitor(const SignalMonitor& that)
	{
		m_data = that.m_data;
		m_touch = false;
		m_signalData = that.m_signalData;
		m_prevalue = that.m_signalData.nadata;
		m_hrepeat = false;
		m_lrepeat = false;
		m_signalStatus = 0;
		m_currentvalue = m_minvalue = m_maxvalue = m_averageValue = that.m_signalData.nadata;
		m_averageCount = 1;
		m_initdata = false;

		m_HiExState = ES_NORMAL;
		m_LowExState = ES_NORMAL;
		m_HiExCount   = 0;
		m_LowExCount    = 0;
		m_maxExceedanceValue   = 0.0;
		m_minExceedanceValue   = 0.0;

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
	void resetPeriodicData();
	void setlimit(int llimit, int tlimit){m_signalData.LowExLimit = llimit; m_signalData.HiExLimit = tlimit;}
	int getpgn(){return m_signalData.pgn;}
	int getspn(){return m_signalData.spn;}
	int getInitData(){return m_signalData.nadata;}
	bool IsInExceedence();  // is the current value outside the exceedence range.
	bool scan();
	void sendMessage(float value, int type);

	SignalData& get() {return m_signalData;}

private:
	//function to throw out bad engine hour data
	bool processEngineHoursValue(unsigned int val);

};

bool compareTimeStamp( const periodicRecord& pl, const periodicRecord& pr );
