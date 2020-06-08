#include <errno.h>

#include "atslogger.h"
#include "ats-string.h"

#include "iridiumdecode.h"
#include "zigbee-monitor.h"

iridiumDecode::iridiumDecode()
{
	memset(m_data, sizeof(m_data)/sizeof(uint8_t), 0);
	m_mobileID = 0;
}

bool iridiumDecode::sanityCheck()const
{
	return true;
}

int iridiumDecode::parseUserPacket(userMsgData& data)
{
	if( 1 > m_dataLength )
		return -EINVAL;

	if(!sanityCheck())
		return -EINVAL;

	data.mdata.data = (uint8_t)m_data[0];

	return 0;

}

const ats::String iridiumDecode::getMobileIDStr() const
{
	if(!m_mobileID)
		return ats::String();

	ats::String p_buf;
	ats_sprintf(&p_buf, "%.16llX", m_mobileID);
	return p_buf;

}

int iridiumDecode::decodePacket(const ats::String& msg)
{
	if(msg.length() & 0x01 || msg.length() > 1024 * 2)
		return -EINVAL;

	ats::String msg_data = ats::from_hex(msg);
	const char* ps = msg_data.c_str();

	m_originMsg = msg;


	size_t id_length = msg_data[0];
	if(id_length >= msg_data.length())
	{
		ats_logf(ATSLOG(0), "ID length(%d bytes) is longer than actual data (%d bytes)",id_length, msg_data.length());
		return -EINVAL;
	}
	ats::String id(msg_data.begin() + 1, msg_data.begin() + id_length + 1);
	m_mobileID = strtoull(id.c_str(), NULL, 10);
	m_mobileIdLength = id_length;
	ssize_t pos = id_length;
	m_dataLength = 256*ps[pos + 1] + ps[pos + 2];
	pos += 3;
	if((ssize_t)(m_dataLength + pos) > (ssize_t)(msg_data.size()))
	{
		ats_logf(ATSLOG(0), "User Data length(%d bytes) is longer than actual data (%d bytes). pos = %d", m_dataLength, msg_data.size(), pos);
		return -EINVAL;
	}
	memcpy(m_data, &ps[pos], m_dataLength);

	return 0;
}

uint32_t getData(const char* p, unsigned int size)
{
	if( size%2 || size > 8 ) return -EINVAL;

	char buf[32];
	for( unsigned int i = 0; i < size - 1; i+=2)
	{
		buf[i] = *(p + (size - 2 - i ));
		buf[i + 1] = *(p + (size - 1 - i ));
	}

	buf[size] = '\0';
	return strtoul(buf, NULL, 16);
}

uint8_t getChar(const char* p)
{
  uint8_t val;
	char buf[8];
	buf[0] = *(p);
	buf[1] = *(p + 1);
	buf[2] = '\0';
  val = (uint8_t)strtoul(buf, NULL, 16);
	return val;
}

ats::String getMobileID(const char* p, unsigned int size)
{
	if( size%2 || size > 16) return ats::String();

	char buf[64];
	for( unsigned int i = 0; i < size - 1; i+=2)
	{
		buf[i] = *(p + (size - 2 - i ));
		buf[i + 1] = *(p + (size - 1 - i ));
	}

	buf[size] = '\0';
	return buf;
}

ats::String getYYMMDDHHmmSS(time_t time)
{
	if( time <= refDateGMT2008 || time > MAXUNIXTIMESTAMP )
		return "000000000000";

	ats::String buf;
  struct tm ts   = *gmtime (&time);
	ats_sprintf(&buf, "%.2d%.2d%.2d%.2d%.2d%.2d", ts.tm_year-100, ts.tm_mon+1,
	       ts.tm_mday, ts.tm_hour, ts.tm_min, ts.tm_sec);
	return buf;
}

/*
 *  <data>
 *  <string name="mobileID"/>
 *  <unsignedByte name="mobileType"/>
 *  <unsignedByte name="prevState"/>
 *  <unsignedByte name="curState"/>
 *  <DateTime name="offWorkTimeGMT"/>
 *  <DateTime name="checkinTimeGMT"/>
 *  <DateTime name="hazardTimeGMT"/>
 *  <unsignedByte name="checkinState"/>
 *  <unsignedByte name="inControl"/>
 *  <unsignedShort name="manualIntervalMins"/>
 *  <DateTime name="sendTimeGMT"/>
 *  <string name="reply_to"/>
 * </data>
 */
int iridiumDecode::decodeWATStateResponseMsg(const ats::String& str, ats::String& watMessage, ats::String& id)
{
	//const ats::String test = "02AEC43E02006F0D00040000A407C00DABB3BF0D000000000100100EA4A5BF0D024349";
	//ats::String msg_data = test;

	ats_logf(ATSLOG(0), " Receive Iridium Message: %s", str.c_str());

	const ats::String& msg_data = str;
	int size = msg_data.size();

	ats_logf(ATSLOG(0), " Receive Iridium Message: (%d bytes) %s", size, str.c_str());

	const int unsignedByteSize = sizeof(unsignedByte)*2;
	const char* p = msg_data.c_str() + 2;//skip first byte for event WAState id
	char* d = (char* )p;

	responseMessage msg;

	d += 30;
	msg.mobileType = getChar(d);

	d += unsignedByteSize;
	msg.prevState = getChar(d);

	d += unsignedByteSize;
	msg.curState = getChar(d);

	ats_logf(ATSLOG(0), "mobileType %d, prevState: %d, curState: %d",	msg.mobileType, msg.prevState, msg.curState);

	return msg.curState;
}
