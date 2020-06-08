#pragma once

#include "ConfigDB.h"
#include "stdio.h"


//  !!!!
// NOTE: inp3 is no longer attached to input 3 - it is actually Iridium wakeup and we currently do not use it.
//  !!!!


class WakeupMask
{
	ats::String m_strMask;
	db_monitor::ConfigDB m_ConfigDB;
	double m_AccelTriggerG;
	double m_CriticalVoltage;	
	double m_WakeupVoltage;
	double m_ShutdownVoltage;

	bool m_RTC, m_Accel, m_Inp1, m_Inp2, m_Inp3, m_Can, m_BattVolt, m_LowBatt;
public:
	
	WakeupMask()
	{
		m_AccelTriggerG = 3.1;
		m_CriticalVoltage = 11.9;
		m_WakeupVoltage = 13.2;
		m_ShutdownVoltage = 12.5;
		m_strMask = m_ConfigDB.GetValue("wakeup", "mask", "rtc,~accel,~inp1,~inp2,~inp3,~can,batt_volt,low_batt");
		m_AccelTriggerG = (double)m_ConfigDB.GetInt("wakeup", "AccelTriggerG", (int)(m_AccelTriggerG * 10)) / 10.0;
		m_CriticalVoltage = (double)m_ConfigDB.GetInt("wakeup", "CriticalVoltage", (int)(m_CriticalVoltage * 1000)) / 1000.0;
		m_WakeupVoltage = (double)m_ConfigDB.GetInt("wakeup", "WakeupVoltage", (int)(m_WakeupVoltage * 1000)) / 1000.0;
		m_ShutdownVoltage = (double)m_ConfigDB.GetInt("wakeup", "ShutdownVoltage", (int)(m_ShutdownVoltage * 1000)) / 1000.0;
		ParseMask();
		Save(); //ISCP-296: Keep Ignition On option always ON. 
	}

	
	//accessor functions
	double AccelTriggerG(){return m_AccelTriggerG;};
	void	AccelTriggerG(double newAccelTriggerG){m_AccelTriggerG = newAccelTriggerG;};

	double CriticalVoltage(){return m_CriticalVoltage;};
	void	CriticalVoltage(double newCriticalVoltage){m_CriticalVoltage = newCriticalVoltage;};

	double WakeupVoltage(){return m_WakeupVoltage;};
	void	WakeupVoltage(double newWakeupVoltage){m_WakeupVoltage = newWakeupVoltage;};

	double ShutdownVoltage(){return m_ShutdownVoltage;};
	void	ShutdownVoltage(double newShutdownVoltage){m_ShutdownVoltage = newShutdownVoltage;};
	
	bool RTC(){return m_RTC;}
	void RTC(bool isEnabled){m_RTC = isEnabled;}

	bool Accel(){return m_Accel;}
	void Accel(bool isEnabled){m_Accel = isEnabled;}

	bool Inp1(){return m_Inp1;}
	void Inp1(bool isEnabled){m_Inp1 = isEnabled;}

	bool Inp2(){return m_Inp2;}
	void Inp2(bool isEnabled){m_Inp2 = isEnabled;}

	bool Inp3(){return m_Inp3;}
	void Inp3(bool isEnabled){m_Inp3 = isEnabled;}

	bool Can(){return m_Can;}
	void Can(bool isEnabled){m_Can = isEnabled;}

	bool BattVolt(){return m_BattVolt;}
	void BattVolt(bool isEnabled){m_BattVolt = isEnabled;}

	bool LowBatt(){return m_LowBatt;}
	void LowBatt(bool isEnabled){m_LowBatt = isEnabled;}

