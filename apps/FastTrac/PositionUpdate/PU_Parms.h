#pragma once
#include <math.h>
#include <iostream>
#include <db-monitor.h>
#include <atslogger.h>
#include <ConfigDB.h>

extern ATSLogger g_log;

// PU_Parms - parms file for the PositionUpdate class
//
// The parameters are as follows:
//
//	 <PositionUpdateParms>
//		 <Time units=seconds>60</Time>
//		 <Distance units=metres>500</Distance>
//		 <Heading units=degrees>15</Heading>
//		 <Pinning>On</Pinning>
//		 <Stop units=m/s>0.3</Stop>
//		 <NotifyOnStop>Yes</NotifyOnStop>
//		 <StopTime units=seconds>120</StopTime>
//		 <MinSendTime units=seconds>2</MinSendTime>
//		 <IridiumReportTime units=minutes>60</IridiumReportTime>
//		 <RealTimeInterval units=seconds>60</RealTimeInterval>
//	 </PositionUpdateParms>

class PU_Parms
{
private:
	unsigned short m_Time;	// delta t between updates
	unsigned short m_Distance;	// delta distance between updates
	unsigned short m_Heading;	// Minimum cumulative change in heading (degrees) that must occur between updates
	bool	m_Pinning;	// if on we hold and use the first position after stopping
	short m_StopVel;	// kph - speeds below this indicate 'Stopped'

	bool m_bReportStopStart;	// true if PU is generating the stop/Start message (false if trip-monitor etc generates it)
	unsigned short m_StopTime;	// number of seconds not moving (< m_StopVel) to indicate Stopped

	bool m_bReportWhenStopped;	// true if reports should be sent even when stopped
	bool m_bIncreaseStoppedReportTime; // Aug 2017 - hardcoded to false
	
	unsigned short m_MinSendTime;	// minimum number of seconds between sends - Aug 2017 - hardcoded to 1
	unsigned short m_IridiumReportTime;	// number of minutes between Iridium sends (priority boosted to 9)
	unsigned short m_RealTimeInterval;	// minimum number of seconds between sends
		
public:
	unsigned short m_nPingCount;

public:
	PU_Parms()
	{
		Init();
		ReadParms();
	};

	void Init()
	{
		m_Time = 30;
		m_Distance =	0;
		m_Heading = 5;
		m_Pinning = true;
		m_StopVel = 4;
		m_nPingCount = 0;
		m_bReportStopStart = false;
		m_StopTime = 120;
		m_RealTimeInterval = 0;
		m_bReportWhenStopped =	false;	// true if reports should be sent even when stopped
		m_bIncreaseStoppedReportTime = false; // if true - double time between reports when stopped (2, 4, 8, 16...)
		m_MinSendTime = 1;	// seconds that must pass before another position will be reported.
		m_IridiumReportTime = 0;	// report hourly over Iridium
	}
	void DeltaTime(unsigned short t){m_Time = t; WriteParms();};
	unsigned short DeltaTime(){return m_Time;};

	void DeltaDist(unsigned short t){m_Distance = t; WriteParms();};
	unsigned short DeltaDist(){return m_Distance;};

	void DeltaHeading(unsigned short t){m_Heading = t; WriteParms();};
	unsigned short DeltaHeading(){return m_Heading;};

	void SetPinning(bool t){m_Pinning = t; WriteParms();};
	bool IsPinning(){return m_Pinning;};

	void ReportStopStart(bool t){m_bReportStopStart = t; WriteParms();};
	bool ReportStopStart(){return m_bReportStopStart;};

	void StopVel(short kph){m_StopVel = fabs(kph); WriteParms();};
	short StopVel(){return m_StopVel;};

	void StopTime(unsigned short t){m_StopTime = t; WriteParms();};
	unsigned short StopTime(){return m_StopTime;};

	void ReportWhenStopped(bool t){m_bReportWhenStopped = t; WriteParms();};
	bool ReportWhenStopped(){return m_bReportWhenStopped;};

