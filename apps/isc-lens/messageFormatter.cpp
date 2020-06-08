#include <fstream>
#include <errno.h>
#include <sys/time.h>
#include <boost/algorithm/string.hpp>

#include "atslogger.h"

#include "messageFormatter.h"
#include "INetConfig.h"

extern INetConfig *g_pLensParms;  // read the db-config parameters


#define	DC_IMEI_DATA_FILE "/tmp/config/imei"
#define	DC_IMEI_DATA_FILE_ORIGIN "/mnt/nvram/rom/imei"
#define TIMESTAMP 26
#define rapidString(a) StringRef(a.c_str(), a.size())

static ats::String imei = ats::String();


void messageFormatter::getUsrDataGPSPosition(ats::String& gpsStr)
{
	ats::String fixTime;
	ats_sprintf(&fixTime, "%.4d-%.2d-%.2d %.2d:%.2d:%.2d", nmea_client.Year(), nmea_client.Month(),nmea_client.Day(),nmea_client.Hour(),nmea_client.Minute(),(short)(nmea_client.Seconds()));

	ats_sprintf(&gpsStr, "fix_time=\"%s\" latitude=%f longitude=%f satellites=%d fix_status=%d hdop=%f", fixTime.c_str(), nmea_client.Lat(), nmea_client.Lon(), nmea_client.NumSVs(), nmea_client.GPS_Quality(), nmea_client.HDOP());
}



const ats::String messageFormatter::defaultJSON("\
		{\
		\"device\": \"\",\
		\"sn\": \"\",\
		\"time\": \"\",\
		\"sequence\": 0,\
		\"status\": 0,\
		\"equipmentCode\": \"VPRO\",\
		\"devicePosition\": {\
		\"latitude\": 0.0,\
		\"longitude\": 0.0,\
		\"accuracy\": 0.0,\
		\"bearing\": 0.0,\
		\"speed\": 0.0 \
		},\
		\"sensors\": []\
		}\
		");

const ats::String messageFormatter::defaultGPS("\
		{\
		\"latitude\": 0.0,\
		\"longitude\": 0.0,\
		\"accuracy\": 0.0,\
		\"bearing\": 0.0,\
		\"speed\": 0.0,\
		}\
		");

const ats::String messageFormatter::defaultSensor("\
		{\
		\"uid\": \"\",\
		\"componentCode\": \"\",\
		\"gasCode\": \"\",\
		\"gasReading\": 0.0,\
		\"uom\": 0,\
		\"status\": 0,\
		}\
		");

messageFormatter::messageFormatter()
{
	if (imei.empty()) imei = readImei();
}

messageFormatter::~messageFormatter()
{
}

ats::String messageFormatter::fromData()
{
	return ats::String();
}

ats::String messageFormatter::getTimestamp()
{

	char s[TIMESTAMP + 1];
	struct timeval tv;
	struct tm *tm;
	gettimeofday(&tv, 0);
	tm = localtime(&tv.tv_sec);
	if (tm != NULL)
	{
		strftime(s, TIMESTAMP, "%Y-%m-%dT%H:%M:%S", tm);
	}

	return ats::String(s);
}

ats::String messageFormatter::readImei()
{
	ats::String imei;

	std::ifstream infile(DC_IMEI_DATA_FILE, std::ios_base::in);
	if(infile)
	{
		while(!infile.eof())
		{
			infile>>imei;
		}
	}
	if(imei.empty())
	{
	  std::ifstream infile(DC_IMEI_DATA_FILE_ORIGIN, std::ios_base::in);
	  if(infile)
	  {
		while(!infile.eof())
		{
		  infile>>imei;
		}
	  }
	}

	return imei;
}

// json format
//{
//  "type": "CreateGatewayState",
//  "payload": {
//    "s": 0,
//    "p": {
//      "lat": 51.0486,
//      "lon": -114.0708,
//      "a": 0.0,
//      "b": 0.0,
//      "s": 0.0
//    },
//    "bl": 0.0,
//    "r": 0,
//    "site": ""
//  }
//}
int messageFormatter::createGateWayState(const GatewayState& gatewayState, ats::String& result)
{
	Document dom;
	Document::AllocatorType &allocator = dom.GetAllocator();
	dom.SetObject();

	dom.AddMember("type", "CreateGatewayState", allocator);
	Value payload(kObjectType);

	payload.AddMember("s", gatewayState.status	, allocator);
	payload.AddMember("bl", gatewayState.batteryLevel, allocator);
	payload.AddMember("r", gatewayState.rssi, allocator);
	if(!gatewayState.siteName.empty())
	{
		payload.AddMember("site", rapidString(gatewayState.siteName), allocator);
	}

	if(nmea_client.Lat() > -90 && nmea_client.Lat() < 90 && nmea_client.Lon() > -180 && nmea_client.Lon() < 180)
	{
		Value gpsObj(kObjectType);
		gpsObj.AddMember("lat", nmea_client.Lat(), allocator);
		gpsObj.AddMember("lon", nmea_client.Lon(), allocator);
		gpsObj.AddMember("a", 2.0, allocator);
		gpsObj.AddMember("s", nmea_client.SOG(), allocator);
		payload.AddMember("p", gpsObj, allocator);
	}

	dom.AddMember("payload", payload, allocator);
	result = toHex(dom);
	return 0;
}

