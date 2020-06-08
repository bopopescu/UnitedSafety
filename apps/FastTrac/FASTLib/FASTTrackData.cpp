//---------------------------------------------------------
// FASTTrackData implementation file
//
// Dave Huff - May 2010 - base class for all data transfers
//
//  Each messages has the message type (short) as the first 2 bytes
//  then a single byte msgID (incremented on each message) to catch
//  missed messages.  The unitID is the serial number of the unit (short).
//  The position record is encoded as it was for the original SkyTrack stuff
//  with 3 bytes for lat and lon, tenths for HDOP, Epoch since Jan 1 2008 etc.
//
//  Derived classes should add data specific bytes and the encode should
//  call FASTTrackData::Encode(...) to get the first chunk of the data then
//  add the unique bytes to the end.
//

#include "J2K.h"
#include "FASTTrackData.h"
//#include "FASTBucketData.h"
#include "utility.h"


unsigned char FASTTrackData::m_MsgID = 0;
DailySeqNum FASTTrackData::m_SeqNum;
const char *FASTTrackData::m_CSVHeaderBase1 = {"MsgID, Call Sign, Time, Date"};
const char *FASTTrackData::m_CSVHeaderBase2 = {"Latitude, Longitude, Height, Course, Speed (kts), HDOP, UnitID, Type"};
char FASTTrackData::CSVbuf[] = {"This is a test"};


FASTTrackData::FASTTrackData()
{
  m_Type = STS_NOTHING;
  idx = 0;
  m_CSVHeader[0] = '\0';
  m_strEventType[0] = '\0';

  m_UnitID = -1;
}

// Encodes the type, message ID, unit ID and position.
//  Derived classes will add data to the end of this
//  The message ID is incremented here so all Encoded
//  messages should be sent via AFF or there will appear to
//  be gaps.
//
//  returns the encoded length
short FASTTrackData::Encode(char *buf)
{
  m_MsgID = m_SeqNum.GetSeqNum(); // increments the sequence number with a daily reset to 1
                                  // Set to m_MsgID because m_MsgID is used elsewhere (CSV output etc)
  idx = 0;
  short type = (short)m_Type;
  
  memcpy(&buf[idx], &type, 2); idx += 2;
  
  buf[idx++] = m_MsgID;
  
  {
    unsigned short s = GetUnitID() & 0xffff;
    memcpy(&buf[idx], &s, 2); idx += 2;
  }
  
  EncodeEpoch(buf, idx);  // idx +4 (will get incremented in function)
  EncodeLat(buf, idx);		// idx +3
  EncodeLon(buf, idx);		// idx +3
  EncodeH(buf, idx);			// idx +2
  short sog = (short)m_GPS.sog;
  
  memcpy(&buf[idx], &sog, 2); idx += 2;
  
  EncodeTrack(buf, idx);		// idx +2
  EncodeHDOP(buf, idx);			// idx +2
  EncodeCallSign(buf, idx);	// idx +0

  // add the RSSI value - item 1984
  
  char rssi = (char)m_RedStoneIPC.Rssi(); // range is -53 to -113 or 99 if unknown
  buf[idx++] = rssi;

  return idx;  // size should be 24
}


//---------------------------------------------------------------
//
// Data encoding functions
//
void FASTTrackData::EncodeEpoch(char *buf, short &idx)
{
  J2K jTime, jSTEpoch;
  m_GPS.GetJ2KTime(jTime);
  jSTEpoch.set_dmy_hms(1, 1, 2008, 0, 0, 0.0);

  long DateTime = (long)(jTime - jSTEpoch);
  memcpy(&buf[idx], &DateTime, 4);
  idx += 4;
}

void FASTTrackData::EncodeEpoch(J2K jTime, char *buf, short &idx)
{
  J2K jSTEpoch;
  jSTEpoch.set_dmy_hms(1, 1, 2008, 0, 0, 0.0);

  long DateTime = (long)(jTime - jSTEpoch);
  memcpy(&buf[idx], &DateTime, 4);
  idx += 4;
}

void FASTTrackData::EncodeLat(char *buf, short &idx)
{
  long iLat;
  char tBuf[8];

  iLat = (long)((90.0 - m_GPS.ddLat) * 93206.75);
  memcpy(tBuf, &iLat, 4);
  memcpy(&buf[idx], tBuf, 3);
  idx += 3;
}

void FASTTrackData::EncodeLon(char *buf, short &idx)
{
  long iLon;
  char tBuf[8];

  if (m_GPS.ddLon < 0.0)
    m_GPS.ddLon += 360.0;

  iLon = (long)(m_GPS.ddLon * 46603.375);
  memcpy(tBuf, &iLon, 4);
  memcpy(&buf[idx], tBuf, 3);
  idx += 3;
}

void FASTTrackData::EncodeH(char *buf, short &idx)
{
  short h = (short)(m_GPS.H + 1000.0);
  memcpy(&buf[idx], &h, 2);
  idx += 2;
}

void FASTTrackData::EncodeTrack(char *buf, short &idx)
{
  short cog = (short)(m_GPS.ddCOG * 10.0);

  memcpy(&buf[idx], &cog, 2);
  idx += 2;
}

void FASTTrackData::EncodeHDOP(char *buf, short &idx)
{
  short hdop = (short)(m_GPS.hdop * 10.0);

  memcpy(&buf[idx], &hdop, 2);
  idx += 2;
}


void FASTTrackData::EncodeCallSign(char *buf, short &idx)
{
  buf[idx++] = 0;
}

//--------------------------------------------------------------------------------------------------------
// write the KML record.
//
void FASTTrackData::WriteKMLRecord(FILE *fp)
{
	fp = fp;  // suppress compiler warning
}

void FASTTrackData::WriteKMLPositionData(FILE *fp)
{
	fp = fp;  // suppress compiler warning
}

//-------------------------------------------------------------------------------------------------
// WriteKMLHeader
//  insert the column titles on a new log file
//
void FASTTrackData::WriteKMLHeader(FILE *fp)
{
	fp = fp;  // suppress compiler warning
}

void FASTTrackData::WriteLogRecord(FILE *fp)
{
  WriteCSVPositionData1(fp);
  WriteCSVPositionData2(fp);
}

// just the position part of the output
void FASTTrackData::WriteCSVPositionData1(FILE *fp)
{
	fp = fp;  // suppress compiler warning
}

void FASTTrackData::WriteCSVPositionData2(FILE *fp)
{
	fp = fp;  // suppress compiler warning
}

const char * FASTTrackData::GetCSVHeader(char *buf)
{
  return buf;
}

void FASTTrackData::UpdateDay()  // should only be called at startup
{
//  J2K theTime;
//  theTime.SetSystemTime();  // get the date from the system
//  theTime += (nmea.GetGMTOffsetFromLon() * 3600);
//  m_SeqNum.UpdateDay(theTime.GetDay());
}


int FASTTrackData::GetUnitID()
{
	if(m_UnitID < 0)
	{
		FILE *f = fopen("/mnt/nvram/rom/UnitID.txt", "r");
		if(!f) return -1;
		std::string s;
		for(;;)
		{
			char c;
			const size_t nread = fread(&c, 1, 1, f);
			if(!nread) break;
			if('\n' == c) break;
			s += c;
		}
		fclose(f);
		m_UnitID = strtol(s.c_str(), 0, 0);
	}
	return m_UnitID;
}
