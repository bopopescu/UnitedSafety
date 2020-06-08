#include <errno.h>

#include "atslogger.h"
#include "ats-string.h"

#include "calampdecode.h"

calampDecode::calampDecode()
{
	memset(m_userMessage, sizeof(m_userMessage)/sizeof(uint8_t), 0);
	m_mobileID = 0;
}

bool calampDecode::extractMsgAckNak(uint8_t *buffer, int16_t count)
{
	if(count != 6)
		return false;

	//FIXME: handle ack/nack message.
	ats_logf(ATSLOG_DEBUG, "%s,%d: ack/nack message", __FILE__, __LINE__);

	return true ;
}

bool calampDecode::extractMsgUserData(uint8_t *buffer, int16_t count)
{
	size_t  pos = 4;
	m_userMsgLength = 0xFFFF & (buffer[2] << 8 | buffer[3]);

	if(m_userMsgLength == 0)  // might be padded from Gemalto
	{
		if (count > 39)
		{
			m_userMsgLength = 0xFFFF & (buffer[38] << 8 | buffer[39]);
			if (m_userMsgLength == 0)
				return true;
			pos = 40;
		}
	}

	if(m_userMsgLength > MAX_USERMESSAGE_LENGTH)
	{
		m_userMsgLength = 0;
		return false;
	}

	memcpy(m_userMessage, buffer + pos, m_userMsgLength);

	pos = m_originMsg.size() - 2 * m_userMsgLength;
	if(pos < 0)
		return false;

	m_userMsg = ats::from_hex(m_originMsg.substr(pos));
	ats_logf(ATSLOG_ERROR, "%s,%d: usermessage %s", __FILE__, __LINE__, m_userMsg.c_str());

	return true;
}

bool calampDecode::sanityCheck()const
{
	char checkSum = 0;

	if(m_userMsg.empty() || m_userMsg[0] != '$')
		return false;

	size_t checkSumLen = m_userMsg.find_last_of('*');
	if(checkSumLen == ats::String::npos)
		return false;

	ats::String s = m_userMsg.substr(checkSumLen + 1);
	char originCheckSum = strtol(s.c_str(), 0, 16);

	for(size_t i = 1; i < checkSumLen; ++i)
	{
		checkSum ^= m_userMsg[i];
	}

	if( originCheckSum != checkSum)
		return false;

	return true;
}

int calampDecode::parseUserPacket(userMsgData& data)
{
	WATMessageType type = WAT_MESSAGE_UNKNOWN;
	if(m_messageType != MESSAGE_USER_DATA || !m_userMsgLength)
		return type;

	if(!sanityCheck())
		return -EINVAL;

	ats::StringList a;
	ats::split(a, m_userMsg, ",");

	if(a.size() < 2 )
		return -EINVAL;

	//Monitor PGEMEC message only
	type = WAT_MESSAGE_NULL;
	const ats::String& command = a[0];
	if(command == "$PGEMSS" && a.size() >= 10 )
	{
		data.type = WAT_MESSAGE_STATE_RESPONSE;
		data.mdata.state = atoi(a[1].c_str());
		int strSize = m_userMsg.size();
		if( strSize < 256)
		{
			memcpy(data.mdata.str, m_userMsg.c_str(), strSize);
			data.mdata.str[strSize]='\0';
			return 0;
		}
	}
	return -1;
}

const ats::String calampDecode::getMobileIDStr() const
{
	if(!m_mobileID)
		return ats::String();

	ats::String p_buf;
	ats_sprintf(&p_buf, "%.16llX", m_mobileID);
	return p_buf;

}

int calampDecode::decodePacket(const ats::String& msg)
{
	const size_t count = msg.size();
	const char* ps = msg.c_str();
	uint8_t message[1024];

	m_originMsg = msg;

	if(count & 0x01 || count > 1024 * 2)
		return -EINVAL;

	for(size_t ct = 0; ct < count; ct++)
	{
		sscanf(ps, "%02hhx", &message[ct]);
		ps += 2 * sizeof(char);
	}

	size_t arrayCount = count >> 1;

	const uint8_t options = message[0];

	size_t pos = 0;

	//in options header area.
	if(!(options & OPTIONS_VALID_MASK))
	{
		ats_logf(ATSLOG_ERROR, "%s,%d: Bad Parameter", __FILE__, __LINE__);
		return -EINVAL;
	}

	pos++;

	if(options & MOBILE_ID_ENABLED_MASK)
	{
		//check length
		if(arrayCount - pos < 1 || arrayCount - pos < (size_t)(message[pos] + 1))
		{
			return -EINVAL;
		}

		m_mobileIdLength = message[pos];
		uint8_t * localBuffer = message + 2;

		// convert packed BCD to large integer
		for(int i = m_mobileIdLength; i > 0; i--, localBuffer++)
		{
			m_mobileID *= 10;
			m_mobileID += ((*localBuffer >> 4) & 0xf);

			if((*localBuffer & 0xf) != 0xf)
			{
				m_mobileID *= 10;
				m_mobileID += (*localBuffer & 0xf);
			}
		}

		pos += 1 + m_mobileIdLength;
	}

	if(options & MOBILE_ID_TYPE_ENABLED_MASK)
	{
		if(arrayCount - pos < 1 || message[pos] != 1)
			return -EINVAL;

		if(message[pos + 1] > 7) //mobile id type less than 6
			return -EINVAL;

		m_mobileIDtype = message[pos + 1];
		pos += 2;
	}

	if(options & AUTHENTICATION_ENABLED_MASK)
	{
		if(arrayCount - pos < 1 || arrayCount - pos < (size_t)(message[pos] + 1))
			return -EINVAL;

		pos += 1 + message[pos];
	}

	if(options & ROUTING_ENABLED_MASK)
	{
		if(arrayCount - pos < 1 || arrayCount - pos < (size_t)(message[pos] + 1))
			return -EINVAL;

		pos += 1 + message[pos];
	}

	if(options & FORWARDING_ENABLED_MASK)
	{
		if(arrayCount - pos < 1 || arrayCount - pos < (size_t)(message[pos] + 1))
			return -EINVAL;

		pos += 1 + message[pos];
	}

	if(options & RESPONSE_REDIRECTION_ENABLED_MASK)
	{
		if(arrayCount - pos < 1 || arrayCount - pos < (size_t)(message[pos] + 1))
			return -EINVAL;

		pos += 1 + message[pos];
	}

	//in Message Header area.
	if(arrayCount - pos < 4) // Message Header size is 4.
			return -EINVAL;
	else
	{
		m_serviceType = message[pos++];
		m_messageType = message[pos++];
		m_sequenceNum = 0xFFFF & (message[pos] << 8 | message[pos+1]);
		pos += 2;
	}

	switch(m_messageType)
	{
		case MESSAGE_NULL:
			break;
		case MESSAGE_ACK_NAK:
			if(!extractMsgAckNak(message + pos, arrayCount - pos))
				return -EINVAL;
			break;
		case MESSAGE_USER_DATA:
			if(!extractMsgUserData(message + pos, arrayCount - pos))
				return -EINVAL;
			break;
		case MESSAGE_EVENT:
		case MESSAGE_ID_REPORT:
		case MESSAGE_APP_DATA:
		case MESSAGE_CFG_PARAM:
		case MESSAGE_UNIT_RQST:
		case MESSAGE_LOCATE_REPORT:
		case MESSAGE_USER_DATA_ACCUM:
		case MESSAGE_MINI_EVENT:
			return -ENOSYS;
		default:
			break;
	}

	return 0;
}