//json format
//{
//  "sn": "15072EK-003",
//  "t": "2018-10-23T13:53:59.000+0000",
//  "seq": 6,
//  "s": 0,
//  "user": "",
//  "site": "Worker",
//  "sensors": [
//    {
//      "cc": "S0005",
//      "gc": "G0020",
//      "gr": 20.90,
//      "uom": 2,
//      "s": 0
//    },
//    {
//      "cc": "S0004",
//      "gc": "G0002",
//      "gr": 0.00,
//      "uom": 1,
//      "s": 0
//    }
//  ]
//}
int messageFormatter::createInstrumentState(const InstrumentState& instrumentState, ats::String& result)
{
	Document dom;
	Document::AllocatorType &allocator = dom.GetAllocator();
	dom.SetObject();

	dom.AddMember("type", "CreateInstrumentState", allocator);
	Value payload(kObjectType);

	payload.AddMember("sn", rapidString(instrumentState.sn), allocator);
	payload.AddMember("t", rapidString(instrumentState.timeStamp), allocator);
	payload.AddMember("seq", instrumentState.sequence, allocator);
	payload.AddMember("s", instrumentState.status, allocator);
	if(!instrumentState.user.empty())
	{
		payload.AddMember("user", rapidString(instrumentState.user), allocator);
	}
	if(!instrumentState.site.empty())
	{
		payload.AddMember("site", rapidString(instrumentState.site), allocator);
	}

	if(nmea_client.Lat() > -90 && nmea_client.Lat() < 90 && nmea_client.Lon() > -180 && nmea_client.Lon() < 180)
	{
		Value gpsObj(kObjectType);
		gpsObj.AddMember("lat", nmea_client.Lat(), allocator);
		gpsObj.AddMember("lon", nmea_client.Lon(), allocator);
		gpsObj.AddMember("a", 2.0, allocator);
		gpsObj.AddMember("s", nmea_client.SOG(), allocator);
		payload.AddMember("p", gpsObj, allocator);
	}

	dom.AddMember("payload", payload, allocator);
	result = toHex(dom);
	return 0;
}

ats::String messageFormatter::toHex(const Document& dom)
{
	StringBuffer buffer;
	Writer<StringBuffer> writer(buffer);
	dom.Accept(writer);
	ats::String rs = buffer.GetString();
	boost::replace_all(rs, "\"$$", "");
	boost::replace_all(rs, "$$\"", "");
	ats_logf(ATSLOG_DEBUG, "%s, %d: %s", __FILE__, __LINE__, rs.c_str());
	return ats::to_hex(rs);
}

int messageFormatter::createData(const INetLiveMessage& data, ats::String& result)
{
	Document dom;
	Document::AllocatorType &allocator = dom.GetAllocator();
	dom.SetObject();
	ats::String ts = getTimestamp();

	dom.AddMember("sn", rapidString(data.sn), allocator);
	dom.AddMember("device", rapidString(imei), allocator);
	dom.AddMember("status", data.status, allocator);
	dom.AddMember("sequence", data.eventSequence, allocator);
	dom.AddMember("equipmentCode", "VPRO", allocator);
	dom.AddMember("time", rapidString(ts), allocator);
	if(!data.user.empty())
	{
		dom.AddMember("user", rapidString(data.user), allocator);
	}
	if(!data.site.empty())
	{
		dom.AddMember("site", rapidString(data.site), allocator);
	}

	Value sensorArray(kArrayType);
	std::list<liveSensor>::const_iterator it = data.sensorList.begin();
	for(; it != data.sensorList.end(); ++it)
	{
		Value sensorObj(kObjectType);
		const liveSensor& s = (*it);
		if (!s.uid.empty())
		{
			sensorObj.AddMember("uid", rapidString(s.uid), allocator);
		}
		if (!s.componentCode.empty())
		{
			sensorObj.AddMember("componentCode", rapidString(s.componentCode), allocator);
		}
		sensorObj.AddMember("gasCode", rapidString(s.gasCode), allocator);
		sensorObj.AddMember("gasReading", rapidString(s.gasReading), allocator);
		sensorObj.AddMember("uom", s.uom, allocator);
		sensorObj.AddMember("status", s.status, allocator);
		sensorArray.PushBack(sensorObj, allocator);
	}
	dom.AddMember("sensors", sensorArray, allocator);

	if(nmea_client.Lat() > -90 && nmea_client.Lat() < 90 && nmea_client.Lon() > -180 && nmea_client.Lon() < 180)
	{
		Value gpsObj(kObjectType);
		gpsObj.AddMember("latitude", nmea_client.Lat(), allocator);
		gpsObj.AddMember("longitude", nmea_client.Lon(), allocator);
		gpsObj.AddMember("accuracy", 2.0, allocator);
		gpsObj.AddMember("speed", nmea_client.SOG(), allocator);
		dom.AddMember("devicePosition", gpsObj, allocator);
	}

	result = toHex(dom);
	return 0;
}

int messageFormatter::LiveInstrumentCreate(const ats::String & sn, ats::String &result)
{
	Document dom;
	Document::AllocatorType &allocator = dom.GetAllocator();
	dom.SetObject();

	char buf[128];
	sprintf(buf, "https://inetsync.indsci.com/iNetAPIO/v1/live/%s/register", sn.c_str());
	ats::String url = buf; 
	dom.AddMember("url", rapidString(url), allocator);
	result = toHex(dom);
	ats_logf(ATSLOG_INFO, "%s,%d: usr_msg_data=%s\r", __FILE__, __LINE__, result.c_str());
	return 0;
}