	void IncreaseStoppedReportTime(bool t){m_bIncreaseStoppedReportTime = t; WriteParms();};
	bool IncreaseStoppedReportTime(){return m_bIncreaseStoppedReportTime;};

	void MinSendTime(unsigned short t){m_MinSendTime = t; WriteParms();};
	unsigned short MinSendTime(){return m_MinSendTime;};

	void IridiumReportTime(unsigned short t){m_IridiumReportTime = t; WriteParms();};
	unsigned short IridiumReportTime(){return m_IridiumReportTime;};
	void RealTimeInterval(unsigned short t){m_RealTimeInterval = t; WriteParms();};
	unsigned short RealTimeInterval(){return m_RealTimeInterval;};

	~PU_Parms(){};

	bool ReadParms() // read the parms file PositionUpdate.prm from the executable files directory
	{								// returns false if it fails to open the database
		db_monitor::ConfigDB db;

		const ats::String app_name("PositionUpdate");

		// must have valid values - get them all
		
		ats::String value;
		m_Time = db.GetInt(app_name, "Time", m_Time);
		m_Distance = db.GetInt(app_name, "Distance", m_Distance);
		m_Heading = db.GetInt(app_name, "Heading", m_Heading);

		value = db.GetValue(app_name, "Pinning", "On");
	
		if (value == "On")
			m_Pinning = true;
		else
			m_Pinning = false;
	

		m_StopVel = db.GetDouble(app_name, "StopVel", m_StopVel);
		m_StopTime = db.GetInt(app_name, "StopTime", m_StopTime);
		m_IridiumReportTime = db.GetInt(app_name, "IridiumReportTime", m_IridiumReportTime);
		m_RealTimeInterval = db.GetInt(app_name, "RealTimeInterval", m_RealTimeInterval);

		value = db.GetValue(app_name, "ReportStopStart", "Off");
	
		if (value == "On")
			m_bReportStopStart = true;
		else
			m_bReportStopStart = false;
		
		value = db.GetValue(app_name, "ReportWhenStopped", "On");
	
		if (value == "On")
			m_bReportWhenStopped = true;
		else
			m_bReportWhenStopped = false;
	
		return true;
	}

	bool WriteParms()
	{
		db_monitor::ConfigDB db;
		const ats::String app_name("PositionUpdate");
		ats::String value;
		db.set_config(app_name, "Time", ats::toStr(m_Time));
		db.set_config(app_name, "Distance", ats::toStr(m_Distance));
		db.set_config(app_name, "Heading", ats::toStr(m_Heading));
		db.set_config(app_name, "Pinning",	m_Pinning?"On":"Off");
		db.set_config(app_name, "StopVel", ats::toStr(m_StopVel));
		db.set_config(app_name, "StopTime", ats::toStr(m_StopTime));
		db.set_config(app_name, "ReportWhenStopped",	m_bReportWhenStopped?"On":"Off");
		db.set_config(app_name, "ReportStopStart",	m_bReportStopStart?"On":"Off");
		db.set_config(app_name, "IridiumReportTime",	ats::toStr(m_IridiumReportTime));
		db.set_config(app_name, "RealTimeInterval",	ats::toStr(m_RealTimeInterval));

		return true;
	};

	void Dump(string & str)
	{
		char buf[256];
		sprintf(buf, "PositionUpdate Parms::	Time:%d Dist:%d	Heading:%d Pinning:%s	StopVel (kph): %d\n", m_Time, m_Distance, m_Heading, (m_Pinning?"On":"Off"), m_StopVel);
		str = buf;
		sprintf(buf, "												StopTime:%d Increase Stopped Report Time:%s	ReportWhenStopped:%s NotifyOnStop:%s	\n", m_StopTime, (m_bIncreaseStoppedReportTime?"On":"Off"), (m_bReportWhenStopped?"On":"Off"),
														 (m_bReportStopStart?"On":"Off"));
		str += buf;
		sprintf(buf, "											 IridiumReportTime: %d\n", m_IridiumReportTime); 
		sprintf(buf, "											 RealTimeInterval: %d\n", m_RealTimeInterval); 
		str += buf;
	}
};

