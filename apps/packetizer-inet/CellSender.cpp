#include <errno.h>
#include <stdlib.h>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <time.h>
#include <netdb.h>
#include <string>
#include <sys/types.h>
#include <arpa/inet.h>
#include <boost/array.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/format.hpp>

#include "ats-common.h"
#include "atslogger.h"
#include "RedStone_IPC.h"
#include "socket_interface.h"
#include "messagetypes.h"
#include "utility.h"
#include "CellSender.h"
#include "NMEA_Client.h"
#include "ConfigDB.h"
#include <INetConfig.h>
#include <colordef.h>  // ascii color definitions for output

#include "ServerSocket.h"
#include "SocketException.h"
#include "SatData90_IDGateway.h"
#include "SatData96_UpdateGateway.h"
long testID;
using namespace rapidjson;
ats::String CellSender::m_client_id;
ats::String CellSender::m_client_secret;
ats::String CellSender::m_username;
ats::String CellSender::m_password;

sem_t CellSender::m_ack_sem;
pthread_mutex_t CellSender::m_mutex;
uint CellSender::m_sequence_num;
int CellSender::m_ack;
extern INetConfig *g_pConfig;
extern INET_IPC g_INetIPC;

//-------------------------------------------------------------------------------------------------
void init_curl_userdata(struct curl_userdata *s)
{
	s->len = 0;
	s->ptr = (char *)malloc(s->len + 1);
	if (s->ptr == NULL)
	{
		fprintf(stderr, "malloc() fail\n");
		exit(EXIT_FAILURE);
	}
	s->ptr[0] = '\0';
}

//-------------------------------------------------------------------------------------------------
size_t curl_callback(void* ptr, size_t size, size_t nmemb, struct curl_userdata *s)
{
	size_t new_len = s->len + size * nmemb;
	s->ptr = (char *)realloc(s->ptr, new_len + 1);
	if (s->ptr == NULL)
	{
		fprintf(stderr, "remalloc() fail\n");
		exit(EXIT_FAILURE);
	}
	memcpy(s->ptr + s->len, ptr, size*nmemb);
	s->ptr[new_len] = '\0';
	s->len = new_len;
	return size * nmemb;
}
//-------------------------------------------------------------------------------------------------
CellSender::CellSender(MyData& pData) : PacketizerSender(pData)
{
printf("%s:%d\n", __FILE__, __LINE__);
	sem_init(&m_ack_sem, 0 , 0);
	pthread_mutex_init(&m_mutex, NULL);
	m_sequence_num = 1;
	g_INetIPC.RunState(NORMAL);
	db_monitor::ConfigDB db;
	const ats::String app_name("packetizer-inet");
	m_client_id = db.GetValue(app_name, "client_id", "0d751b9e-7e4e-41a7-8029-e0e00a4302a9");
	m_client_secret = db.GetValue(app_name, "client_secret", "472967e3-a969-41c8-90a1-395a3fbc2a5f");
	m_bPS19Test = db.GetBool("isc-lens", "PS19Test", false);

	// ISCP-230 - use the iNetIndex to get password and username
	if (g_pConfig->INetIndex() == 0)
	{
		m_username= db.GetValue("isc-lens", "INetUsername", "inetdevapi");//ISC-331
		m_password = db.GetValue("isc-lens", "INetPassword", "iNetDev!23");//ISC-331
	}
	else
	{
		char key[128];
		sprintf(key, "INet%dUsername", g_pConfig->INetIndex() );
		m_username= db.GetValue("isc-lens", key, "inetdevapi");//ISC-331
		sprintf(key, "INet%dPassword", g_pConfig->INetIndex() );
		m_password = db.GetValue("isc-lens", key, "iNetDev!23");//ISC-331
	}
	// End of ISCP-230 - use the iNetIndex to get password and username
	m_bUpdateSettings = false;
	m_bFirstUpdate = true;
	m_ack = 0;
	m_bAccountChanged = false;
	m_bIsSettingDownloaded = false; //ISCP-345
}

//-------------------------------------------------------------------------------------------------
CellSender::~CellSender()
{}

//-------------------------------------------------------------------------------------------------
bool CellSender::start()
{
printf("%s:%d\n", __FILE__, __LINE__);
	int tries = 0;
	while (	inetGetAuthentication() != 0 && tries++ < 5)
	{
		sleep(5);  // we are awaiting internet connectivity or an auth code.  No use going anywhere until we have an auth code.
	}

	if (tries >= 5) // never made a connection.
	{
		
		ats_logf( ATSLOG_DEBUG, RED_ON "%s,%d:TGX Auth Failed \n" RESET_COLOR,__FILE__, __LINE__);
		
		return false;
	}
	ats_logf(ATSLOG_DEBUG, GREEN_ON "%s,%d: 'start' got authentication!  tries = %d" RESET_COLOR, __FILE__, __LINE__, tries);
	
	inetExchangeStatusInformation();
	
	if (m_bAccountChanged)
	{
		ats_logf(ATSLOG_DEBUG, RED_ON "New Account name received - restarting packetizer-inet" RESET_COLOR);
		exit(1);  // causes a restart of the packetizer-inet. Needed to restart with new settings. ISCP-220.
	}
	m_bFirstUpdate = false;  // guarantees that the system wont reboot after first read of the settings.

	inetCreateGateway();
	inetUploadEquipment();
	inetDownloadEventSchedules();
//	inetUploadDebugLog();
printf("%s:%d\n", __FILE__, __LINE__);
	return true;
}

void CellSender::BlinkCellLED(bool isOn)
{
	if (m_RedStone.pppState() == APS_CONNECTED)//ISCP-295
    {
        
		if(isOn == true)
		{
			send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink kick name=cell led=cell  script=\"1,200000;0,200000;1,200000;0,200000;1,200000;0,200000\"\r"); //ISCP-306
			return;
		}
	}
	send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink kick name=cell led=cell  script=\"1,1000000;\"\r");//ISCP-306
}

//-------------------------------------------------------------------------------------------------
// PostCurlRequest - returns the response as a string.
ats::String CellSender::PostCurlRequest(const ats::String& url, const ats::String& params, const ats::StringList& headers, int& http_code, int &curlError)
{
	g_INetIPC.SendingToINet(true);
	ats::String retVal = ats::String();
	CURL* curlHandle = curl_easy_init();
	curlError = 0;

	if (curlHandle)
	{
		struct curl_slist* list = NULL;
		struct curl_userdata storage;
		init_curl_userdata(&storage);

		ats_logf(ATSLOG_DEBUG, "%s,%d: URL: %s   	", __FILE__, __LINE__, url.c_str());
		curl_easy_setopt(curlHandle, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curlHandle, CURLOPT_CAINFO, "/mnt/nvram/rom/cacert.pem");
		curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, curl_callback);
		curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &storage);
		curl_easy_setopt(curlHandle, CURLOPT_TIMEOUT, 20L);

		ats::StringList::const_iterator it = headers.begin();
		for (; it != headers.end(); ++it)
		{
			list = curl_slist_append(list, (*it).c_str());
			//ats_logf(ATSLOG_DEBUG, "%s,%d: header %s   	", __FILE__, __LINE__, (*it).c_str());
		}

		curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, list);
		curl_easy_setopt(curlHandle, CURLOPT_VERBOSE, 0L); //set 1L to turn on verbose
		if (params.size() != 0)
		{
			curl_easy_setopt(curlHandle, CURLOPT_POST, 1);
			curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, params.c_str());
//			ats_logf(ATSLOG_DEBUG, "%s,%d: params %s", __FILE__, __LINE__, params.c_str());
		}

		if ((curlError = curl_easy_perform(curlHandle)) != 0)
		{
			ats_logf(ATSLOG_ERROR, "%s,%d: " RED_ON "Fail to execute curl_easy_perform %d" RESET_COLOR, __FILE__, __LINE__, curlError);
			curl_easy_cleanup(curlHandle);
			http_code = 0;
			free(storage.ptr);
			g_INetIPC.SendingToINet(false);
			return ats::String();
		}

		curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &http_code);
		retVal = ats::String(storage.ptr, storage.len);
		free(storage.ptr);

		curl_easy_cleanup(curlHandle);
		curl_slist_free_all(list);
	}
	g_INetIPC.SendingToINet(false);
	return retVal;
}


//-------------------------------------------------------------------------------------------------
// GetCurlRequest - returns the response as a string.
ats::String CellSender::GetCurlRequest(const ats::String& url, const ats::StringList& headers, int& http_code, int &curlError)
{
	g_INetIPC.SendingToINet(true);
	ats::String retVal = ats::String();
	CURL* curlHandle = curl_easy_init();
	curlError = 0;

	if (curlHandle)
	{
		struct curl_slist* list = NULL;
		struct curl_userdata storage;
		init_curl_userdata(&storage);

		curl_easy_setopt(curlHandle, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curlHandle, CURLOPT_CAINFO, "/mnt/nvram/rom/cacert.pem");
		curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, curl_callback);
		curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &storage);

		ats::StringList::const_iterator it = headers.begin();
		for (; it != headers.end(); ++it)
		{
			list = curl_slist_append(list, (*it).c_str());
			ats_logf(ATSLOG_DEBUG, "%s,%d: header %s   	", __FILE__, __LINE__, (*it).c_str());
		}

		curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, list);
		curl_easy_setopt(curlHandle, CURLOPT_VERBOSE, 0L); //set 1L to turn on verbose

		if ((curlError = curl_easy_perform(curlHandle)) != 0)
		{
			ats_logf(ATSLOG_ERROR, "%s,%d: " RED_ON "Fail to execute curl_easy_perform %d" RESET_COLOR, __FILE__, __LINE__, curlError);
			curl_easy_cleanup(curlHandle);
			free(storage.ptr);
		}
		else
		{
			curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &http_code);
			retVal = ats::String(storage.ptr, storage.len);
			ats_logf(ATSLOG_INFO, "%s,%d: curl response: %s", __FILE__, __LINE__, retVal.c_str() );
			free(storage.ptr);

			curl_easy_cleanup(curlHandle);
			curl_slist_free_all(list);
		}
	}

	g_INetIPC.SendingToINet(false);  // change LED back to solid
	return retVal;
}

//-------------------------------------------------------------------------------------------------
// GetFileCurlRequest - returns the response as a string.
ats::String CellSender::GetFileCurlRequest
(
	const ats::String& url,
	const ats::StringList& headers,
	const ats::String& fileName,
	int& http_code,
	int &curlError
)
{
	ats::String retVal = ats::String();
	CURL* curlHandle = curl_easy_init();
	curlError = 0;
	FILE *fp;
	fp = fopen(fileName.c_str(), "w");

	if (curlHandle && fp)
	{
		struct curl_slist* list = NULL;
		struct curl_userdata storage;
		init_curl_userdata(&storage);

		curl_easy_setopt(curlHandle, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curlHandle, CURLOPT_CAINFO, "/mnt/nvram/rom/cacert.pem");
		curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, NULL); // NULL causes writing to fp passed into CURLOPE_WRITEDATA
		curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, fp);

		ats::StringList::const_iterator it = headers.begin();
		for (; it != headers.end(); ++it)
		{
			list = curl_slist_append(list, (*it).c_str());
			ats_logf(ATSLOG_DEBUG, "%s,%d: header %s   	", __FILE__, __LINE__, (*it).c_str());
		}

		curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, list);
		curl_easy_setopt(curlHandle, CURLOPT_VERBOSE, 0L); //set 1L to turn on verbose

		if ((curlError = curl_easy_perform(curlHandle)) != 0)
		{
			ats_logf(ATSLOG_ERROR, "%s,%d: " RED_ON "Fail to execute curl_easy_perform %d" RESET_COLOR, __FILE__, __LINE__, curlError);
			curl_easy_cleanup(curlHandle);
			free(storage.ptr);
			return ats::String();
		}

		curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &http_code);
		retVal = ats::String(storage.ptr, storage.len);
		ats_logf(ATSLOG_INFO, "%s,%d: curl response: %s", __FILE__, __LINE__, retVal.c_str() );
		free(storage.ptr);

		curl_easy_cleanup(curlHandle);
		curl_slist_free_all(list);
	}
	return retVal;
}
  