	// Save function - rebuild the string and set all values that have changed
	void Save()
	{
		ats::String strMask;
		if (!m_RTC) strMask = "~";	strMask += "rtc,";
		if (!m_Accel) strMask += "~";	strMask += "accel,";
		strMask += "inp1,";//ISCP-296: Keep Ignition On option always ON. 
		//if (!m_Inp1) strMask += "~";	strMask += "inp1,";
		if (!m_Inp2) strMask += "~";	strMask += "inp2,";
		if (!m_Inp3) strMask += "~";	strMask += "inp3,";
		if (!m_Can) strMask += "~";	strMask += "can,";
		if (!m_BattVolt) strMask += "~";	strMask += "batt_volt,";
		if (!m_LowBatt) strMask += "~";	strMask += "low_batt";

		m_ConfigDB.set_config("wakeup", "mask", strMask);
		m_ConfigDB.Set("wakeup", "AccelTriggerG",  ats::toStr(m_AccelTriggerG * 10));
		m_ConfigDB.set_config("wakeup", "CriticalVoltage", ats::toStr(m_CriticalVoltage * 1000));
		m_ConfigDB.set_config("wakeup", "WakeupVoltage", ats::toStr(m_WakeupVoltage * 1000));
		m_ConfigDB.set_config("wakeup", "ShutdownVoltage", ats::toStr(m_ShutdownVoltage * 1000));
		m_strMask = strMask;
	}

	ats::String Dump()
	{
		ats::String strMask;
		if (!m_RTC) strMask = "~";	strMask += "rtc,";
		if (!m_Accel) strMask += "~";	strMask += "accel,";
		//if (!m_Inp1) strMask += "~";	strMask += "inp1,";
		strMask += "inp1,";//ISCP-296: Keep Ignition On option always ON. 
		if (!m_Inp2) strMask += "~";	strMask += "inp2,";
		if (!m_Inp3) strMask += "~";	strMask += "inp3,";
		if (!m_Can) strMask += "~";	strMask += "can,";
		if (!m_BattVolt) strMask += "~";	strMask += "batt_volt,";
		if (!m_LowBatt) strMask += "~";	strMask += "low_batt";
		strMask += "\n";
 		strMask += "db-config value - " + m_strMask + "\n";
		char strVal[32];

		sprintf(strVal, "AccelTriggerG=%d\n", (int)(m_AccelTriggerG * 10));
		strMask += strVal;

		sprintf(strVal, "CriticalVoltage=%d\n", (int)(m_CriticalVoltage * 1000));
		strMask += strVal;

		sprintf(strVal, "WakeupVoltage=%d\n", (int)(m_WakeupVoltage * 1000));
		strMask += strVal;
		
		sprintf(strVal, "ShutdownVoltage=%d\n", (int)(m_ShutdownVoltage * 1000));
		strMask += strVal;
		
		return strMask;
	}
private:
	//------------------------------------------------------------------------------------
	// ParseMask - sets the wakeup flag booleans based on the m_strMask values.
	void ParseMask()
	{
		m_RTC = m_Accel = m_Inp1 = m_Inp2 = m_Inp3 = m_Can = m_BattVolt = m_LowBatt = false;

		m_RTC = ((m_strMask.find("rtc") != string::npos) && (m_strMask.find("~rtc") == string::npos));
		m_Accel = (m_strMask.find("accel") != string::npos && m_strMask.find("~accel") == string::npos);
		m_Inp1 = true;//ISCP-296: Keep Ignition On option always ON. 
		//m_Inp1 = (m_strMask.find("inp1") != string::npos && m_strMask.find("~inp1") == string::npos);
		m_Inp2 = (m_strMask.find("inp2") != string::npos && m_strMask.find("~inp2") == string::npos);
		m_Inp3 = (m_strMask.find("inp3") != string::npos && m_strMask.find("~inp3") == string::npos);
		m_Can = (m_strMask.find("can") != string::npos && m_strMask.find("~can") == string::npos);
		m_BattVolt = (m_strMask.find("batt_volt") != string::npos && m_strMask.find("~batt_volt") == string::npos);
		m_LowBatt = (m_strMask.find("low_batt") != string::npos && m_strMask.find("~low_batt") == string::npos);
	}

};

