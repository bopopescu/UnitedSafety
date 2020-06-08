#include <errno.h>

#include "atslogger.h"
#include "ats-string.h"

#include "iridiumdecode.h"
#include "zigbee-monitor.h"
#include "checksum.h"

#define RED_ON "\x1b[1;31m"
#define GREEN_ON "\x1b[1;32m"
#define RESET_COLOR "\x1b[0m"

ats::String getYYMMDDHHmmSS(time_t time);
long g_LastMsgTime;  // last received message time.

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

int iridiumDecode::decodePacket(const ats::String& msg, ats::String &watMessage, ats::String imei)
{
	if(msg.length() & 0x01 || msg.length() > 1024 * 2)
		return -EINVAL;

  m_mobileID = atoll(imei.c_str());
	ats::String msg_data = ats::from_hex(msg);
	const char* ps = msg_data.c_str();

	responseMessage rMsg;

	size_t id_length = msg_data[0];
	ats::String id(msg_data.begin() + 1, msg_data.begin() + id_length + 1);
	short lenData;
	long pTime;
	memcpy(&lenData, &ps[id_length + 1], 2);
	rMsg.curState = ps[id_length + 3];
	memcpy(&pTime, &ps[id_length + 4], 4); rMsg.sendTimeGMT = pTime;
	
	if (pTime < g_LastMsgTime) // check for out of order messages - ignore older state messages.
	{
		ats_logf(ATSLOG_DEBUG, "%s:%d: " RED_ON "Iridium message out of order - dropping message"RESET_COLOR, __FILE__, __LINE__);
		return -EINVAL;
	}
  g_LastMsgTime = pTime;	
	memcpy(&pTime, &ps[id_length + 8], 4); rMsg.offWorkTimeGMT = pTime;
	memcpy(&pTime, &ps[id_length + 12], 4); rMsg.hazardTimeGMT = pTime;
	memcpy(&pTime, &ps[id_length + 16], 4); rMsg.checkinTimeGMT = pTime;
	
	ats_sprintf(&watMessage, "$PGEMSS,%d,0,60,3600,Manual,%s,%s,%s,%s",rMsg.curState, getYYMMDDHHmmSS(rMsg.sendTimeGMT).c_str(), 
			getYYMMDDHHmmSS(rMsg.offWorkTimeGMT).c_str(), getYYMMDDHHmmSS(rMsg.hazardTimeGMT).c_str(), getYYMMDDHHmmSS(rMsg.checkinTimeGMT).c_str());
			
	CHECKSUM cs;
	cs.add_checksum(watMessage);
	ats_logf(ATSLOG_DEBUG, "decodePacket: " GREEN_ON "%s"RESET_COLOR, watMessage.c_str());

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

	ats_logf(ATSLOG_DEBUG, " Receive Iridium Message: %s", str.c_str());

	const ats::String& msg_data = str;
	int size = msg_data.size();

	if( size < WATRESPONSEMINLENGTH )
		return -EINVAL;

	const int unsignedByteSize = sizeof(unsignedByte)*2;
	const int unsignedShortSize = sizeof(unsignedByte)*4;
	const int dateTimeSize = sizeof(DateTime)*2;
	const int mobileIDSize = 8*unsignedByteSize;
	const char* p = msg_data.c_str() + 2;//skip first byte for event WAState id
	char* d = (char* )p;

	if( size > WATRESPONSEFIXTYPELENGTH )
	{
		const unsigned char replyLength = getData(p + WATRESPONSEFIXTYPELENGTH - 2, unsignedByteSize);
		if( replyLength*2 + 2 + WATRESPONSEFIXTYPELENGTH <= size )
		{
			responseMessage msg;
			id = msg.mobileID = getMobileID(d, mobileIDSize);
			//ats_logf(ATSLOG_INFO, "id: %s", id.c_str());

			d += mobileIDSize;
			msg.mobileType = getData(d, unsignedByteSize);

			d += unsignedByteSize;
			msg.prevState = getData(d, unsignedByteSize);

			d += unsignedByteSize;
			msg.curState = getData(d, unsignedByteSize);

			d += unsignedByteSize;
			msg.offWorkTimeGMT = getData(d, dateTimeSize);

			d += dateTimeSize;
			msg.checkinTimeGMT = getData(d, dateTimeSize);

			d += dateTimeSize;
			msg.hazardTimeGMT = getData(d, dateTimeSize);

			d += dateTimeSize;
			msg.checkinState = getData(d, dateTimeSize);

			d += unsignedByteSize;
			msg.inControl = getData(d, unsignedByteSize);

			d += unsignedByteSize;
			msg.manualIntervalMins = getData(d, unsignedShortSize);

			d += unsignedShortSize;
			msg.sendTimeGMT = getData(d, dateTimeSize);
#if 0 
			time_t now = time(NULL);
			msg.sendTimeGMT = now - refDateGMT2008;
#endif

			d += (dateTimeSize + unsignedByteSize);
			ats::String ss = ats::from_hex(msg_data.substr(d-p+2, replyLength*2));
			if( ss[0] != 'M' && ss[0] != 'm' )
			{
				ss = "PGEM"+ss;
			}
			msg.reply_to = ss;

			ats_logf(ATSLOG_INFO, "mobileID: %s, mobileType %d, prevState: %d, curState: %d, offWorkTImeGMT: %d, checkinTimeGMT: %d,"
					"hazardTImeGMT: %d, checkinState: %d, inCtrotrol: %d, manualIntervalMins: %d, sendTimeGMT: %d, reply_to: %s",
					msg.mobileID.c_str(), msg.mobileType, msg.prevState, msg.curState, msg.offWorkTimeGMT, msg.checkinTimeGMT, msg.hazardTimeGMT, msg.checkinState, msg.inControl,
					msg.manualIntervalMins, msg.sendTimeGMT, msg.reply_to.c_str());

			ats_sprintf(&watMessage, "$PGEMSS,%d,%d,%d,0,%s,%s,%s,%s,%s,0",msg.curState,msg.inControl,msg.manualIntervalMins,msg.reply_to.c_str(), getYYMMDDHHmmSS(msg.sendTimeGMT+refDateGMT2008).c_str(), getYYMMDDHHmmSS(msg.offWorkTimeGMT+refDateGMT2008).c_str(), getYYMMDDHHmmSS(msg.hazardTimeGMT+refDateGMT2008).c_str(), getYYMMDDHHmmSS(msg.checkinTimeGMT+refDateGMT2008).c_str());

			ats_logf(ATSLOG_INFO, " watMessage: %s, ID: %s", watMessage.c_str(), id.c_str());
			return 0;
		}
	}

	return -EINVAL;
}
