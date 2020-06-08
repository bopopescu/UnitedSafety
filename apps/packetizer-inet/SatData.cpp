//---------------------------------------------------------
// SatData implementation file
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
//  call SatData::Encode(...) to get the first chunk of the data then
//  add the unique bytes to the end.
//

#include "J2K.h"
#include "SatData.h"
#include "utility.h"
#include <LensMAC.h>
#include "atslogger.h"

unsigned char SatData::m_SeqNum = 0;

SatData::SatData()
{
  m_Type = SMT_NOTHING;
}

// Encodes the type, message ID, unit ID and position.
//  Derived classes will add data to the end of this
//  The message ID is incremented here so all Encoded
//  messages should be sent via AFF or there will appear to
//  be gaps.
//
//  returns the encoded length
short SatData::Encode(char *buf)
{
  ++m_SeqNum;  // unsigned byte will wrap at 255

  idx = 0;
  short type = (short)m_Type;
  
  memcpy(&buf[idx], &type, 2); idx += 2;
  
  buf[idx++] = m_SeqNum;
  
  EncodeEpoch(buf, idx);  // idx +4 (will get incremented in function)
  EncodeLat(buf, idx);		// idx +3
  EncodeLon(buf, idx);		// idx +3
  EncodeH(buf, idx);			// idx +2
	
  short sog = (short)m_GPS.sog;
  memcpy(&buf[idx], &sog, 2); idx += 2;
  
  EncodeTrack(buf, idx);		// idx +2
  EncodeHDOP(buf, idx);			// idx +2

  return idx;  // size should be 18
}

//---------------------------------------------------------------
//
// Data encoding functions
//
void SatData::EncodeEpoch(char *buf, short &idx)
{
  time_t secondsTillToday; 
  long secondsTill2008 = 1199145600; //ISCP-338
  long secondsFrom2008TillToday = 0; 

  secondsTillToday = time (NULL);
  secondsFrom2008TillToday = secondsTillToday - secondsTill2008;  //ISCP-338 
  memcpy(&buf[idx], &secondsFrom2008TillToday, 4);
  idx += 4;  
}


void SatData::EncodeLat(char *buf, short &idx)
{
  long iLat;
  char tBuf[8];

  //if( m_GPS.gps_quality == -1 )
  if(m_GPS.isValid() == false || m_GPS.gps_quality == 0)
  {
    ats_logf(ATSLOG_INFO, "%s,%d: ISC-303 Fix:NO GPS fix found Excluding GPS data from Iridium Encoder\n", __FILE__, __LINE__);
    iLat = 0; // setting to zero will force Translator service not to send GPS data to inet
  }
  else
  {
  iLat = (long)((90.0 - m_GPS.ddLat) * 93206.75);
  }

  ats_logf(ATSLOG_INFO, "%s,%d: ISC-303 Fix:Lattitude Is Set TO:%ld\n", __FILE__, __LINE__,iLat);

  memcpy(tBuf, &iLat, 4);
  memcpy(&buf[idx], tBuf, 3);
  idx += 3;
}

void SatData::EncodeLon(char *buf, short &idx)
{
  long iLon;
  char tBuf[8];

  if (m_GPS.ddLon < 0.0)
    m_GPS.ddLon += 360.0;

  //if( m_GPS.gps_quality == -1 )
  if(m_GPS.isValid() == false || m_GPS.gps_quality == 0)
  {
    ats_logf(ATSLOG_INFO, "%s,%d: ISC-303 Fix:NO GPS fix found Excluding GPS data from Iridium Encoder\n", __FILE__, __LINE__);
    iLon = 0; // setting to zero will force Translator service not to send GPS data to inet
  }
  else
  {
  iLon = (long)(m_GPS.ddLon * 46603.375);
  }

  ats_logf(ATSLOG_INFO, "%s,%d: ISC-303 Fix:Longitude Is Set TO:%ld\n", __FILE__, __LINE__,iLon);

  memcpy(tBuf, &iLon, 4);
  memcpy(&buf[idx], tBuf, 3);
  idx += 3;
}

void SatData::EncodeH(char *buf, short &idx)
{
  short h = (short)(m_GPS.H + 1000.0);
  memcpy(&buf[idx], &h, 2);
  idx += 2;
}

void SatData::EncodeTrack(char *buf, short &idx)
{
  short cog = (short)(m_GPS.ddCOG * 10.0);

  memcpy(&buf[idx], &cog, 2);
  idx += 2;
}

void SatData::EncodeHDOP(char *buf, short &idx)
{
  short hdop = (short)(m_GPS.hdop * 10.0);

  memcpy(&buf[idx], &hdop, 2);
  idx += 2;
}

void SatData::EncodeMAC(char *buf, LensMAC mac)
{
	mac.GetMac(&buf[idx]);
  idx += 3;
}

void SatData::EncodeString  // encode a string up to maxLen or 32 (hard limit)
(
	char *buf, 
	std::string str,	
	short maxLen
)
{
	if (maxLen > 32)
		maxLen = 32;
	int len;
	if ( (len = str.length()) > maxLen)
		len = maxLen;
	
	buf[idx++] = len;
	memcpy(&buf[idx], str.c_str(), len);
	idx += len;
}
