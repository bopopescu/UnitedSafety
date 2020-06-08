#pragma once
#include <string>
#include "ConfigDB.h"

// INetConfig - holds all of the db-config parameters for INet.
// NOTE: you can not set any of these things here - it must be done through db-config set

class INetConfig
{
private:
	int m_PeriodicTime;
	int m_AlarmPeriodicTime;
	int m_MaxPeers;
	int m_MaxHops;
	int m_NetworkName;  // used in 0x61 Set Network Configuration
	int m_NetworkInterval;  // 1 or 2 but should be 2
	int m_PrimaryChannel;
	int m_SecondaryChannel;
	short m_FeatureBits;
	int m_ChannelMask;
	int m_EncryptionType;
	std::string m_RadioProtocolVersion;
//	std::string m_DeviceID;
	std::string m_SerialNum;
	std::string m_NetworkEncryption;  // used in 0x65 Set Network Encryption
	std::string m_SettingsVersion;

	int m_InetKeepAliveMinutes;  // send gateway command back to inet every x minutes
	int m_InstrumentKeepAliveMinutes;  // request Instrument settings every x minutes
	int m_InstrumentLostSeconds;  // instrument is 'lost' after this many Seconds.
	int m_SatKeepAliveMinutes; // send 'updateGateway every this many minutes under Iridium
	int m_CriticalIntervalMinutes;
	int m_SatelliteCriticalIntervalMinutes;
	int m_NonCriticalIntervalSeconds;
	int m_GPSUpdateMinutes;  // how often a GPS update should go out for GPS enabled instruments.
	
	int m_EnableTestMode;
	int m_LogLevel;
	std::string m_SiteName;  //used in registering the Gateway
	std::string m_iNetURL;
	std::string m_INetAccount;
	int m_INetIndex;
	int m_PingIntervalSeconds;
	int m_PowerLevel; // Derived from isc-mfg RegionOp where Region 0 = 17 and all other regions are used as the value (Aug 2019)
	// Upload bootloader script values from load_lens_script
	std::string m_UploadBin;  
	std::string m_UploadScript;
	std::string m_UploadCRC;
	std::string m_UploadVer;
	std::string m_FWVer;  // the Trulink Firmware version e.g. 3.0.0.12145
	
public:
	INetConfig()
	{
		Load();
	}
	void Load()
	{
		std::string isc_lens_key = "isc-lens";
		db_monitor::ConfigDB db;
		m_InetKeepAliveMinutes = db.GetInt(isc_lens_key, "INetKeepAliveMinutes", 5);  // note name change due to modbus.
		m_InstrumentKeepAliveMinutes = db.GetInt(isc_lens_key, "InstrumentKeepAliveMinutes", 5); //<ISCP-260>
		m_InstrumentLostSeconds = db.GetInt(isc_lens_key, "InstrumentLostSeconds", 90);
		m_CriticalIntervalMinutes = db.GetInt(isc_lens_key, "CriticalIntervalMinutes", 1);
		m_SatelliteCriticalIntervalMinutes = db.GetInt(isc_lens_key, "SatelliteCriticalIntervalMinutes", 2);
		m_NonCriticalIntervalSeconds = db.GetInt(isc_lens_key, "NonCriticalInterval", 5) * 60;
		m_GPSUpdateMinutes = db.GetInt(isc_lens_key, "GPSUpdateMinutes", 5);
		m_SatKeepAliveMinutes = db.GetInt(isc_lens_key, "SatUpdateMinutes", 15); //ISCP-357, changed to 15 minutes by defulat
		m_LogLevel = db.GetInt(isc_lens_key, "LogLevel", 0);
		m_SiteName = db.GetValue(isc_lens_key, "SiteName", "Undefined");
		m_MaxHops = db.GetInt(isc_lens_key, "MaxHops", 5);
		m_EnableTestMode = db.GetInt(isc_lens_key, "EnableTestMode", 0);
		m_INetIndex = db.GetInt(isc_lens_key, "INetIndex", 0);
		m_PingIntervalSeconds = db.GetInt(isc_lens_key, "PingIntervalSeconds", 900);  // default to 15 minutes.

		if (m_MaxHops > 10)  // max hops is 10
		{
			m_MaxHops = 10;
			db.Set(isc_lens_key, "MaxHops", ats::toStr(m_MaxHops) );
		}

		m_NetworkInterval = db.GetInt(isc_lens_key, "NetworkInterval", 2);
		if (m_NetworkInterval != 1 && m_NetworkInterval != 2)  //must be one or two
		{
			m_NetworkInterval = 2;
			db.Set(isc_lens_key, "NetworkInterval", ats::toStr(m_NetworkInterval) );
		}

		m_MaxPeers = db.GetInt(isc_lens_key, "MaxPeers", 24);
		if (m_MaxPeers < 1 || m_MaxPeers > 29)  //must between 1 and 29 inclusive
		{
			m_MaxPeers = 24;
			db.Set(isc_lens_key, "MaxPeers", ats::toStr(m_MaxPeers) );
		}
		
		// ISCP-230 - need to use the Inet URL index and the ISC-MODBUS supplied urls.
		m_INetIndex = db.GetInt(isc_lens_key, "INetIndex", 0);
		if (m_INetIndex < 0 || m_INetIndex > 3)
			m_INetIndex = 0;
		char key[128];
		if (m_INetIndex == 0)
			sprintf(key, "INetURL");
		else
			sprintf(key, "INet%dURL", m_INetIndex);

		m_iNetURL = db.GetValue(isc_lens_key, key, "https://inetnowstg.indsci.com");
		// end ISCP-230

		m_PeriodicTime = db.GetInt(isc_lens_key, "DefaultPeriodicSeconds", 60);
		m_AlarmPeriodicTime = db.GetInt(isc_lens_key, "AlarmPeriodicSeconds", 1);
//		m_DeviceID = db.GetValue(isc_lens_key, "DeviceID", "0123456789");
		m_SerialNum = db.GetValue(isc_lens_key, "SerialNumber", "PRINCEDEV1-001");
		m_NetworkEncryption = db.GetValue(isc_lens_key, "NetworkEncryption", "Aware360DevKey17");
		m_EncryptionType = db.GetInt(isc_lens_key, "Encryption", 2);
		m_NetworkName = db.GetInt(isc_lens_key, "NetworkName", 1);
		m_INetAccount = db.GetValue(isc_lens_key, "INetAccount", "ISCPDA");
		m_SettingsVersion = db.GetValue(isc_lens_key, "SettingsVersion", "2019-01-01T00:00:00.000-0400");

		m_PrimaryChannel = db.GetInt(isc_lens_key, "PrimaryChannel", 4);
		m_SecondaryChannel = db.GetInt(isc_lens_key, "SecondaryChannel", 9);
		m_FeatureBits = (short)db.GetInt(isc_lens_key, "FeatureBits", 0x001f);
		m_ChannelMask = db.GetInt(isc_lens_key, "ChannelMask", 0x04c4);

		m_UploadBin = db.GetValue(isc_lens_key, "scriptBin", "");  
		m_UploadScript = db.GetValue(isc_lens_key, "scriptName", "");
		m_UploadCRC = db.GetValue(isc_lens_key, "scriptCRC", "");
		m_UploadVer = db.GetValue(isc_lens_key, "scriptVersion", "");

		m_FWVer = db.GetValue(isc_lens_key, "FWVer", "3.0.0");  
		m_PowerLevel = db.GetInt("isc-mfg","RegionOp", 0);
		if (m_PowerLevel == 0 || m_PowerLevel > 17)
			m_PowerLevel = 17;
	};

