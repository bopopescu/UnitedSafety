#pragma once
#include <list>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include "NMEA_Client.h"
#include "ats-string.h"

using namespace rapidjson;

// support max 8 sensors
enum inetStatus
{
	_normal = 0x00,
	_sensorAlarm = 0x01,
	_panicAlarm = 0x02,
	_shutdown = 0x04,
	_mandownAlarm = 0x08,
	_pumpFault = 0x10,
	_lowBattery = 0x20,
	_remote = 0x40,
	_charging = 0x80
};

enum liveSensorStatus
{
	normal = 0x00,
	lowAlarm = 0x01,
	highAlarm = 0x02,
	negativeOverRange = 0x03,
	overRange = 0x04,
	calibrationFault = 0x05,
	zeroFault = 0x06,
	userDisabled = 0x08,
	bumpFault = 0x09,
	calibrationOverdue = 0x0b,
	dataFail = 0x0d,
	TWAAlarm = 0x0e,
	STELAlarm = 0x0f,
};

class gpsPosition
{
public:
	gpsPosition()
    : latitude(0.0f)
    , longitude(0.0f)
    , accuracy(0.0f)
    , bearing(0.0f)
    , speed(0.0f)
	{}
	gpsPosition(double _lat
			, double _lon
			, float _accuracy
			, float _bearing
			, float _speed)
		: latitude(_lat)
    , longitude(_lon)
    , accuracy(_accuracy)
    , bearing(_bearing)
    , speed(_speed)
	{ }
	double latitude;
	double longitude;
	float accuracy;
	float bearing;
	float speed;
};

struct liveSensor
{
	ats::String uid;
	ats::String componentCode;
	ats::String gasCode;
	ats::String gasReading;
	int uom;
	liveSensorStatus status;
};

// inet live event upload is deprecated in inet 7.7
// use create instrument state and update instrument state instead
typedef std::list<liveSensor> liveSensorList;
class INetLiveMessage
{
public:
	INetLiveMessage(const ats::String& _mac
			, const ats::String& _sn
			, char _eventSeq
			, char _status
			, const liveSensorList& _sensorList, const ats::String& _name, const ats::String& _site)
		: mac(_mac)
		, sn(_sn)
		, eventSequence(_eventSeq)
		, status(_status)
		, user(_name)
		, site(_site)
		, sensorList(_sensorList)
	{}
	ats::String mac;
	ats::String devicePosition;
	float deviceBattLevel;
	ats::String sn;
	ats::String time;
	short eventSequence;
	char status;
	ats::String equipmentCode;
	ats::String position;
	ats::String user;
	ats::String site;
	liveSensorList sensorList;
};

class GatewayState
{
public:
		GatewayState(inetStatus _status, const gpsPosition& _position, float _bl = 0.0, int _rssi = 0, const ats::String& _siteName = "")
			: status(_status)
			, position(_position)
			, batteryLevel(_bl)
			, rssi(_rssi)
			, siteName(_siteName)
		{
		}
		inetStatus status; // integer status
		gpsPosition position;
		float batteryLevel;
		int rssi;
		string siteName;
};

class InstrumentState
{
public:
		InstrumentState(const ats::String& _sn,
				ats::String& _timeStamp,
				short _sequence,
				inetStatus _status,
				const liveSensorList& _sensorList,
				const gpsPosition& _position,
				const ats::String& _user = "",
				const ats::String& _siteiName = "")
			: sn(_sn)
			, timeStamp(_timeStamp)
			, sequence(_sequence)
			, status(_status)
			, position(_position)
			, user(_user)
			, site(_siteiName)
			, sensorList(_sensorList)
			{}

		ats::String sn;
		ats::String timeStamp;
		short sequence;
		inetStatus status;
		gpsPosition position;
		ats::String user;
		ats::String site;
		liveSensorList sensorList;
};

class ExchangeStatusInformation
{
	public:
		ExchangeStatusInformation();
};

class messageFormatter
{
public:
	messageFormatter();
	~messageFormatter();
	ats::String fromData();
	int createData(const INetLiveMessage&, ats::String&);
	int createGateWayState(const GatewayState& gatewayState, ats::String& result);
	int createInstrumentState(const InstrumentState& instrumentState, ats::String& result);
	int LiveInstrumentCreate(const ats::String & sn, ats::String &result);
	void getUsrDataGPSPosition(ats::String& gpsStr);
protected:
	ats::String readImei();
	ats::String getTimestamp();
	ats::String toHex(const Document& dom);
private:
	static const ats::String defaultJSON;
	static const ats::String defaultGPS;
	static const ats::String defaultSensor;
	NMEA_Client nmea_client;
};