//-------------------------------------------------------------------------------------------------
// The data in usr_msg_data will be url:"xxx' data:{data}
// build a standard header and send the data to the URL
int CellSender::sendSingleMessage(message_info* p_mi)
{
	ats::String user_msg = ats::String(ats::from_hex(p_mi->sm.get("usr_msg_data").c_str()));
	const int code = p_mi->sm.get_int("event_type");
	char mybuf[2048];
	strncpy(mybuf, user_msg.c_str(), user_msg.size());	
	mybuf[user_msg.size()] = '\0';


	BlinkCellLED(true);//
    
	if(code == TRAK_INET_MSG && user_msg.length())
	{
		g_INetIPC.RunState(NORMAL);
		ats_logf(ATSLOG_INFO, "%s,%d: send message ##%s##", __FILE__, __LINE__, user_msg.c_str());
		Document doc;

		if (doc.Parse<0>(mybuf).HasParseError())
		{
			ats_logf(ATSLOG_INFO, "%s,%d: Error message JSON has errors!", __FILE__, __LINE__);
			BlinkCellLED(false);//
			return 0;  // discard message if it can't be parsed
		}
				
		std::string url = doc["url"].GetString();
		std::string mac = doc["mac"].GetString();
		std::string strData;
		std::string strFFID = "/FFFFFFFF/";
		std::string strNull = "/0/";
		
		if (doc["data"].IsObject() )
		{
			StringBuffer sb;            
			Writer<StringBuffer> writer(sb);
			doc["data"].Accept(writer);
			strData = sb.GetString();
		}
		ats_logf(ATSLOG_INFO, "%s,%d: data %s", __FILE__, __LINE__, strData.c_str());

		if(url.find(strFFID) != std::string::npos || url.find(strNull) != std::string::npos) // <ISCP-322> 
		{

			url = g_pConfig->iNetURL()	 + "/iNetAPI/v1/live/" + g_pConfig->SerialNum() + "/createinst"; //<ISCP-346>
			ats_logf(ATSLOG_DEBUG, CYAN_ON "----------- creatinst url %s" RESET_COLOR, url.c_str());
			ats_logf(ATSLOG_DEBUG, GREEN_ON "========== creatinst data %s" RESET_COLOR, strData.c_str());
		}

		ats::StringList headers;
		int r;
		if ((r = BuildJSONHeader(headers)) != 0)  // only happens if auth fails - what do we do?  Keep trying or drop message?
		{
			ats_logf(ATSLOG_DEBUG, RED_ON "%s,%d: Failed to build JSON header: %d" RESET_COLOR, __FILE__,__LINE__, r);
			BlinkCellLED(false);//
			return -1;
		}

		// push to iNet servers
		int http_code, curlError;
		ats_logf(ATSLOG_DEBUG, GREEN_ON "%s" RESET_COLOR, url.c_str());
		ats::String ret = PostCurlRequest(url, strData, headers, http_code, curlError);
		

		if( 0 != curlError)
		{
			RegisterCurlError(true);
			ats_logf(ATSLOG_DEBUG, RED_ON "%s,%d: CURL Error: %d result: %s for mac %s" RESET_COLOR, __FILE__,__LINE__, curlError, ret.c_str(), mac.c_str());
			//Notify equipment that it is not in contact with iNet
			// ISCP-339 g_INetIPC.INetConnected(false);
			BlinkCellLED(false);//
			return -1;
		}
		else if (http_code >= 200 && http_code < 300 )
		{
			RegisterCurlError(false);
			RegisterHTTPError(false);
			ats_logf(ATSLOG_DEBUG, YELLOW_ON "%s,%d: result: %s for mac %s" RESET_COLOR, __FILE__,__LINE__, ret.c_str(), mac.c_str());
			LensMAC newmac;
			newmac.SetMACHex(mac);
			int idx = g_INetIPC.Add(newmac, ret);
			if (idx == -1)  // array is full!
			{
				// TODO: send the Network FULL message.
			}
			else if (idx == -2) // unit is leaving
				ats_logf(ATSLOG_INFO, YELLOW_ON "%s,%d: iNet notified of MAC %s leaving network" RESET_COLOR, __FILE__, __LINE__, mac.c_str());			
			else
				ats_logf(ATSLOG_INFO, "%s,%d: Added objectID %s to MAC %s", __FILE__, __LINE__, ret.c_str(), mac.c_str());
				
			g_INetIPC.Dump();
		}
		else
		{
			RegisterHTTPError(true);
				ats_logf(ATSLOG_DEBUG, RED_ON "%s,%d: DROPPING message: Http Error: %d result: %s for mac %s" RESET_COLOR, __FILE__,__LINE__, http_code, ret.c_str(), mac.c_str());
				BlinkCellLED(false);//
				return 0; // drop the message because it causes a failure at iNet 
			}
		}
	else 	if (code == TRAK_CRITICAL_BATTERY_MSG)
	{
		//;g_INetIPC.RunState(LOW_BATTERY); // disable the low battery alarm bit in the TGX status (Henry 5/7/2020)
		g_INetIPC.RunState(NORMAL);
		inetUpdateGateway();
	}
	else if (code == TRAK_INET_ERROR)
	{
		ATS_TRACE;
		BlinkCellLED(false);//
		if( inetUploadError(p_mi) != 0)
			{
				return -1; // If an error events fails to be sent through cellular, donot remove it
			}
		else
		{
			return 0;
		}
		ATS_TRACE;
	}
	else
	{
		ats_logf(ATSLOG_DEBUG, "%s,%d:" YELLOW_ON "Discarding Event Message %d for  mid %d seq %d pri %d" RESET_COLOR, __FILE__, __LINE__, code, p_mi->mid, (int)p_mi->seq, (int)p_mi->pri);
		BlinkCellLED(false);//<ISCP-355>
		return 1;// remove message but with failure on cellular
	}
    
	BlinkCellLED(false);//
	lock();
	m_sequence_num = p_mi->seq;
	sem_post(&m_ack_sem);
	unlock();
	return 0;
}


//-------------------------------------------------------------------------------------------------
int CellSender::waitforack()
{
	return 1;  // do not use acks in iNet
}

//-------------------------------------------------------------------------------------------------
std::string CellSender::inetBuildGPSJSON()
{
	std::string strGPS;
	NMEA_Client nmea;
	NMEA_DATA data = nmea.GetData();

	ats_logf(ATSLOG_DEBUG, "%s,%d:" YELLOW_ON "GPS_IS_VALID_FLAG:%d GPS_Quality:%d\n" RESET_COLOR, __FILE__, __LINE__,data.isValid(),data.gps_quality);
	if (data.isValid() == false || data.gps_quality == 0)
		return strGPS; // return an empty string if GPS is invalid

	std::string strFmt = "\"p\": {\"lat\":%2.6f,\"lon\":%2.6f,\"a\":%2.6f,\"b\":%2.6f,\"s\":%2.4f}"; // ISCP-354
	char buf[128];
	sprintf(buf, strFmt.c_str(), data.ddLat, data.ddLon, data.hdop, data.ddCOG, data.sog);
	strGPS = buf;
	return strGPS;
}

//-------------------------------------------------------------------------------------------------
int CellSender::inetGetAuthentication()
{
	ats::StringList headers;
	headers.push_back("Content-Type: application/x-www-form-urlencoded;charset=UTF-8");

	std::string query_fmt = "grant_type=password&client_id=%s&client_secret=%s&username=%s&password=%s";
	ats::String uri;
	ats_sprintf(&uri, query_fmt.c_str(), m_client_id.c_str(), m_client_secret.c_str(), m_username.c_str(), m_password.c_str());

	int http_code, curlError;
	ats::String url = g_pConfig->iNetURL() + "/oauth2/endpoint/iNet/token";
	ats::String auth = PostCurlRequest(url, uri, headers, http_code,curlError);

	if (!auth.empty() && (http_code == 200 || http_code == 201 ))
	{
		ats_logf(ATSLOG_INFO, "%s,%d: Read authentication message %s, http status code %d", __FILE__, __LINE__, auth.c_str(), http_code);
		Document doc;
		doc.Parse(auth.c_str());
		assert(doc.IsObject());
		assert(doc["access_token"].IsString());
		assert(doc["refresh_token"].IsString());
		assert(doc["token_type"].IsString());
		assert(doc["expires_in"].IsInt());
		accessToken.access_token = doc["access_token"].GetString();
		accessToken.refresh_token = doc["refresh_token"].GetString();
		accessToken.token_type = doc["token_type"].GetString();
		accessToken.expires_in = doc["expires_in"].GetInt();
		ats_logf(ATSLOG_INFO, "%s,%d: access_token %s", __FILE__, __LINE__, accessToken.access_token.c_str());
		RegisterAuthError(false);
		return 0;
	}
	else
	{
		if( !auth.empty() )
		{
			RegisterAuthError(true);
		}
		ats_logf(ATSLOG_ERROR, "%s-%d getAuthentication - unable to retrieve auth token. Http ret code is %d", __FILE__, __LINE__, http_code);
	}
	return -1;
}

//-------------------------------------------------------------------------------------------------
ssize_t CellSender::inetCreateGateway()
{
	ats::StringList headers;
	int r;
	if ((r = BuildJSONHeader(headers)) != 0)
		return r;

	std::string strData, gps;
		
	int connectionState = 0x00; 

    //ISCP-295
	if (m_RedStone.pppState() == APS_CONNECTED)
	{
        connectionState = (((int)g_INetIPC.RunState() & 0xFFFFF1FF) | 0x800); 
	}
	else 
	{
        if (m_RedStone.isEthernetInternetWorking() == true)
        {
            connectionState = (((int)g_INetIPC.RunState() & 0xFFFFF1FF) | 0x200); 
        }
        else 
        {
            connectionState = (((int)g_INetIPC.RunState() & 0xFFFFF1FF) | 0x800); //ISCP-346
        }
         
	}		
	strData = str(boost::format("{\"s\":%d,\"site\":\"") % connectionState);
	strData += g_pConfig->SiteName() + "\"";
	gps = inetBuildGPSJSON();

	if (gps.length())
	{
		strData += ", ";
		strData += gps;
	}
	strData += "}";

	// push to iNet servers
	if (m_bPS19Test)
	{
		TestIridiumCreateGateway();
		return 0;
	}

	int http_code, curlError;
	std::string url = g_pConfig->iNetURL()	 + "/iNetAPI/v1/live/" + g_pConfig->SerialNum() + "/create";
	ats_logf(ATSLOG_DEBUG, CYAN_ON "inetCreateGateway url %s" RESET_COLOR, url.c_str());
	ats_logf(ATSLOG_DEBUG, GREEN_ON "inetCreateGateway data %s" RESET_COLOR, strData.c_str());
	ats::String ret = PostCurlRequest(url, strData, headers, http_code, curlError);
	ats_logf(ATSLOG_DEBUG, YELLOW_ON "%s,%d: result: %s" RESET_COLOR, __FILE__,__LINE__, ret.c_str());
		
	if (!ret.empty() && (http_code == 200 || http_code == 201 ))
	{
		g_INetIPC.GatewayObjectID(StrToLL(ret));
		ats_logf(ATSLOG_INFO, "%s,%d: Create Gateway response is ## %s ##, http status code %d Object ID is %llu", __FILE__, __LINE__, ret.c_str(), http_code, (long long unsigned)g_INetIPC.GatewayObjectID());
		g_INetIPC.INetConnected(true);
		return 0;
	}

	if (curlError != 0 || (http_code < 200 || http_code > 299))
	{
		if(http_code == 401)
			ats_logf(ATSLOG_ERROR, "%s,%d: " RED_ON "Failed to CreateGateway - SN %s is not recognized by iNet, status code: %d" RESET_COLOR, __FILE__, __LINE__, g_pConfig->SerialNum().c_str(), http_code);
		else
			ats_logf(ATSLOG_ERROR, "%s,%d:" RED_ON " Failed to CreateGateway, status code: %d" RESET_COLOR, __FILE__, __LINE__, http_code);

		if (curlError != 0)
		{
			// ISCP-339 g_INetIPC.INetConnected(false);
		}
		inetGetAuthentication(); // get new authentication
		return -1;
	}

	return 0;
}