	int InetKeepAliveMinutes() const {return m_InetKeepAliveMinutes;};  // send gateway command back to inet every x minutes
	int InstrumentKeepAliveMinutes() const {return m_InstrumentKeepAliveMinutes;};  // request Instrument settings every x minutes
	int InstrumentLostSeconds() const {return m_InstrumentLostSeconds;};  // Instrument is lost to the network after x minutes of no messages
	int GPSUpdateMinutes() const {return m_GPSUpdateMinutes;};  // send gateway command back to inet every x minutes
	int LogLevel() const {return m_LogLevel;};
	int INetIndex() const {return m_INetIndex;};
	std::string SiteName() const {return m_SiteName;};
	std::string iNetURL() const { return m_iNetURL;};
	int PeriodicTime()		{return m_PeriodicTime;};
	int AlarmPeriodicTime()	{return m_AlarmPeriodicTime;};
	int MaxPeers() {return m_MaxPeers;};
	int MaxHops() {return m_MaxHops;};
	int NetworkName() {return m_NetworkName;};
	int NetworkInterval() {return m_NetworkInterval;};
	int PowerLevel() {return m_PowerLevel;}
	std::string INetAccount() const {return m_INetAccount;}
//	std::string DeviceID()	{return m_DeviceID;};
	std::string SerialNum()	{return m_SerialNum;};
	std::string NetworkEncryption()	{return m_NetworkEncryption;}
	std::string SettingsVersion()	{return m_SettingsVersion;}
	int PrimaryChannel(){ return m_PrimaryChannel;}
	int SecondaryChannel(){ return m_SecondaryChannel;}
	short FeatureBits(){ return m_FeatureBits;}
	int ChannelMask(){ return m_ChannelMask;}
	int EncryptionType(){ return m_EncryptionType;}
	int EnableTestMode(){ return m_EnableTestMode;}
	int CriticalIntervalMinutes(){ return m_CriticalIntervalMinutes;}
	int SatelliteCriticalIntervalMinutes(){ return m_SatelliteCriticalIntervalMinutes;}
	int NonCriticalIntervalSeconds(){ return m_NonCriticalIntervalSeconds;}
	int PingIntervalSeconds(){ return m_PingIntervalSeconds;}
	int SatKeepAliveMinutes(){ return m_SatKeepAliveMinutes;}

	// Upload bootloader script values from load_lens_script
	std::string UploadBin()		{ 	std::string fname = "/mnt/nvram/lens/" + UploadScript() + "_" + UploadCRC() + "_" + UploadVer() + ".bin"; return fname;}
	std::string UploadScript(){ return m_UploadScript;}
	std::string UploadCRC()		{ return m_UploadCRC;}
	std::string UploadVer()		{ return m_UploadVer;}

	std::string FWVer()		{ return m_FWVer;}
	
	void SettingsVersion(std::string val){m_SettingsVersion = val;}

	std::string SettingsVersionNoTZ()
	{
		std::string tmp = m_SettingsVersion; 
		if (tmp.find('+') != std::string::npos)
			tmp.resize(tmp.find('+') ); 
		std::size_t idx =  tmp.find_last_of('-');
		if (idx != std::string::npos  && idx > 11)
			tmp.resize(idx); 
		return tmp;
	}
};
