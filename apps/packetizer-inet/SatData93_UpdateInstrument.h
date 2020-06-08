#pragma once

#include "SatData.h"


// This defines the Identify Gateway message.
//	The SetData function takes the current GPS and the STS_xxx type as inputs.
// See ISC_Iridium_Specification Rev1_4 IIS-16 for definition.
//
class SatData93_UpdateInstrument : public SatData
{
private:
		LensMAC m_MAC;
		unsigned char m_status;  // this is the gateway status byte found in ...
		std::string m_siteName;
		std::string m_userName;	
public:
	SatData93_UpdateInstrument()
	{
		m_Type = SMT_UPDATE_INSTRUMENT;
	}


	void SetData
	(
		NMEA_DATA gps,
		LensMAC MAC,
		unsigned char status
	)
	{
		SetGPS(gps);
		m_MAC        = MAC;
		m_status     = (unsigned char)status;
	}

	//new code
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

		SetGPS(nmea);
		
		m_status = (unsigned char)doc["s"].GetInt();
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
	//end new code
	
	short	 Encode(char *buf)	// encode to buf - return length encoded
	{
		SatData::Encode(buf);	// will set idx for us
		EncodeMAC(buf, m_MAC);
		buf[idx++] = m_status;
		EncodeString(buf, m_siteName, 16);
		EncodeString(buf, m_userName, 16);		
		return idx;
	}
};