//-------------------------------------------------------------------------------------------------
ssize_t CellSender::inetUpdateGateway()
{
	if (m_bPS19Test)
	{
		TestIridiumUpdateGateway();
		return 0;
	}

	ats::StringList headers;
	int r;
	if ((r = BuildJSONHeader(headers)) != 0)
		return r;

	std::string strData, gps;
	int connectionState = 0x00; 

        //ISCP-295
	if (m_RedStone.pppState() == APS_CONNECTED)
	{
            connectionState = (((int)g_INetIPC.RunState() & 0xFFFFF1FF) | 0x800); 
        }
        else 
         {
            if (m_RedStone.isEthernetInternetWorking() == true)
            {
                 connectionState = (((int)g_INetIPC.RunState() & 0xFFFFF1FF) | 0x200); 
            
            }
            else 
            {
                connectionState = (((int)g_INetIPC.RunState() & 0xFFFFF1FF) | 0x800); //ISCP-346 add a default network as Cellular
            }
	}
	strData = str(boost::format("{\"s\":%d,\"site\":\"") % connectionState);
	strData += g_pConfig->SiteName() + "\"";
	gps = inetBuildGPSJSON();

	if (gps.length())
	{
		strData += ", ";
		strData += gps;
	}
	strData += "}";

	// push to iNet servers
	int http_code, curlError;
	std::string url = g_pConfig->iNetURL()	 + "/iNetAPI/v1/live/";
	url += str( boost::format("%llu/update") % g_INetIPC.GatewayObjectID());
	ats_logf(ATSLOG_DEBUG, GREEN_ON "inetUpdateGateway url %s" RESET_COLOR, url.c_str());
	ats_logf(ATSLOG_DEBUG, GREEN_ON "inetUpdateGateway data %s" RESET_COLOR, strData.c_str());
	ats::String ret = PostCurlRequest(url, strData, headers, http_code, curlError);
	ats_logf(ATSLOG_DEBUG, YELLOW_ON "%s,%d: result: %s" RESET_COLOR, __FILE__,__LINE__, ret.c_str());
		
	if (!ret.empty() && (http_code == 200 || http_code == 201 ))
	{
		g_INetIPC.GatewayObjectID(StrToLL(ret));
		ats_logf(ATSLOG_INFO, "%s,%d:Update Gateway response is ## %s ##, http status code %d Object ID is %llu", __FILE__, __LINE__, ret.c_str(), http_code, (long long unsigned)g_INetIPC.GatewayObjectID());
		g_INetIPC.INetConnected(true);
		return 0;
	}

	if (curlError != 0 || (http_code < 200 || http_code > 299))
	{
		if(curlError != 0 || (http_code == 401 || http_code == 404 || http_code == 400) )
		{
			ats_logf(ATSLOG_ERROR, "%s,%d: Failed to Update Gateway, object not recognized by iNet. status code: %d", __FILE__, __LINE__, http_code);
			if (curlError != 0)
			{
				// ISCP-339 g_INetIPC.INetConnected(false);
			}
			inetGetAuthentication(); // get new authentication
			return inetCreateGateway();  // try to restablish gateway.
		}
		
		ats_logf(ATSLOG_ERROR, "%s,%d: " RED_ON "Fail to Update Gateway, status code: %d" RESET_COLOR, __FILE__, __LINE__, http_code);
		return -1;
	}
	//243
	return -2;
}

//-------------------------------------------------------------------------------------------------
// Returns false if you cannot get an authentication.
//
bool CellSender::inetBuildAuthHeader(ats::String &header)
{
	std::string fmt = "Authorization: Bearer %s";

	if (accessToken.access_token.empty())
	{
		if (inetGetAuthentication() != 0)
		{
			return false;
		}
	}

	ats_sprintf(&header, fmt.c_str(), accessToken.access_token.c_str());

	return true;
}

//-------------------------------------------------------------------------------------------------
uint64_t CellSender::StrToLL(std::string strVal)
{
	std::stringstream sstr(strVal);
	uint64_t val;
	sstr >> val;
	return val;
}

//-------------------------------------------------------------------------------------------------
// this header is required when sending JSON.
// returns -1 if no auth.
//
int CellSender::BuildJSONHeader(ats::StringList &headers)
{
	headers.push_back("Content-Type: application/json");

	ats::String header;
	std::string acct = "X-accountId: " + g_pConfig->INetAccount();
	headers.push_back(acct.c_str());
	// Needs to have a valid access token.
	if(!inetBuildAuthHeader(header))
		return -1;
			
	headers.push_back(header);
	return 0;
}

//-------------------------------------------------------------------------------------------------
// this header is required when sending parameters.
int CellSender::BuildParamHeader(ats::StringList &headers)
{
	ats::String header;
	// Needs to have a valid access token.
	if(!inetBuildAuthHeader(header))
		return -1;
			
	headers.push_back(header);
	std::string acct = "X-accountId: " + g_pConfig->INetAccount();	
	headers.push_back(acct);
	return 0;
}

//-------------------------------------------------------------------------------------------------
// Source: iNet v7.7 Core Server Functional Specification - Page 13
// (CS-1705) This endpoint is used to upload a debug log file from a smart device.  
//	The first (POST) request is made to obtain an id, which is then used in the second (PUT) request to 
//	upload the file.
//		HTTP Request
//		POST https://inetsync.indsci.com/iNetAPI/v1/file
//		Content-Type: application/json

ssize_t CellSender::inetUploadDebugLog()
{
	ats::StringList headers;
	int r;
	if ((r = BuildJSONHeader(headers)) != 0)
		return r;

	// push to iNet servers
	int http_code, curlError;
	std::string strData = "";
	strData = str( boost::format("comment=%s_isc-len.log") % g_pConfig->SerialNum() );
	std::string url = g_pConfig->iNetURL()	 + "/iNetAPI/v1/file?" + strData;
	ats_logf(ATSLOG_DEBUG, GREEN_ON "inetUploadDebugLog url %s" RESET_COLOR, url.c_str());
	strData = "";
	ats::String ret = PostCurlRequest(url, strData, headers, http_code, curlError);
	ats_logf(ATSLOG_DEBUG, YELLOW_ON "%s,%d: result: %s" RESET_COLOR, __FILE__,__LINE__, ret.c_str());
		
	if (!ret.empty() && (http_code == 200 || http_code == 201 ))
	{
		g_INetIPC.GatewayObjectID(StrToLL(ret));
		ats_logf(ATSLOG_INFO, "%s,%d:Update Gateway response is ## %s ##, http status code %d Object ID is %llu", __FILE__, __LINE__, ret.c_str(), http_code, (long long unsigned)g_INetIPC.GatewayObjectID());
		g_INetIPC.INetConnected(true);
		return 0;
	}

	if (curlError != 0 || (http_code < 200 || http_code > 299))
	{
		if (curlError != 0 || (http_code == 401 || http_code == 404) )
		{
			ats_logf(ATSLOG_ERROR, "%s,%d: Failed to Update Gateway, object not recognized by iNet. status code: %d", __FILE__, __LINE__, http_code);
			if (curlError != 0)
			{
				// ISCP-339 g_INetIPC.INetConnected(false);
			}
			inetGetAuthentication(); // get new authentication
			return inetCreateGateway();  // try to restablish gateway.
		}
		
		ats_logf(ATSLOG_ERROR, "%s,%d: " RED_ON "Fail to Update Gateway, status code: %d" RESET_COLOR, __FILE__, __LINE__, http_code);
		return -1;
	}

	return 0;
}

//-------------------------------------------------------------------------------------------------
// Source: iNet v7.7 Core Server Functional Specification - Page 16
//	(CS-2061)   This endpoint is used to exchange status with iNet. It allows the equipment to give 
//	status about its current operation. It also allows iNet to inform the equipment of any data 
//	updates that the equipment does not yet have. This is done through a series of versions. 
//	Each version has an associated download service that allows the equipment to retrieve the data
//	 updates.  Exchange status also returns the account that the equipment is currently associated with.
//
//	HTTP Request
//	GET https://inetnewstg.indsci.com/iNetAPI/v1/equipment/{sn}/status
//	Content-Type: application/json
//
// returns: 0 - OK
//					-1 - failed iNet call
//          -2 - failed to build Parameter header???  should not happen.
//					-3 - unexpected HTTP response

int CellSender::inetExchangeStatusInformation()
{
	ats::StringList headers;
	ats::String header;
	// Needs to have a valid access token.
	if(inetBuildAuthHeader(header))
		headers.push_back(header);
	else
		return -2;

	// push to iNet servers
	int http_code, curlError;
	std::string url = g_pConfig->iNetURL()	 + "/iNetAPI/v1/equipment/" + g_pConfig->SerialNum() + "/status?returnVersions=true";
	ats_logf(ATSLOG_DEBUG, GREEN_ON "inetExchangeStatusInformation " MAGENTA_ON "url %s" RESET_COLOR, url.c_str());
	ats::String ret = GetCurlRequest(url, headers, http_code, curlError);
	ats_logf(ATSLOG_DEBUG, YELLOW_ON "%s,%d: result: %s" RESET_COLOR, __FILE__,__LINE__, ret.c_str());
		
	if (!ret.empty() && (http_code == 200 || http_code == 201 ))
	{
		ats_logf(ATSLOG_INFO, CYAN_ON "%s,%d:inetExchangeStatusInformation http status code %d.  Response is:" RESET_COLOR, __FILE__, __LINE__, http_code);
		ats_logf(ATSLOG_INFO, YELLOW_ON "%s" RESET_COLOR, ret.c_str());
		ParseExchangeStatusInformationResponse(ret);
			
		if (m_bUpdateSettings)  // only call if the ExchangeStatusInformation indicates new settings.
		{
			ats_logf(ATSLOG_ERROR, RED_ON "We need to download the settings!");
			return inetDownloadSettings(ret);//ISCP-345
		}

		return 0;
	}
	else if ( http_code == 400 )
	{
		ats_logf(ATSLOG_ERROR, RED_ON "Failed inetExchangeStatusInformation - (%d) returned by iNet. Response is:" RESET_COLOR, http_code);
		ats_logf(ATSLOG_INFO, YELLOW_ON "%s" RESET_COLOR, ret.c_str());
		return -1;
	}
	else
	{
		ats_logf(ATSLOG_ERROR, RED_ON "inetDownloadSettings - unexpected error code (%d) returned by iNet. Response is:" RESET_COLOR, http_code);		
		ats_logf(ATSLOG_INFO, YELLOW_ON "%s" RESET_COLOR, ret.c_str());
		return -3;
	}
	return 0;
}


