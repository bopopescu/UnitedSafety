#pragma once
//-------------------------------------------------------------------
// SatData
//
// Defines a base class for all satellite data records.	It contains
// a time, position, type, unique message ID, etc
// The message ID and the unit ID are statics across the class so that
// message IDs increment based on each data set encoded for transmission.
// The Unit ID is the last 5 digits of the serial number on the box.
//
// Each message type should have the following:
//	- Basic data (Time, position etc)
//	- Encode - encodes the class to send via AFF
//
// Dave Huff 	- May 2019
// 		- adapted from FASTTrackData 	- May 2019
//

#include "NMEA_DATA.h"
#include <LensMAC.h>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "ConfigDB.h"

enum SAT_MSG_TYPES
{
	SMT_NOTHING = 0,
	SMT_IDENTIFY_GATEWAY = 90,
	SMT_IDENTIFY_INSTRUMENT,
	SMT_CREATE_INSTRUMENT,
	SMT_UPDATE_INSTRUMENT,
	SMT_UPLOAD_ERROR,
	SMT_LOST_INSTRUMENT,
	SMT_UPDATE_GATEWAY
};



class SatData
{
public:
	SAT_MSG_TYPES m_Type; // Message type.
	static unsigned char m_SeqNum; // increments on every encode
	NMEA_DATA m_GPS;

	short idx; // length of message as it is encoded.

public:
	SatData();

	virtual ~SatData(){};

	void SetGPS(NMEA_DATA &gps){m_GPS = gps;};
	void SetMsgType(SAT_MSG_TYPES type){m_Type = type;};
	int GetUnitID();

	virtual short Encode(char *buf);	// encode to buf - return length encoded


public:	// functions for encoding the data
	void EncodeEpoch(char *buf, short &idx);	//ISCP-338
	void EncodeLat(char *buf, short &idx);
	void EncodeLon(char *buf, short &idx);
	void EncodeTrack(char *buf, short &idx);
	void EncodeH(char *buf, short &idx);
	void EncodeHDOP(char *buf, short &idx);
	void EncodeMAC(char *buf, LensMAC mac);
	void EncodeString(char *buf, std::string str, short maxLen);  // encode a string up to maxLen or 32 (hard limit)
	
};
