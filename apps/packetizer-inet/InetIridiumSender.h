#pragma once
#include <vector>
#include "ats-common.h"
#include <IridiumUtil.h>
#include "DeviceBase.h"
#include "FTData_Debug.h"
#include "FTData_Generic.h"
#include "FTData_J1939.h"
#include "FTData_Debug_ESN.h"
#include "FTData_Generic_ESN.h"
#include "FTData_J1939_ESN.h"
#include "FTData_UserData_ESN.h"
#include "NMEA_DATA.h"
#include "messagetypes.h"

class InetIridiumSender : public DeviceBase
{
public:
	InetIridiumSender();
	void SetData(ats::StringMap& sm);
	void packetize(std::vector< char >& data);
	int mid() {return m_mid;}
	void mid(int mid) {m_mid = mid;}

	bool start();
	bool sendSingleMessage( message_info* mi, IridiumUtil *pIridiumUtil);
	bool SendiNetCreateGateway(IridiumUtil *pIridiumUtil);
	bool SendiNetUpdateGateway(IridiumUtil *pIridiumUtil);
	void SendTestPacket(char * sdbBuf, int len);
	ssize_t sendUdpData(const char *p_data, uint p_data_length);
	static void* processUdpData(void* p);
	bool SendViaIridium(char * iridiumBuf, int len, IridiumUtil *pIridiumUtil);
	//298
	bool GetResponseIridium(IridiumUtil *pIridiumUtil);
	void HandleIridiumResponse(std::string receivedStr);
	int hexcharToInt(char hexChar);
	//298
	long m_testID;

private:
	TRAK_MESSAGE_TYPE m_MsgCode;
	NMEA_DATA m_nmea;
	ats::String m_esn;
	int m_esn_type; //based on moblie id type in CalAMPs message
	ats::String m_cell_imei;
	std::string m_IridiumIMEI;
	int m_mid;
	int m_SeatbeltOn;
	ats::String m_MsgUserData;
	ServerData m_sd;
	bool m_bPS19Test;
	vector <LensMAC> m_IridiumIdentifiedInstruments;  // list of instruments identified to service this session.
	
	
private:
	void DumpIPC(); // just dump the IPC instrument data to log file.
};