//-------------------------------------------------------------------------------------------------
void CellSender::ParseExchangeStatusInformationResponse(std::string strJson)
{
	rapidjson::Document document;
	document.Parse(strJson.c_str());

	if (document.HasMember("accountId") && (document["accountId"].IsString()) )
	{
		if (g_pConfig->INetAccount() != document["accountId"].GetString())
		{
			db_monitor::ConfigDB db;
			db.Set("isc-lens", "INetAccount", document["accountId"].GetString());
			db.Set("isc-lens", "SettingsVersion", "2019-01-01T00:00:00.000-0400"); // reset the setting time.
			g_pConfig->Load();
			m_bAccountChanged =  true;
		}
	}
	// pull the settingsVersion time and check to see if the settings need to be updated
	if (document.HasMember("settingsVersion") && (document["settingsVersion"].IsString()) )
	{
		std::string settingsVersion = document["settingsVersion"].GetString();
		ats_logf(ATSLOG_ERROR, RED_ON "SettingsVersion: existing %s   new %s" RESET_COLOR, g_pConfig->SettingsVersion().c_str(), settingsVersion.c_str());
		
		if (g_pConfig->SettingsVersion() < settingsVersion) // string comparison works with these time strings if TimeZone is constant
		{
			ats_logf(ATSLOG_ERROR, RED_ON "SettingsVersion is triggering a DownloadSettings" RESET_COLOR);
//ISCP-345 removed updating settings here.
			m_bUpdateSettings = true;  // this will trigger the DownloadSettings 
		}
	}
}


//-------------------------------------------------------------------------------------------------
// Source: iNet v7.7 Core Server Functional Specification - Page 18
//
//		(CS-2060) This endpoint is used to get settings for equipment.  
//		HTTP Request
//		GET https://inetsync.indsci.com/iNetAPI/v1/equipment/{sn}/settings
//		Content-Type: application/json
//
// returns: 0 - OK
//					-1 - failed iNet call
//          -2 - failed to build Parameter header???  should not happen.
//					-3 - unexpected HTTP response
//
int CellSender::inetDownloadSettings(std::string strJson)
{
	rapidjson::Document documentExchangeInfo;
	documentExchangeInfo.Parse(strJson.c_str());//ISCP-345

	ats::StringList headers;
	int r;
	if ((r = BuildParamHeader(headers)) != 0)
		return -2;

	// push to iNet servers
	int http_code, curlError;
	std::string url = g_pConfig->iNetURL()	 + "/iNetAPI/v1/equipment/" + g_pConfig->SerialNum() + "/settings?" +
		"settingsVersion="+ g_pConfig->SettingsVersionNoTZ() + "&" +
		"equipmentSoftwareVersion=" + g_pConfig->FWVer();
	ats_logf(ATSLOG_DEBUG, GREEN_ON "inetDownloadSettings " MAGENTA_ON "url %s" RESET_COLOR, url.c_str());
	ats::String ret = GetCurlRequest(url, headers, http_code, curlError);
	ats_logf(ATSLOG_DEBUG, YELLOW_ON "%s,%d: http status code %d.\nresult: %s" RESET_COLOR, __FILE__,__LINE__, http_code, ret.c_str());
		
	if (!ret.empty() && (http_code == 200 || http_code == 201 ))
	{
		ParseDownloadSettingsResponse(ret);
        
//ISCP-345
		// pull the settingsVersion time and check to see if the settings need to be updated
		if (documentExchangeInfo.HasMember("settingsVersion") && (documentExchangeInfo["settingsVersion"].IsString()) )
		{
			std::string settingsVersion = documentExchangeInfo["settingsVersion"].GetString();
			ats_logf(ATSLOG_ERROR, RED_ON "SettingsVersion: existing %s   new %s" RESET_COLOR, g_pConfig->SettingsVersion().c_str(), settingsVersion.c_str());

			if (g_pConfig->SettingsVersion() < settingsVersion) // string comparison works with these time strings if TimeZone is constant
			{
				ats_logf(ATSLOG_ERROR, RED_ON "SettingsVersion is being updated after successful downlading" RESET_COLOR);
				db_monitor::ConfigDB db;
				db.Set("isc-lens", "SettingsVersion", documentExchangeInfo["settingsVersion"].GetString());
				g_pConfig->SettingsVersion(documentExchangeInfo["settingsVersion"].GetString());			
			}
		}

        if (m_bIsSettingDownloaded == true)
        {
        	ats_logf(ATSLOG_DEBUG, RED_ON "INet Settings - New settings downloaded - rebooting!!" RESET_COLOR);			
			::system("touch /tmp/logdir/inet.txt");
			sleep(2);
			::system("/sbin/reboot");
        }
		

		return 0;
	}
	else if(http_code == 400)
	{
		return -1;
	}
	else
	{
		ats_logf(ATSLOG_ERROR, RED_ON "inetDownloadSettings - unexpected error code (%d) returned by iNet. Response is:" RESET_COLOR, http_code);		
		return -3;
	}
	return 0;
}

