#pragma once

#include "SatData.h"
#include "../isc-lens/InstrumentIdentifySensor.h"

// This defines the Create Instrument message.
// See ISC_Iridium_Specification Rev1_4 IIS-18 for definition.
//
// This is the XML definition
/*
<?xml version="1.0" encoding="UTF-8"?>
<event id="92" name="CreateInstrument">

	<data>
		<unsignedShort name="event_id"/>
		<unsignedByte name="seqnum"/>
		<unsignedShort name="vehicle_uid"/>
		<DateTime name="datetime" units="UTC"/>
		<latitude name="latitude"/>
		<longitude name="longitude"/>
		<altitude name="altitude" units="meters"/>
		<unsignedShort name="speed" units="kph"/>
		<tenthsUShort name="COG" units="degrees"/>
		<tenthsUShort name="HDOP" units="degrees"/>
		<ShortMAC name="MAC"/>
		<unsignedByte name="AlarmDetail"/>
		<unsignedByte name="NumSensors"/>
		<Sensor id=0>
			<unsignedByte name="ComponentCode"/>
			<unsignedByte name="GasCode"/>
			<unsignedByte name="MeasurementUnits"/>
			<unsignedShort name="Reading"/>
			<unsignedByte name="GasAlarm"/>
		<Sensor/>
		...
		<Sensor id=(NumSensors - 1)></Sensor>
	</data>
</event>
*/

// This is the packet of what is actually in the CreateInstrument JSON
class SensorData
{
public:
	int cc;
	int gc;
	int nDec;
	short gr;
	int uom;
	int s;
};

class SatData92_CreateInstrument : public SatData
{
private:
		LensMAC m_MAC;
		unsigned char m_nSensors;
		unsigned char m_status;
		SensorData m_Sensors[8];
		std::string m_siteName;
		std::string m_userName;	
public:
	SatData92_CreateInstrument()
	{
		m_Type = SMT_CREATE_INSTRUMENT;
	}


	void SetData
	(
		NMEA_DATA gps,
		LensMAC MAC,
		unsigned char numSensors,
		unsigned char status
	)
	{
		SetGPS(gps);
		m_MAC        = MAC;
		m_nSensors   = numSensors;
		m_status     = (unsigned char)status;
	}
	

