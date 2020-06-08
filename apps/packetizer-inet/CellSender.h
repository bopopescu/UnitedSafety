#pragma once
//#include <QByteArray>

#include <semaphore.h>
#include <pthread.h>
#include <vector>
#include <time.h>

#include "ats-common.h"
#include "ats-string.h"
#include "RedStone_IPC.h"
#include "socket_interface.h"
#include "packetizer.h"
#include "curl/curl.h"
#include <INET_IPC.h>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "ConfigDB.h"

#include "packetizerSender.h"

struct curl_userdata
{
	char *ptr;
	size_t len;
};

class CellSender : public PacketizerSender
{
public:
	CellSender(MyData &);
	~CellSender();

	bool start();

	int sendSingleMessage( message_info* mi);
	ssize_t inetCreateGateway();
	ssize_t inetUpdateGateway();
	ssize_t inetUploadDebugLog();
	int inetGetAuthentication();
	int inetDownloadFirmware();
	int inetExchangeStatusInformation(); 
	int inetDownloadSettings(std::string strJson);// returns: 0=OK, -1=failure. //ISCP-345
	int inetUploadEquipment();
	int inetUploadError(message_info* p_mi);
	int inetDownloadEventSchedules();
	int waitforack();
	
	class authToken
	{
		public:
			ats::String access_token;
			ats::String refresh_token;
			ats::String token_type;
			int expires_in;
	};
private:
	MyData m_data;
	ServerData m_ServerData;
	static ats::String m_client_id;
	static ats::String m_client_secret;
	static ats::String m_username;
	static ats::String m_password;
	static sem_t m_ack_sem;
	static uint m_sequence_num;
	static int m_ack;
	static pthread_mutex_t m_mutex;
	REDSTONE_IPC m_RedStone;
	static ats::String m_defaultMobileId;
	bool m_bUpdateSettings;  // true if there is new settings expected (via inetExchangeStatusInformation)
	bool m_bFirstUpdate; // true if this is the first time we have read settings (at startup) to prevent reboot happening except at startup.
	bool m_bAccountChanged;  // true if a new account name was sent - causes a new sending of the 'start' command.
	bool m_bPS19Test;
	bool m_bIsSettingDownloaded; //ISCP-345

//	static bool m_bAlarmMonitor;  // Is alarm-monitor needing to be notified of incoming messages.
	// additions for NTPC like Primary/Secondary configurations
//	static bool m_bIsPrimary;// is this in a network as a primary unit?
//	static int m_BroadcastPort;

	authToken accessToken;

	// additions for NTPC like Primary/Secondary configurations
	//static void BroadcastToSecondary(QByteArray &data);

	bool packetizeMessageInfo(message_info* p_mi, std::vector<char>& data, std::map<int,int>&);
	//static void processMessage(QByteArray &data, ServerData &p_sd);
	//*************************************************************************
	// sendUdpData()
	// Sends UDP data to the socket defined by p_sd
	//   p_sd          - is the server data that contains the socket to send the data
	//   p_data        - is the data to be sent
	//   p_data_length - is the data length
	//
	// Returns the number of bytes sent, -1 if there was a system error or -2 if
	// there was an issue resolving the host
	//***************************************************************************
	//static ssize_t sendUdpData(ServerData& p_sd, const char * p_data, uint p_data_length);
	//static void* processUdpData(void *p);
	static void lock() {pthread_mutex_lock(&m_mutex);}
	static void unlock() {pthread_mutex_unlock(&m_mutex);}
	//static void sendAck(ServerData& p_sd, QByteArray& msg);

	// iNet functions - related to building JSON and sending/receiving data from iNet portal via curl.
	ats::String PostCurlRequest(const ats::String& url, const ats::String& params, const ats::StringList& headers, int& http_code, int &curlError);
	ats::String GetCurlRequest(const ats::String& url, const ats::StringList& headers, int& http_code, int &curlError);
	ats::String GetFileCurlRequest(const ats::String& url,const ats::StringList& headers, const std::string& fileName, int& http_code,int &curlError);
	
	std::string inetBuildGPSJSON();
	bool inetBuildAuthHeader(ats::String &header);
	void BlinkCellLED(bool);
	
	uint64_t StrToLL(std::string strVal);
	int BuildJSONHeader(ats::StringList &headers);
	int BuildParamHeader(ats::StringList &headers);
	void ParseExchangeStatusInformationResponse(std::string strJson);
	void ParseDownloadSettingsResponse(std::string strJson);
	void BuildEquipmentJSON(rapidjson::Document &doc);
	void BuildParamArray(rapidjson::Value &parmArray, rapidjson::Document::AllocatorType &allocator, std::string propName, std::string propVal, std::string propType);
	bool SaveDownloadSetting(const rapidjson::Value &document, db_monitor::ConfigDB &db, std::string docKey, std::string key);
	bool SaveDownloadSetting(const rapidjson::Value &document, db_monitor::ConfigDB &db, std::string docKey, std::string app, std::string key);
	void BuildComponentArray(rapidjson::Value &parmArray, rapidjson::Document::AllocatorType &allocator, std::string name, std::string value);
	time_t ConvertTimeZoneToUnix(std::string & ISO8601String);

	void RegisterCurlError(bool);
	void RegisterHTTPError(bool);
	void RegisterAuthError(bool);
	void RegisterSettingsParserError(bool);
	void RegisterSchParserError(bool);
	
void TestIridiumCreateGateway();  // test the various messages.
void TestIridiumUpdateGateway();

};