//-------------------------------------------------------------------------------------------------
void CellSender::ParseDownloadSettingsResponse(std::string strJson)
{
	db_monitor::ConfigDB db;
	Document document;
	document.Parse(strJson.c_str());
	if(document.HasParseError())
	{
		RegisterSettingsParserError(true);
	}
	Value::ConstMemberIterator itrESH = document.FindMember("equipmentSettingsHeader");
	if (itrESH == document.MemberEnd()) 
		return;

	Value::ConstMemberIterator itrSet = itrESH->value.FindMember("settings"); 
	if (itrSet == document.MemberEnd()) 
		return;

	bool changed = false;
	bool debugged = false;

  RegisterSettingsParserError(false);
  // ISCP-305: update default values of the custom key first, These will be applied in case iNet 
  // does not send these values
  db.Update("isc-lens","Encryption","2");	// ISCP-305
  db.Update("isc-lens","NetworkEncryption","Aware360DevKey17");	
  
  changed |=SaveDownloadSetting(itrSet->value, db, "NONCRIT_INTV", "NonCriticalInterval");
	if (!debugged && changed)	{	ats_logf(ATSLOG_ERROR, RED_ON "inetDownloadSettings - different on line %d" RESET_COLOR, __LINE__);	debugged = true;}
  changed |=SaveDownloadSetting(itrSet->value, db, "LENS_HOPS", "MaxHops");
	if (!debugged && changed)	{	ats_logf(ATSLOG_ERROR, RED_ON "inetDownloadSettings - different on line %d" RESET_COLOR, __LINE__);	debugged = true;}
  changed |=SaveDownloadSetting(itrSet->value, db, "LENS_PRI_CH", "PrimaryChannel");
	if (!debugged && changed)	{	ats_logf(ATSLOG_ERROR, RED_ON "inetDownloadSettings - different on line %d" RESET_COLOR, __LINE__);	debugged = true;}
  changed |=SaveDownloadSetting(itrSet->value, db, "LENS_SEC_CH", "SecondaryChannel");
	if (!debugged && changed)	{	ats_logf(ATSLOG_ERROR, RED_ON "inetDownloadSettings - different on line %d" RESET_COLOR, __LINE__);	debugged = true;}
  changed |=SaveDownloadSetting(itrSet->value, db, "LOST_THRESH_GROUP", "InstrumentLostSeconds");
	if (!debugged && changed)	{	ats_logf(ATSLOG_ERROR, RED_ON "inetDownloadSettings - different on line %d" RESET_COLOR, __LINE__);	debugged = true;}
  changed |=SaveDownloadSetting(itrSet->value, db, "LENS_CH_MASK", "ChannelMask");
    if (!debugged && changed)	{	ats_logf(ATSLOG_ERROR, RED_ON "inetDownloadSettings - different on line %d" RESET_COLOR, __LINE__);	debugged = true;}
  changed |=SaveDownloadSetting(itrSet->value, db, "LENS_FTR_BITS", "FeatureBits");
	if (!debugged && changed)	{	ats_logf(ATSLOG_ERROR, RED_ON "inetDownloadSettings - different on line %d" RESET_COLOR, __LINE__);	debugged = true;}
  changed |=SaveDownloadSetting(itrSet->value, db, "KEEPALIVE_INTV", "INetKeepAliveMinutes");
	if (!debugged && changed)	{	ats_logf(ATSLOG_ERROR, RED_ON "inetDownloadSettings - different on line %d" RESET_COLOR, __LINE__);	debugged = true;}
  changed |=SaveDownloadSetting(itrSet->value, db, "KEY_CUSTOM", "Encryption");
	if (!debugged && changed)	{	ats_logf(ATSLOG_ERROR, RED_ON "inetDownloadSettings - different on line %d" RESET_COLOR, __LINE__);	debugged = true;}
  changed |=SaveDownloadSetting(itrSet->value, db, "LENS_CUSTOM_KEY", "NetworkEncryption");
	if (!debugged && changed)	{	ats_logf(ATSLOG_ERROR, RED_ON "inetDownloadSettings - different on line %d" RESET_COLOR, __LINE__);	debugged = true;}
  changed |=SaveDownloadSetting(itrSet->value, db, "GAS_EVENT_INTV", "CriticalIntervalMinutes");
	if (!debugged && changed)	{	ats_logf(ATSLOG_ERROR, RED_ON "inetDownloadSettings - different on line %d" RESET_COLOR, __LINE__);	debugged = true;}
  changed |=SaveDownloadSetting(itrSet->value, db, "GAS_EVENT_INTV", "SatelliteCriticalIntervalMinutes");
	if (!debugged && changed)	{	ats_logf(ATSLOG_ERROR, RED_ON "inetDownloadSettings - different on line %d" RESET_COLOR, __LINE__);	debugged = true;}
  changed |=SaveDownloadSetting(itrSet->value, db, "SATENABLE", "packetizer-inet", "IridiumEnable");
	if (!debugged && changed)	{	ats_logf(ATSLOG_ERROR, RED_ON "inetDownloadSettings - different on line %d" RESET_COLOR, __LINE__);	debugged = true;}
  changed |=SaveDownloadSetting(itrSet->value, db, "SATENABLE", "feature", "iridium-monitor");
	if (!debugged && changed)	{	ats_logf(ATSLOG_ERROR, RED_ON "inetDownloadSettings - different on line %d" RESET_COLOR, __LINE__);	debugged = true;}
  changed |=SaveDownloadSetting(itrSet->value, db, "SLEEP_KEEP_AWAKE", "RedStone", "KeepAwakeMinutes");
	if (!debugged && changed)	{	ats_logf(ATSLOG_ERROR, RED_ON "inetDownloadSettings - different on line %d" RESET_COLOR, __LINE__);	debugged = true;}
  changed |=SaveDownloadSetting(itrSet->value, db, "WL_MAX_PEERS", "MaxPeers");
	if (!debugged && changed)	{	ats_logf(ATSLOG_ERROR, RED_ON "inetDownloadSettings - different on line %d" RESET_COLOR, __LINE__);	debugged = true;}
  changed |=SaveDownloadSetting(itrSet->value, db, "SLEEP_LOW_BAT_THRESH", "wakeup", "CriticalVoltage");
	if (!debugged && changed)	{	ats_logf(ATSLOG_ERROR, RED_ON "inetDownloadSettings - different on line %d" RESET_COLOR, __LINE__);	debugged = true;}
  changed |=SaveDownloadSetting(itrSet->value, db, "WAKE_VOLT_LOCK_THRESH", "wakeup", "WakeupVoltage");
	if (!debugged && changed)	{	ats_logf(ATSLOG_ERROR, RED_ON "inetDownloadSettings - different on line %d" RESET_COLOR, __LINE__);	debugged = true;}
  changed |=SaveDownloadSetting(itrSet->value, db, "WAKE_VOLT_ABOVE_THRESH", "wakeup", "ShutdownVoltage");
	if (!debugged && changed)	{	ats_logf(ATSLOG_ERROR, RED_ON "inetDownloadSettings - different on line %d" RESET_COLOR, __LINE__);	debugged = true;}
  changed |=SaveDownloadSetting(itrSet->value, db, "SAT_POS_INTERVAL", "SatUpdateMinutes");
	if (!debugged && changed)	{	ats_logf(ATSLOG_ERROR, RED_ON "inetDownloadSettings - different on line %d" RESET_COLOR, __LINE__);	debugged = true;}
  changed |=SaveDownloadSetting(itrSet->value, db, "GPS_INTV", "GPSUpdateMinutes");
	if (!debugged && changed)	{	ats_logf(ATSLOG_ERROR, RED_ON "inetDownloadSettings - different on line %d" RESET_COLOR, __LINE__);	debugged = true;}
  changed |=SaveDownloadSetting(itrSet->value, db, "pingInterval", "PingIntervalSeconds");
	if (!debugged && changed)	{	ats_logf(ATSLOG_ERROR, RED_ON "inetDownloadSettings - different on line %d" RESET_COLOR, __LINE__);	debugged = true;}
  changed |=SaveDownloadSetting(itrSet->value, db, "NTP_URL", "NTP_URL");
	if (!debugged && changed)	{	ats_logf(ATSLOG_ERROR, RED_ON "inetDownloadSettings - different on line %d" RESET_COLOR, __LINE__);	debugged = true;}

	// now parse the options.  Only concerned about 'SAT_ENABLE' - if it isn't there the iridium-monitor is off.  On if it is there.
	// The GROUPx option sets the network name (because its harder than sending a setting!)
	bool isSat = false;

	Value::ConstMemberIterator itrOptions = itrESH->value.FindMember("options"); 

	if (itrOptions != document.MemberEnd()) 
	{
		for (Value::ConstValueIterator itr = itrOptions->value.Begin(); itr != itrOptions->value.End(); ++itr)
		{
			std::string val = itr->GetString();
			if (val == "SAT_ENABLE")
			{
				isSat = true;
				ats_logf(ATSLOG_DEBUG, GREEN_ON "INet Settings - SAT_ENABLE found" RESET_COLOR);
			}
			else if (val.substr(0, 5) == "GROUP")
			{
				db.Update("isc-lens", "NetworkName", val.substr(5, 1).c_str()); 
			}
		}
	}

	//db.Set("feature", "iridium-monitor", (isSat ? "1" : "0"));  // set the iridium feature. // ISCP-340

	if (changed)
	{
		g_pConfig->Load();		
		g_INetIPC.NewRegisters(true);  // notify isc-lens that we have new settings.
//TODO: Fix this! we want to reboot if the setting change.		
//		if (m_bFirstUpdate) // we need to reboot - but only on the first time we connect (should be close to startup)
		{
			m_bIsSettingDownloaded = true;//ISCP-345
			db.close_db();
			
		}
	}
}
//-------------------------------------------------------------------------------------------------
// document should represent something like this:
// "settings":[
//		{"name":"GAS_EVENT_INTV","value":"1","type":"string"},
//		{"name":"GAS_EVENT_INTV_S","value":"60","type":"string"},
//		{"name":"NONCRIT_INTV","value":"4","type":"string"},
//		{"name":"NTP_URL","value":"www.timeserver.com/testing","type":"string"},
// ...]
//
bool CellSender::SaveDownloadSetting(const rapidjson::Value &document, db_monitor::ConfigDB &db, std::string docKey, std::string key = "isc-lens")
{
	return SaveDownloadSetting(document, db, docKey, "isc-lens", key);
}
bool CellSender::SaveDownloadSetting(const rapidjson::Value &document, db_monitor::ConfigDB &db, std::string docKey, std::string app, std::string key = "isc-lens")
{
	for (Value::ConstValueIterator itr = document.Begin(); itr != document.End(); ++itr) // iterate over each entry
	{
		const Value& entry = *itr; // represents {"name":"GAS_EVENT_INTV","value":"1","type":"string"}
		
		Value::ConstMemberIterator itrEntry = entry.FindMember("name");
		if (itrEntry == entry.MemberEnd()) // should never happen
			return false;
			
		if (itrEntry->value.GetString() == docKey) // have we found the one we want
		{
			// pull the value and set the db-config.
			Value::ConstMemberIterator itrValue = entry.FindMember("value");
			if (itrValue == entry.MemberEnd()) // should never happen
				return false;

			char buf[32];
			if ( docKey == "SLEEP_LOW_BAT_THRESH" || docKey == "WAKE_VOLT_LOCK_THRESH" || docKey == "WAKE_VOLT_ABOVE_THRESH" )
			{
				sprintf(buf, "%d", (int)( (atof(itrValue->value.GetString()) * 1000)));
				return db.UpdateB(app, key.c_str(), buf);  // comes in as volts - need as millivolts.
			}
			else if (docKey == "KEY_CUSTOM")  // default key from iNet is 0 but stored as 2.
			{
				std::string sKey = itrValue->value.GetString();
				int val = atoi(itrValue->value.GetString());
				if (val == 0)
					sKey = "2";
				return db.UpdateB(app, key.c_str(), sKey);
			}
			else if (docKey == "LENS_CUSTOM_KEY")  // default key from iNet is 0 but stored as 2.
			{
				db.Update(app, "Encryption", "1");
				return db.UpdateB(app, key.c_str(), itrValue->value.GetString());
			}
			else
				return db.UpdateB(app, key.c_str(), itrValue->value.GetString());
			return true;
		}
	}
	
	return false;
}

//-------------------------------------------------------------------------------------------------
// Source: iNet v7.7 Core Server Functional Specification - Page 23
//
//		(CS-2059) This endpoint is used to get firmware for equipment.  
//		HTTP Request
//		GET https://inetsync.indsci.com/iNetAPI/v1/equipment/{sn}/firmware
//		Content-Type: application/ octet-stream
//		Post Parameters
//		The download firmware update endpoint does not require any parameters.
//		Response
//		A stream that will allow for the download of the firmware file.
//		Returns
//		(CS-2054) On success, HTTP status 200/201 is returned.
// returns: 0 - OK
//					-1 - failed iNet call
//          -2 - failed to build Parameter header???  should not happen.
//					-3 - unexpected HTTP response
//
int CellSender::inetDownloadFirmware()
{
	std::string fname = "/mnt/update/firmware.lens";
	ats::StringList headers;
	int r;
	if ((r = BuildParamHeader(headers)) != 0)
		return -2;

	// push to iNet servers
	int http_code, curlError;
	std::string version;
	ats::get_file_line(version, "/version", 1);
	int v = atoi(version.c_str());
	char vbuf[32];
	sprintf(vbuf, "3.%d.0.%d", v / 10000, v%10000);
	version = vbuf;
	std::string url = g_pConfig->iNetURL()	 + "/iNetAPI/v1/equipment/" + g_pConfig->SerialNum() + "/firmware" +
		"settingsVersion="+ g_pConfig->SettingsVersionNoTZ() + "&" +
		"equipmentSoftwareVersion=" + version;
	ats_logf(ATSLOG_DEBUG, GREEN_ON "inetDownloadFirmware " MAGENTA_ON "url %s" RESET_COLOR, url.c_str());
	ats::String ret = GetFileCurlRequest(url, headers, fname, http_code, curlError);
	ats_logf(ATSLOG_DEBUG, YELLOW_ON "%s,%d: result: %s" RESET_COLOR, __FILE__,__LINE__, ret.c_str());
		
	if (!ret.empty() && (http_code == 200 || http_code == 201 ))
	{
		ats_logf(ATSLOG_INFO, CYAN_ON "%s,%d:inetDownloadFirmware http status code %d.  Response is:" RESET_COLOR, __FILE__, __LINE__, http_code);
		ParseDownloadSettingsResponse(ret);
		return 0;
	}
	else if(http_code == 400)
	{
		ats_logf(ATSLOG_ERROR, RED_ON "Failed inetDownloadFirmware - (%d) returned by iNet. Response is:" RESET_COLOR, http_code);
		return -1;
	}
	else
	{
		ats_logf(ATSLOG_ERROR, RED_ON "inetDownloadFirmware - unexpected error code (%d) returned by iNet. Response is:" RESET_COLOR, http_code);		
		return -3;
	}
	return 0;
}

//-------------------------------------------------------------------------------------------------
int CellSender::inetUploadEquipment()
{
	ats::StringList headers;
	int r;
	if ((r = BuildJSONHeader(headers)) != 0)
		return r;

	Document doc;
	BuildEquipmentJSON(doc);

	// push to iNet servers
	int http_code, curlError;
	std::string url = g_pConfig->iNetURL()	 + "/iNetAPI/v1/equipment/" + g_pConfig->SerialNum();
	ats_logf(ATSLOG_DEBUG, GREEN_ON "inetUploadEquipment " MAGENTA_ON "url %s" RESET_COLOR, url.c_str());
	StringBuffer buffer;
	Writer<StringBuffer> writer(buffer);
	doc.Accept(writer);
	std::string strData = buffer.GetString();
	ats_logf(ATSLOG_DEBUG, GREEN_ON "inetUploadEquipment data %s" RESET_COLOR, strData.c_str());
	ats::String ret = PostCurlRequest(url, strData, headers, http_code, curlError);
	ats_logf(ATSLOG_DEBUG, YELLOW_ON "%s,%d: result: %s" RESET_COLOR, __FILE__,__LINE__, ret.c_str());

	if (curlError != 0)
	{
		// ISCP-339 g_INetIPC.INetConnected(false);
	}

	if (!ret.empty() && (http_code == 200 || http_code == 201 ))
	{
		g_INetIPC.GatewayObjectID(StrToLL(ret));
		ats_logf(ATSLOG_INFO, "%s,%d: inetUploadEquipment response is ## %s ##, http status code %d Object ID is %llu", __FILE__, __LINE__, ret.c_str(), http_code, (long long unsigned)g_INetIPC.GatewayObjectID());
		g_INetIPC.INetConnected(true);
		return 0;
	}

	if (http_code < 200 || http_code > 299)
	{
		if(http_code == 401)
			ats_logf(ATSLOG_ERROR, "%s,%d: " RED_ON "Failed to inetUploadEquipment - SN %s is not recognized by iNet, status code: %d" RESET_COLOR, __FILE__, __LINE__, g_pConfig->SerialNum().c_str(), http_code);
		else
			ats_logf(ATSLOG_ERROR, "%s,%d:" RED_ON " Failed to inetUploadEquipment, status code: %d" RESET_COLOR, __FILE__, __LINE__, http_code);

		inetGetAuthentication(); // get new authentication
		return -1;
	}

	return 0;
}