	//--------------------------------------------------------------------------------------------------------
	// json: params {"sn":"160318P-001","t":"2019-07-15T20:42:38","seq":3,"s":0,"p":{"lat":50.971395,"lon":-114.02337266666668,
	//								"a":0.9900000095367432,"s":0.293,"b":0.0},
	//								"sensors":[	{"cc":"S0005","gc":"G0020","gr":107.7,"uom":2,"s":65},
	//														{"cc":"S0008","gc":"G0026","gr":12555.0,"uom":3,"s":64},
	//														{"cc":"S0004","gc":"G0001","gr":0.0,"uom":1,"s":0},
	//														{"cc":"S0004","gc":"G0002","gr":-71.6,"uom":1,"s":65}],
	//														"site":"Aware360","user":"Dave"}	
	void SetData(LensMAC mac, NMEA_DATA &nmea, std::string json)
	{
		#define LOCBUF_SIZ	2048
		int bufSize = json.size() >= LOCBUF_SIZ ? (LOCBUF_SIZ-1) : json.size();
		char mybuf[LOCBUF_SIZ];
		strncpy(mybuf, json.c_str(), bufSize);	
		mybuf[bufSize] = '\0';
		rapidjson::Document doc;

		if (doc.Parse(mybuf).HasParseError())
		{
			ats_logf(ATSLOG_INFO, "%s,%d: Error message JSON has errors!", __FILE__, __LINE__);
			return;  // discard message if it can't be parsed
		}
		m_MAC 		= mac;
		std::string data;

		if( doc.HasMember("p") )
		{
			rapidjson::Value::ConstMemberIterator itrEntry = doc["p"].FindMember("lat");
			if(doc.MemberBegin() != itrEntry)
			{
				nmea.ddLat = itrEntry->value.GetDouble();
			}
			itrEntry = doc["p"].FindMember("lon");
			if(doc.MemberBegin() != itrEntry)
			{
				nmea.ddLon = itrEntry->value.GetDouble();
			}
			itrEntry = doc["p"].FindMember("a");
			if(doc.MemberBegin() != itrEntry)
			{
				nmea.hdop = itrEntry->value.GetDouble();
			}

			itrEntry = doc["p"].FindMember("s");
			if(doc.MemberBegin() != itrEntry)
			{
				//nmea.sog = itrEntry->value.GetDouble();
			}

			itrEntry = doc["p"].FindMember("b");
			if(doc.MemberBegin() != itrEntry)
			{
				//nmea.ddCOG = itrEntry->value.GetDouble();
			}
			nmea.gps_quality = 1;
			nmea.valid = true;
			nmea.SetTimeFromSystem();
		}

		SetGPS(nmea);  // required for all messages
		
		//Message 92 specific data from JSON
		
		//m_SeqNum 	= (unsigned char)doc["seq"].GetInt();  // don;t use this - just use the generated value to guarentee uniqueness.
		m_status 	= (unsigned char)doc["s"].GetInt();
		//... sensors?
		const rapidjson::Value& sensorArray = doc["sensors"]; 
		m_nSensors 	= (unsigned char)sensorArray.Size();

		for(int i = 0; i < m_nSensors; i++)
		{
			std::string gc, cc, gr;
			if (sensorArray[i].HasMember("cc") )cc = sensorArray[i]["cc"].GetString();
			if (sensorArray[i].HasMember("gc") )gc = sensorArray[i]["gc"].GetString();
			m_Sensors[i].cc = atoi(&(cc.c_str()[1]));
			m_Sensors[i].gc = atoi(&(gc.c_str()[1]));
			
			if ( sensorArray[i].HasMember("gr") )
			{
				if ( sensorArray[i]["gr"].IsInt() )
				{
					m_Sensors[i].gr = (short)sensorArray[i]["gr"].GetInt();
					m_Sensors[i].nDec = 0;
				}
				else if ( sensorArray[i]["gr"].IsDouble() )
				{
					m_Sensors[i].gr = (short)(sensorArray[i]["gr"].GetDouble() * 100);
					m_Sensors[i].nDec = 2;
				}
				else
				{
					m_Sensors[i].gr = 0;
					m_Sensors[i].nDec = 0;
					ats_logf(ATSLOG_ERROR, RED_ON "%s,%d: Gas reading is WRONG: val %d ndec %d" RESET_COLOR, __FILE__, __LINE__, m_Sensors[i].gr, m_Sensors[i].nDec);
				}
			}
			else
			{
				m_Sensors[i].gr = 0;
				m_Sensors[i].nDec = 0;
				ats_logf(ATSLOG_ERROR, RED_ON "%s,%d: Gas reading is NOT THERE!: val %d ndec %d" RESET_COLOR, __FILE__, __LINE__, m_Sensors[i].gr, m_Sensors[i].nDec);
			}


			if (doc.HasMember("uom") ) m_Sensors[i].uom = sensorArray[i]["uom"].GetInt();
			if (doc.HasMember("s") ) m_Sensors[i].s = sensorArray[i]["s"].GetInt();
		}
		if (doc.HasMember("user") && doc["user"].IsString())
		{
		 	m_userName = doc["user"].GetString();
		 	ats_logf(ATSLOG_INFO, "%s,%d: ISCP-304 Fix\n user:%s\n", __FILE__, __LINE__,m_userName.c_str());
		}
		else
		{
			ats_logf(ATSLOG_INFO, "%s,%d: ISCP-304 Fix for user information not found!", __FILE__, __LINE__);
		}

		if (doc.HasMember("site") && doc["site"].IsString())
		{
			m_siteName = doc["site"].GetString();
			ats_logf(ATSLOG_INFO, "%s,%d: ISCP-304 Fix\n site:%s\n", __FILE__, __LINE__,m_siteName.c_str());
		}
		else
		{
			ats_logf(ATSLOG_INFO, "%s,%d: ISCP-304 Fix site information not found!", __FILE__, __LINE__);	
		}		
	}
	
	//--------------------------------------------------------------------------------------------------------
	short	 Encode(char *buf)	// encode to buf - return length encoded
	{
		SatData::Encode(buf);	// will set idx for us
		EncodeMAC(buf, m_MAC);
		buf[idx++] 	= m_status;
		buf[idx++] 	= m_nSensors;

		for (short i = 0; i < m_nSensors; i++)
		{
			EncodeSensor(buf, m_Sensors[i]);
		}
		EncodeString(buf, m_siteName, 16);
		EncodeString(buf, m_userName, 16);
		return idx;
	}

	//--------------------------------------------------------------------------------------------------------
	short EncodeSensor(char *buf, SensorData &packet)
	{
		buf[idx++] 	= packet.cc;		//sensor type
		buf[idx++] 	= packet.gc;		//gas code
		buf[idx++] 	= packet.nDec;		//num dec in gas reading
		char swap[8];
		memcpy(swap, &packet.gr, 2);		// Two byte for gas reading need to be swapped
		buf[idx++] = swap[1];
		buf[idx++] = swap[0];
		buf[idx++] 	= packet.uom; 
		buf[idx++] 	= packet.s;
		return idx;
	}
};

