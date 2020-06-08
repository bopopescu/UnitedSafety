#pragma once

#include "SatData.h"


// This defines the Identify Gateway message.
//	The SetData function takes the current GPS and the STS_xxx type as inputs.
// See ISC_Iridium_Specification Rev1_4 IIS-16 for definition.
//
class SatData91_IDInstrument : public SatData
{
private:
		LensMAC m_MAC;
		std::string m_serialNum;
		std::string m_siteName;
		std::string m_userName;
		unsigned char m_iNetURLIdx;
		unsigned char m_status;
	
public:
	SatData91_IDInstrument()
	{
		m_Type = SMT_IDENTIFY_INSTRUMENT;
	}


	void SetData
	(
		NMEA_DATA gps,
		LensMAC MAC,
		std::string serialNum,
		std::string siteName,
		std::string userName,
		int iNetURLIdx,
		unsigned char status
	)
	{
		SetGPS(gps);
		m_MAC        = MAC;
		m_serialNum  = serialNum;
		m_siteName   = siteName;
		m_userName   = userName;
		m_iNetURLIdx = iNetURLIdx;
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
		char mybuf[LOCBUF_SIZ];
		int bufSize = json.size() >= LOCBUF_SIZ ? (LOCBUF_SIZ-1) : json.size();
		strncpy(mybuf, json.c_str(), bufSize);	
		mybuf[bufSize] = '\0';
		rapidjson::Document doc;

		if (doc.Parse(mybuf).HasParseError())
		{
			ats_logf(ATSLOG_INFO, "%s,%d: Error message JSON has errors!", __FILE__, __LINE__);
			return;  // discard message if it can't be parsed
		}
		m_MAC = mac;
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
		ats_logf(ATSLOG_INFO, "%s,%d: %s", __FILE__, __LINE__, __FUNCTION__);
		
		// Message 91 specific data from JSON
		if (doc.HasMember("site") && doc["site"].IsString())	m_siteName = doc["site"].GetString();
		if (doc.HasMember("user") && doc["user"].IsString()) m_userName = doc["user"].GetString();
		if (doc.HasMember("sn") && doc["sn"].IsString())m_serialNum = doc["sn"].GetString();
		if (doc.HasMember("s")) m_status = (unsigned char)doc["s"].GetInt();

		// INetIndex is from the db-config
		db_monitor::ConfigDB  db;
		m_iNetURLIdx = (unsigned char)db.GetInt("isc-lens", "INetIndex", 0);
	}

	short	 Encode(char *buf)	// encode to buf - return length encoded
	{
		SatData::Encode(buf);	// will set idx for us
		EncodeMAC(buf, m_MAC);
		EncodeString(buf, m_serialNum, 16);
		EncodeString(buf, m_siteName, 16);
		EncodeString(buf, m_userName, 16);
		buf[idx++] = m_iNetURLIdx;
		buf[idx++] = m_status;
		
		return idx;
	}
};