//-------------------------------------------------------------------------------------------------
void CellSender::BuildEquipmentJSON(Document &doc)
{
	db_monitor::ConfigDB db;
	std::string strOut;
	std::string instTime = db.GetValue("RedStone", "InstalledOn", "2019-01-01T00:00:01.000+0000");
	std::string strVal;
	/* @Ammar Fix for ISC-294*/
	ats::String awareSerialNum;
	ats::get_file_line(awareSerialNum, "/mnt/nvram/rom/sn.txt", 1);
	/*Fix for ISC-294*/
	double dVal;
	
	Document::AllocatorType &allocator = doc.GetAllocator();
	doc.SetObject();
	doc.AddMember("equipmentType", "GATEWAY", allocator);
	doc.AddMember("equipmentCode", "TGX", allocator);
	doc.AddMember("setupDate", Value(instTime.c_str(), allocator).Move(), allocator);

	ats::String line = db.GetValue("isc-lens", "FWVer", "unknown");
	doc.AddMember("softwareVersion", Value(line.c_str(), allocator).Move(), allocator);
	doc.AddMember("hardwareVersion", "3.02", allocator);

	AFS_Timer t;
	t.SetTime();
	doc.AddMember("time", Value(t.GetTimestampWithOS().c_str(), allocator).Move(), allocator);
	
	Value parmArray(kArrayType);
	BuildParamArray(parmArray, allocator, "AWARE_SN",awareSerialNum, "string"); //@Ammar fix for ISC-294
	BuildParamArray(parmArray, allocator, "GPS_ENABLE", "true", "bool");
	BuildParamArray(parmArray, allocator, "GPS_REQ", "true", "bool");
	BuildParamArray(parmArray, allocator, "GPS_INTV", db.GetValue("isc-lens", "GPSUpdateMinutes", "5"), "int");
	BuildParamArray(parmArray, allocator, "NETWORK_NAME", db.GetValue("isc-lens", "NetworkName"), "int");

	int enc = db.GetInt("isc-lens", "Encryption", 2);
	if (enc == 2)
		BuildParamArray(parmArray, allocator, "KEY_DEFAULT", "true", "bool");
	else
	{
		BuildParamArray(parmArray, allocator, "KEY_CUSTOM", "true", "bool");
		BuildParamArray(parmArray, allocator, "LENS_CUSTOM_KEY", db.GetValue("isc-lens", "NetworkEncryption"), "string");
	}

	BuildParamArray(parmArray, allocator, "SMS_1", db.GetValue("isc-lens", "SMS1"), "string");
	BuildParamArray(parmArray, allocator, "SMS_2", db.GetValue("isc-lens", "SMS2"), "string");
	BuildParamArray(parmArray, allocator, "SMS_3", db.GetValue("isc-lens", "SMS3"), "string");
	BuildParamArray(parmArray, allocator, "NTP_URL", db.GetValue("isc-lens", "NTP_URL"), "string");

	BuildParamArray(parmArray, allocator, "WAKE_IGNITION_ENABLE", "true", "bool");  // Note - This is always true 
	BuildParamArray(parmArray, allocator, "WAKE_VOLT_LOCK_ENABLE", "true", "bool");  // Note - This is always true 
	dVal = db.GetInt("wakeup", "CriticalVoltage", 11800) / 1000.0;  //convert to volts
	strVal = str( boost::format("%.2f") % dVal);
	BuildParamArray(parmArray, allocator, "WAKE_VOLT_LOCK_THRESH", strVal, "float");  // Volts

	BuildParamArray(parmArray, allocator, "WAKE_VOLT_ABOVE_ENABLE", "true", "bool");  // Note - This is always true 
	dVal = db.GetInt("wakeup", "WakeupVoltage", 13200) / 1000.0;  //convert to volts
	strVal = str( boost::format("%.2f") % dVal);
	BuildParamArray(parmArray, allocator, "WAKE_VOLT_ABOVE_THRESH", strVal, "float");  // Volts
	
	dVal = db.GetInt("wakeup", "ShutdownVoltage", 13200) / 1000.0;  //convert to volts
	strVal = str( boost::format("%.2f") % dVal);
	BuildParamArray(parmArray, allocator, "SLEEP_LOW_BAT_THRESH", strVal, "float");  // Volts

	BuildParamArray(parmArray, allocator, "SLEEP_KEEP_AWAKE", db.GetValue("RedStone", "KeepAwakeMinutes"), "int");
	
	BuildParamArray(parmArray, allocator, "KEEPALIVE_INTV", db.GetValue("isc-lens", "INetKeepAliveMinutes"), "int");
	BuildParamArray(parmArray, allocator, "GAS_EVENT_INTV",  db.GetValue("isc-lens", "CriticalIntervalMinutes"),  "int");
	BuildParamArray(parmArray, allocator, "NONCRIT_INTV",  db.GetValue("isc-lens", "NonCriticalInterval"),  "int");
	BuildParamArray(parmArray, allocator, "CELL_ENABLE", "true", "bool");
	if  (db.GetInt("feature", "iridium-monitor", 0) == 0)
		BuildParamArray(parmArray, allocator, "SAT_ENABLE", "false", "bool");
	else
		BuildParamArray(parmArray, allocator, "SAT_ENABLE", "true", "bool");
	BuildParamArray(parmArray, allocator, "SAT_POS_INTERVAL",  db.GetValue("isc-lens", "SatUpdateMinutes", "15"), "int"); //ISCP-357, chnaged to 15 minutes by default 
	BuildParamArray(parmArray, allocator, "LOST_THRESH_GROUP", db.GetValue("isc-lens", "InstrumentLostSeconds"), "int");	
	BuildParamArray(parmArray, allocator, "LENS_PRI_CH",  db.GetValue("isc-lens", "PrimaryChannel"), "int");
	BuildParamArray(parmArray, allocator, "LENS_SEC_CH",  db.GetValue("isc-lens", "SecondaryChannel"), "int");
	BuildParamArray(parmArray, allocator, "LENS_CH_MASK", db.GetValue("isc-lens", "ChannelMask"), "int");
	BuildParamArray(parmArray, allocator, "LENS_FTR_BITS", db.GetValue("isc-lens", "FeatureBits"), "int");
	BuildParamArray(parmArray, allocator, "LENS_HOPS", db.GetValue("isc-lens", "MaxHops"), "int");	
	BuildParamArray(parmArray, allocator, "WL_MAX_PEERS", db.GetValue("isc-lens", "MaxPeers"), "int");	
	BuildParamArray(parmArray, allocator, "ETHERNET_CELLULAR_SATELLITE", "true", "bool");
	strVal = str( boost::format("GROUP%d") % db.GetInt("isc-lens", "NetworkName", 1));
	BuildParamArray(parmArray, allocator, strVal, "true", "bool");
	

	BuildParamArray(parmArray, allocator, "WIFI_SSID", db.GetValue("WiFi", "ssid"), "string");
	BuildParamArray(parmArray, allocator, "SITE", db.GetValue("isc-lens", "SiteName"), "string");
	BuildParamArray(parmArray, allocator, "LENS_SVER", g_INetIPC.GetLensRegisters().rawRadioProtocolVersion(), "string");

	if (db.GetInt("isc-lens", "EnableTestMode", 0) == 1)
		BuildParamArray(parmArray, allocator, "MODE_TESTING", "true", "bool");
	else
		BuildParamArray(parmArray, allocator, "MODE_BINDING", "true", "bool");

	doc.AddMember("property", parmArray, allocator);
	// add the component values.
	Value compArray(kArrayType);
	Value objValue;
	objValue.SetObject();
	objValue.AddMember("sn", (Value(db.GetValue("RedStone", "IMEI").c_str(), allocator).Move()), allocator);
	objValue.AddMember("uid", (Value(db.GetValue("RedStone", "IMEI").c_str(), allocator).Move()), allocator);
	objValue.AddMember("componentType", "CELLULAR", allocator);
	objValue.AddMember("componentCode", "CM001", allocator);
	objValue.AddMember("manufacturerCode", "NA", allocator);
	objValue.AddMember("installTime", Value(instTime.c_str(), allocator).Move(), allocator);
	compArray.PushBack(objValue, allocator);
	Value satValue;
	satValue.SetObject();
	satValue.AddMember("sn", (Value(db.GetValue("Iridium", "IMEI").c_str(), allocator).Move()), allocator);
	satValue.AddMember("uid", (Value(db.GetValue("Iridium", "IMEI").c_str(), allocator).Move()), allocator);
	satValue.AddMember("componentType", "SATELLITE", allocator);
	satValue.AddMember("componentCode", "SAT001", allocator);
	satValue.AddMember("manufacturerCode", "NA", allocator);
	satValue.AddMember("installTime", Value(instTime.c_str(), allocator).Move(), allocator);
	compArray.PushBack(satValue, allocator);
	Value simValue;
	simValue.SetObject();
	simValue.AddMember("sn", (Value(db.GetValue("Cellular", "CCID").c_str(), allocator).Move()), allocator);
	simValue.AddMember("uid", (Value(db.GetValue("Cellular", "CCID").c_str(), allocator).Move()), allocator);
	simValue.AddMember("componentType", "SIMCARD", allocator);
	simValue.AddMember("componentCode", "SIM", allocator);
	simValue.AddMember("manufacturerCode", "NA", allocator);
	simValue.AddMember("installTime", Value(instTime.c_str(), allocator).Move(), allocator);
	compArray.PushBack(simValue, allocator);
	doc.AddMember("component", compArray, allocator);
}

//#define rapidString(a) StringRef(a.c_str(), a.size())

#define rapidString(a) (Value(a.c_str(), allocator).Copy() )

void CellSender::BuildParamArray(rapidjson::Value &parmArray, Document::AllocatorType &allocator, std::string propName, std::string propVal, std::string propType)
{
	Value objValue;
	objValue.SetObject();
	
	objValue.AddMember("propName", (Value(propName.c_str(), allocator).Move()), allocator);	
	objValue.AddMember("propValue",  (Value(propVal.c_str(), allocator).Move()), allocator);
	objValue.AddMember("propType", (Value(propType.c_str(), allocator).Move()), allocator);

	parmArray.PushBack(objValue, allocator);
}

//-------------------------------------------------------------------------------------------------
void CellSender::BuildComponentArray(rapidjson::Value &parmArray, Document::AllocatorType &allocator, std::string name, std::string value)
{
	Value objValue;
	objValue.SetObject();
	
	objValue.AddMember((Value(name.c_str(), allocator).Move()), (Value(value.c_str(), allocator).Move()), allocator);	
	parmArray.PushBack(objValue, allocator);
}

//-------------------------------------------------------------------------------------------------
// Source: iNet v7.7 Core Server Functional Specification - Page 19
//
//		(CS-2060) This endpoint is used to download schedures for the equipment that are controlled by
//		users via iNet Control.
//
//		HTTP Request
//		GET https://inetsync.indsci.com/iNetAPI/v1/equipment/{sn}/schedules
//		Content-Type: application/json
//
// returns: 0 - OK
//					-1 - failed iNet call
//          -2 - failed to build Parameter header???  should not happen.
//					-3 - unexpected HTTP response
//
int CellSender::inetDownloadEventSchedules()
{
	ats::StringList headers;
	Document doc;
	std::string str;
	std::string firmwareUpdateTime_str;
	
	const std::string stringCreateSch = "CREATE";
	std::string schVersion_str;
	std::string prevSchVersion;
	std::string firVersion_str;
	ats::String prevFirmVer;

	time_t firmwareUpdateTime = 0;
	int r;
	if ((r = BuildParamHeader(headers)) != 0)
		return -2;

	// push to iNet servers
	int http_code, curlError;
	std::string url = g_pConfig->iNetURL()	 + "/iNetAPI/v1/equipment/" + g_pConfig->SerialNum() + "/schedules";
	ats_logf(ATSLOG_DEBUG, GREEN_ON "inetDownloadEventSchedules " MAGENTA_ON "url %s" RESET_COLOR, url.c_str());
	ats::String ret = GetCurlRequest(url, headers, http_code, curlError);
	ats_logf(ATSLOG_DEBUG, YELLOW_ON "%s,%d: http status code %d.\nresult: %s" RESET_COLOR, __FILE__,__LINE__, http_code, ret.c_str());
		
	if (!ret.empty() && (http_code == 200 || http_code == 201 ))
	{
		doc.Parse(ret.c_str(),ret.length());
		if(doc.HasParseError())
		{
			RegisterSchParserError(true);
		}

		if(!doc.IsObject())
		{
			RegisterSchParserError(true);
			return -1;
		}

		if( !doc.HasMember("schedulesVersion") || !doc["schedulesVersion"].IsString() )
		{
			RegisterSchParserError(true);
			return -1;
		}

		RegisterSchParserError(false);

		db_monitor::ConfigDB db;
		
		prevSchVersion = db.GetValue("isc-lens", "SchVer", "0000-00-00T0:0:0.00-0000");
		prevFirmVer = db.GetValue("isc-lens", "FWVer", "0.0.0.0");

		str=doc["schedulesVersion"].GetString();
		schVersion_str = str;
		if(schVersion_str > prevSchVersion)
		{
			ats_logf(ATSLOG_DEBUG, "%s,%d: " GREEN_ON "%s:%s\n" RESET_COLOR, __FILE__, __LINE__,"schedulesVersion",schVersion_str.c_str());
			ats_logf(ATSLOG_ERROR, RED_ON "SettingsVersion is triggering a DownloadSettings" RESET_COLOR);
		
			if( ( !doc.HasMember("eventScheduleChanges") ) || (!doc["eventScheduleChanges"].IsArray() ) )
				return -1;

			const Value& evtSchCh = doc["eventScheduleChanges"];
			for(Value::ConstValueIterator iter = evtSchCh.Begin(); iter != evtSchCh.End(); ++iter)
			{
				const Value& jObj = *iter;

				if(jObj.HasMember("changeType"))
				{
					str = jObj["changeType"].GetString();
					if (str == stringCreateSch)
					{

						if( jObj.HasMember("eventSchedule") && jObj["eventSchedule"].IsObject() )
						{
							jObj["eventSchedule"].HasMember("referenceID");
							Value::ConstMemberIterator itrEntry = jObj["eventSchedule"].FindMember("referenceID");
							if(doc.MemberBegin() != itrEntry)
							{
								ats_logf(ATSLOG_DEBUG, "%s,%d: " GREEN_ON "%s:%lld\n" RESET_COLOR, __FILE__, __LINE__,itrEntry->name.GetString(),itrEntry->value.GetUint64());
								// Extract RefrenceID Here
							}

							jObj["eventSchedule"].HasMember("eventType");
							itrEntry = jObj["eventSchedule"].FindMember("eventType");
							if(doc.MemberBegin() != itrEntry)
							{
								str = itrEntry->value.GetString();
								ats_logf(ATSLOG_DEBUG, "%s,%d: " GREEN_ON "%s:%s\n" RESET_COLOR, __FILE__, __LINE__,itrEntry->name.GetString(),str.c_str());
								// Extract EventType Here
							}

							jObj["eventSchedule"].HasMember("equipmentType");
							itrEntry = jObj["eventSchedule"].FindMember("equipmentType");
							if(doc.MemberBegin() != itrEntry)
							{
								str = itrEntry->value.GetString();
								ats_logf(ATSLOG_DEBUG, "%s,%d: " GREEN_ON "%s:%s\n" RESET_COLOR, __FILE__, __LINE__,itrEntry->name.GetString(),str.c_str());
								// Extract EquipmentType Here
							}

							jObj["eventSchedule"].HasMember("equipmentSerialNumbers");
							itrEntry = jObj["eventSchedule"].FindMember("equipmentSerialNumbers");
							if(doc.MemberBegin() != itrEntry)
							{
								if(itrEntry->value.IsArray())
								{
									for(Value::ConstValueIterator iter = itrEntry->value.Begin(); iter != itrEntry->value.End(); ++iter)
									{
										ats_logf(ATSLOG_DEBUG, "%s,%d: " GREEN_ON "%s:%s\n" RESET_COLOR, __FILE__, __LINE__,itrEntry->name.GetString(),iter->GetString());
									}

								}
								else
								{
									str = itrEntry->value[0].GetString();
									ats_logf(ATSLOG_DEBUG, "%s,%d: " GREEN_ON "%s:%s\n" RESET_COLOR, __FILE__, __LINE__,itrEntry->name.GetString(),str.c_str());
								}
								// Extract Equipment Serial Numbers Here
							}
							jObj["eventSchedule"].HasMember("schedule");
							itrEntry = jObj["eventSchedule"].FindMember("schedule");
							if(doc.MemberBegin() != itrEntry)
							{
								const Value& val= itrEntry->value;
								if(val.HasMember("type"))
								{
									str = val["type"].GetString();
									ats_logf(ATSLOG_DEBUG, "%s,%d: " GREEN_ON "%s:%s\n" RESET_COLOR, __FILE__, __LINE__,"type",str.c_str());
								}
								if(val.HasMember("runAfter"))
								{
									str = val["runAfter"].GetString();
									firmwareUpdateTime_str = str;
									ats_logf(ATSLOG_DEBUG, "%s,%d: " GREEN_ON "%s:%s\n" RESET_COLOR, __FILE__, __LINE__,"runAfter",firmwareUpdateTime_str.c_str());
									firmwareUpdateTime = ConvertTimeZoneToUnix(firmwareUpdateTime_str);
								/*
									timeDiff = firmwareUpdateTime - time(NULL);
									ats_logf(ATSLOG_DEBUG, "%s,%d: " GREEN_ON "%s:%lu\n" RESET_COLOR, __FILE__, __LINE__,"runAfterSeconds",timeDiff);
						
									std::string tstr= "2019-12-19T14:0:0.00-0500";
									timeDiff = (ConvertTimeZoneToUnix(tstr) - time(NULL));
									ats_logf(ATSLOG_DEBUG, "%s,%d: " GREEN_ON "%s:%lu\n" RESET_COLOR, __FILE__, __LINE__,"Test",timeDiff);
								*/
								}
							}

							jObj["eventSchedule"].HasMember("properties");
							itrEntry = jObj["eventSchedule"].FindMember("properties");
							for(Value::ConstValueIterator iter = itrEntry->value.Begin(); iter != itrEntry->value.End(); ++iter)
							{
								const Value& member = *iter;
								if(member.HasMember("name"))
								{
									str = member["name"].GetString();
									ats_logf(ATSLOG_DEBUG, "%s,%d: " GREEN_ON "%s:%s\n" RESET_COLOR, __FILE__, __LINE__,"name",str.c_str());
								}
								if(member.HasMember("value"))
								{
									str = member["value"].GetString();
									firVersion_str = str;
									ats_logf(ATSLOG_DEBUG, "%s,%d: " GREEN_ON "%s:%s\n" RESET_COLOR, __FILE__, __LINE__,"value",firVersion_str.c_str());
								}
							}
							jObj["eventSchedule"].HasMember("enabled");
							itrEntry = jObj["eventSchedule"].FindMember("enabled");
							jObj["eventSchedule"].HasMember("enabled");
							itrEntry = jObj["eventSchedule"].FindMember("enabled");
							if(doc.MemberBegin() != itrEntry)
							{
								if(itrEntry->value.IsBool())
								{
									ats_logf(ATSLOG_DEBUG, "%s,%d: " GREEN_ON "%s:%d" RESET_COLOR, __FILE__, __LINE__,itrEntry->name.GetString(),itrEntry->value.GetBool());
								}
							}
						}
					}	
				
				}
			}
			ats_logf(ATSLOG_DEBUG, "%s,%d: " GREEN_ON "Prev FW:%s\n" RESET_COLOR, __FILE__, __LINE__,prevFirmVer.c_str());
			if(firVersion_str > prevFirmVer)
			{
				ats_logf(ATSLOG_DEBUG, "%s,%d: " GREEN_ON "New FW:%s\n" RESET_COLOR, __FILE__, __LINE__,firVersion_str.c_str());
				m_RedStone.SetFirmwareUpdateTime(firmwareUpdateTime);
				ats_logf(ATSLOG_DEBUG, "%s,%d: " GREEN_ON "New FW Sch At:%lu Seconds\n" RESET_COLOR, __FILE__, __LINE__,firmwareUpdateTime);
				m_RedStone.SetFirmwareUpdateStatus(true);
			}
			else
			{
				db.Set("isc-lens", "SchVer", doc["schedulesVersion"].GetString());
			}
		}
	}
	else if(http_code == 400)
	{
		return -1;
	}
	else
	{
		ats_logf(ATSLOG_ERROR, RED_ON "inetDownloadEventSchedules - unexpected error code (%d) returned by iNet. Response is:" RESET_COLOR, http_code);		
		return -3;
	}
	return 0;
}

time_t CellSender::ConvertTimeZoneToUnix(string & ISO8601String)
{
	char * ptr;
	struct tm time;
	int timeZoneOffsetInSec = 0;
	memset(&time, 0, sizeof(struct tm));
	tzset();

	ats_logf(ATSLOG_DEBUG, "%s,%d: " GREEN_ON "Time Zone Offset:%lu\n" RESET_COLOR, __FILE__, __LINE__,timezone);
	ptr = strptime(ISO8601String.c_str(),"%FT%T",&time);
	if(ptr)
	{
		char *ptrZoneStart = 0;
		ptrZoneStart = strchr(ptr,'+');
		if(!ptrZoneStart)
		{
			ptrZoneStart = strchr(ptr,'-');
		}
		if(ptrZoneStart)
		{
			ptr = ptrZoneStart;
			ptr = strptime(ptr,"%z",&time);
			timeZoneOffsetInSec = timezone;
			timeZoneOffsetInSec += time.tm_gmtoff;
			time.tm_gmtoff = 0;
		}
	}
	return (mktime(&time) - timeZoneOffsetInSec);
}

//-------------------------------------------------------------------------------------------------
int CellSender::inetUploadError(message_info* p_mi)
{
	std::string errorVal = ats::from_hex(p_mi->sm.get("usr_msg_data").c_str());
	ats_logf(ATSLOG_DEBUG, GREEN_ON "inetUploadError " MAGENTA_ON "usr_msg_data %s" RESET_COLOR, errorVal.c_str());
	std::string errTime = errorVal;
	std::string errString;
	errorVal.erase(errorVal.find_first_of(","));
	errTime.erase(0, errTime.find_first_of(",") + 1);
	errString = errTime;  
	errString.erase(0, errString.find_first_of(",") + 1);
	errTime.erase(errTime.find_last_of(","));
ATS_TRACE;
	ats::StringList headers;
	int r;
	if ((r = BuildJSONHeader(headers)) != 0)
		return r;
ATS_TRACE;

	Document doc;
	Document::AllocatorType &allocator = doc.GetAllocator();
	doc.SetObject();
ATS_TRACE;
	AFS_Timer t;
	t.SetTime();
	doc.AddMember("time", Value(t.GetTimestampWithOS().c_str(), allocator).Move(), allocator);
	doc.AddMember("errorCode", Value(errorVal.c_str(), allocator).Move(), allocator);
  doc.AddMember("errorDetail", Value(errString.c_str(), allocator).Move(), allocator);
	doc.AddMember("errorTime", Value(errTime.c_str(), allocator).Move(), allocator);
ATS_TRACE;
	// push to iNet servers
	int http_code, curlError;
	std::string url = g_pConfig->iNetURL() + "/iNetAPI/v1/equipment/" + g_pConfig->SerialNum() + "/error";
	ats_logf(ATSLOG_DEBUG, GREEN_ON "inetUploadError " MAGENTA_ON "url %s" RESET_COLOR, url.c_str());
	StringBuffer buffer;
	Writer<StringBuffer> writer(buffer);
	doc.Accept(writer);
	std::string strData = buffer.GetString();
	ats_logf(ATSLOG_DEBUG, GREEN_ON "inetUploadError data %s" RESET_COLOR, strData.c_str());
	ats::String ret = PostCurlRequest(url, strData, headers, http_code, curlError);
	ats_logf(ATSLOG_DEBUG, YELLOW_ON "%s,%d: result: %s,%d" RESET_COLOR, __FILE__,__LINE__, ret.c_str(),http_code);
		
	if (!ret.empty() && (http_code == 200 || http_code == 201 ))
	{
		g_INetIPC.GatewayObjectID(StrToLL(ret));
		ats_logf(ATSLOG_INFO, "%s,%d: inetUploadError response is ## %s ##, http status code %d Object ID is %llu", __FILE__, __LINE__, ret.c_str(), http_code, (long long unsigned)g_INetIPC.GatewayObjectID());
		g_INetIPC.INetConnected(true);
		return 0;
	}

	if (http_code < 200 || http_code > 299)
	{
		if(http_code == 401)
			ats_logf(ATSLOG_ERROR, "%s,%d: " RED_ON "Failed to inetUploadEquipment - SN %s is not recognized by iNet, status code: %d" RESET_COLOR, __FILE__, __LINE__, g_pConfig->SerialNum().c_str(), http_code);
		else
			ats_logf(ATSLOG_ERROR, "%s,%d:" RED_ON " Failed to inetUploadEquipment, status code: %d" RESET_COLOR, __FILE__, __LINE__, http_code);

		if (curlError != 0)
		{
			// ISCP-339 g_INetIPC.INetConnected(false);
		}

		inetGetAuthentication(); // get new authentication
		return -1;
	}

		if (curlError != 0)
		{
			return -1; //ISCP-339
		}
	return 0;
}

//-------------------------------------------------------------------------------------------------
void CellSender::TestIridiumCreateGateway()
{
ATS_TRACE;
	SatData90_IDGateway sd_msg;
	NMEA_Client nmea;
	NMEA_DATA GPSdata = nmea.GetData();

	LensMAC mac = g_INetIPC.GetLensRegisters().GetMAC();
	sd_msg.SetData(GPSdata, mac, g_pConfig->SerialNum(), g_pConfig->SiteName(), g_pConfig->INetIndex(), g_INetIPC.RunState());
	char sdbBuf[128];
	int len;
	len = sd_msg.Encode(sdbBuf);
	// create a header and wrap the data
	char msgBuf[256];
	msgBuf[0] =  0x01;
	msgBuf[3] =  0x01;
	short v = 28;
	msgBuf[4] =  v / 256;	msgBuf[5] =  v % 256;
	testID++;
	memcpy(&msgBuf[6], &testID, 4);

	db_monitor::ConfigDB db;
	std::string imei = db.GetValue("Iridium", "IMEI");
	memcpy(&msgBuf[10], imei.c_str(), 15);
	msgBuf[25] = 0;
	v = (short)testID;
	memcpy(&msgBuf[26], &v, 2);
	memcpy(&msgBuf[28], &v, 2);

	time_t curTime;
	time(&curTime);
	long t = (long)curTime;
	memcpy(&msgBuf[30], &t, 4);
	// now add the SBD payload
	msgBuf[34] = 0x02;
	msgBuf[35] =  len / 256;	msgBuf[36] =  len % 256;
	memcpy(&msgBuf[37], sdbBuf, len);

	// now reset the overall length
	v = 34 + len;
	msgBuf[1] =  v / 256;	msgBuf[2] =  v % 256;

	// open socket to server
  try
  {
		Socket client ;    // Create the socket
		client.create();

		if (client.connect("67.212.66.103", 38050))
		{
			ats_logf(ATSLOG_ERROR, "Encoded length %d  (%s)", len+37, ats::to_hex(msgBuf, 37 + len).c_str() );
			client.send(msgBuf, 37 + len);	// send data
//			client.send(msgBuf, 28);	// send data
		}
		else
			printf("Failed to connect to port.\n");
	}
  catch ( SocketException& e )
	{
		ats_logf(ATSLOG_ERROR,  "%s:Exception was caught: %s  errno=%d", __FUNCTION__, e.description().c_str(), errno);
	}	
}
//-------------------------------------------------------------------------------------------------
void CellSender::TestIridiumUpdateGateway()
{
ATS_TRACE;
	SatData96_UpdateGateway sd_msg;
	NMEA_Client nmea;
	NMEA_DATA GPSdata = nmea.GetData();

	LensMAC mac = g_INetIPC.GetLensRegisters().GetMAC();
	sd_msg.SetData(GPSdata, mac, g_INetIPC.RunState());
	char sdbBuf[128];
	int len;
	len = sd_msg.Encode(sdbBuf);
	// create a header and wrap the data
	char msgBuf[256];
	msgBuf[0] =  0x01;
	msgBuf[3] =  0x01;
	short v = 28;
	msgBuf[4] =  v / 256;	msgBuf[5] =  v % 256;
	testID++;
	memcpy(&msgBuf[6], &testID, 4);

	db_monitor::ConfigDB db;
	std::string imei = db.GetValue("Iridium", "IMEI");
	memcpy(&msgBuf[10], imei.c_str(), 15);
	msgBuf[25] = 0;
	v = (short)testID;
	memcpy(&msgBuf[26], &v, 2);
	memcpy(&msgBuf[28], &v, 2);

	time_t curTime;
	time(&curTime);
	long t = (long)curTime;
	memcpy(&msgBuf[30], &t, 4);
	// now add the SBD payload
	msgBuf[34] = 0x02;
	msgBuf[35] =  len / 256;	msgBuf[36] =  len % 256;
	memcpy(&msgBuf[37], sdbBuf, len);

	// now reset the overall length
	v = 34 + len;
	msgBuf[1] =  v / 256;	msgBuf[2] =  v % 256;

	// open socket to server
  try
  {
		Socket client ;    // Create the socket
		client.create();

		if (client.connect("67.212.66.103", 38050))
		{
			ats_logf(ATSLOG_ERROR, "Encoded length %d  (%s)", len+37, ats::to_hex(msgBuf, 37 + len).c_str() );
			client.send(msgBuf, 37 + len);	// send data
//			client.send(msgBuf, 28);	// send data
		}
		else
			printf("Failed to connect to port.\n");
	}
  catch ( SocketException& e )
	{
		ats_logf(ATSLOG_ERROR,  "%s:Exception was caught: %s  errno=%d", __FUNCTION__, e.description().c_str(), errno);
	}	
}

void CellSender::RegisterCurlError(bool setError)
{
	AFS_Timer t;
	std::string user_data;
	static bool curlError = false;
	if(setError)
	{
		if(!curlError)
		{
			t.SetTime();
			user_data = "1003," + t.GetTimestampWithOS() + ", TGX Curl Error";
			ats_logf( ATSLOG_DEBUG, RED_ON "%s,%d: : TGX Curl Error Logged:%s\n" RESET_COLOR,__FILE__, __LINE__, user_data.c_str() );
			user_data = ats::to_hex(user_data);
			send_redstone_ud_msg("message-assembler", 0, "msg inet_error msg_priority=9 usr_msg_data=%s\r", user_data.c_str());
			curlError = true;
		}
	}
	else
	{
		curlError = false;
	}

}
void CellSender::RegisterHTTPError(bool setError)
{
	AFS_Timer t;
	std::string user_data;
	static bool httpError = false;
	if(setError)
	{
		if(!httpError)
		{
			t.SetTime();
			user_data = "980," + t.GetTimestampWithOS() + ", TGX HTTP Error";
			ats_logf( ATSLOG_DEBUG, RED_ON "%s,%d: : TGX HTTP Error Logged:%s\n" RESET_COLOR,__FILE__, __LINE__, user_data.c_str() );
			user_data = ats::to_hex(user_data);
			send_redstone_ud_msg("message-assembler", 0, "msg inet_error msg_priority=9 usr_msg_data=%s\r", user_data.c_str());
			httpError = true;
		}
	}
	else
	{
		httpError = false;
	}
}

void CellSender::RegisterAuthError(bool setError)
{
	AFS_Timer t;
	std::string user_data;
	static bool authError = false;
	static int retryCount = 2;
	if(setError)
	{

		--retryCount;
		if( (!authError) && (0 > retryCount) )
		{
			t.SetTime();
			user_data = "1004," + t.GetTimestampWithOS() + ", TGX Auth Failed Error";
			ats_logf( ATSLOG_DEBUG, RED_ON "%s,%d: : TGX Auth Failed Error Logged:%s\n" RESET_COLOR,__FILE__, __LINE__, user_data.c_str() );
			user_data = ats::to_hex(user_data);
			send_redstone_ud_msg("message-assembler", 0, "msg inet_error msg_priority=9 usr_msg_data=%s\r", user_data.c_str());
			authError = true;
		}
	}
	else
	{
		retryCount = 2;
		authError = false;
	}
}

void CellSender::RegisterSchParserError(bool setError)
{
	AFS_Timer t;
	std::string user_data;
	static bool schError = false;

	if(setError)
	{

		if( !schError )
		{
			t.SetTime();
			user_data = "987," + t.GetTimestampWithOS() + ", TGX Schedule Parser Failed Error";
			ats_logf( ATSLOG_DEBUG, RED_ON "%s,%d: : TGX Schedule Parser Error Logged:%s\n" RESET_COLOR,__FILE__, __LINE__, user_data.c_str() );
			user_data = ats::to_hex(user_data);
			send_redstone_ud_msg("message-assembler", 0, "msg inet_error msg_priority=9 usr_msg_data=%s\r", user_data.c_str());
			schError = true;
		}
	}
	else
	{
		schError = false;
	}

}

void CellSender::RegisterSettingsParserError(bool setError)
{
	AFS_Timer t;
	std::string user_data;
	static bool settingsError = false;
	
	if(setError)
	{
		if( !settingsError )
		{
			t.SetTime();
			user_data = "986," + t.GetTimestampWithOS() + ", TGX Settings Parser Failed Error";
			ats_logf( ATSLOG_DEBUG, RED_ON "%s,%d: : TGX Settings Parser Error Logged:%s\n" RESET_COLOR,__FILE__, __LINE__, user_data.c_str() );
			user_data = ats::to_hex(user_data);
			send_redstone_ud_msg("message-assembler", 0, "msg inet_error msg_priority=9 usr_msg_data=%s\r", user_data.c_str());
			settingsError = true;
		}
	}
	else
	{
		settingsError = false;
	}

}
