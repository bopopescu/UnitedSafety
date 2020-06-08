#include "ats-common.h"
#include "calampsackmessage.h"

CalAmpsAckMessage::CalAmpsAckMessage() :
	CalAmpsMessage()
{

	messageHeader[0] = CALAMP_SERVICE_RESPONSE;
	messageHeader[1] = CALAMP_MESSAGE_ACK_NACK;
}


CalAmpsAckMessage::CalAmpsAckMessage(ats::StringMap& sm) :
	CalAmpsMessage()
{
	m_mobile_id = QString(sm.get("mobile_id").c_str());
	m_mobile_id_type = sm.get_int("mobile_id_type");
	if(m_mobile_id.isEmpty() || ((m_mobile_id_type < 1) && (m_mobile_id_type > 6)))
	{
		auto_setImei();
		m_mobile_id_type = CALAMP_MOBILE_ID_IMEI;
	}

	messageHeader = QByteArray(4, 0);
	messageHeader[0]=CALAMP_SERVICE_RESPONSE;	// Response to Acknowledged Request
	messageHeader[1]=CALAMP_MESSAGE_ACK_NACK; // Event Report Message

	optionsHeader[0] = 0x83; // 1000 0011 bit7: always set,  bit0:Mobile ID, bit1: Mobile ID type
	QByteArray esn = convertToBCD(m_mobile_id);
	optionsHeader[1] = esn.length();
	optionsHeader.append(esn);
	optionsHeader[esn.length()+2] = 1; //always 1 as per CalAMPs spec.
	optionsHeader[esn.length()+3] = m_mobile_id_type; // default is IMEI type

	m_message_type = sm.get_int("message_type");
}

int CalAmpsAckMessage::fromData(CalAmpsAckMessage *msg, QByteArray p_data)
{
	if(p_data.length() < 4)
	{
		return -1;
	}
	int offset = 0;
	int data_size = p_data.size();
	if(p_data[offset] & CALAMP_OPTION_HEADER_DEFINE_BIT)
	{
		offset++;
		quint8 mask =1;
		for(int i=0; i<6; i++)
		{
			if(p_data[0] & mask)
				offset += p_data[offset] + 1;
			if(data_size < (offset+5))
				return -1;
			mask = mask << 1;
		}
	}
	{
		QByteArray oh(p_data.data(), offset);
		msg->setOptionHeader(oh);
	}
	msg->setSequenceNumber((256 * p_data[offset + 2]) + p_data[offset +3]);
	msg->setMessageType(p_data[offset + 1]);
	return 0;
}

void CalAmpsAckMessage::WriteData(QByteArray &data)
{
	data.append(optionsHeader);
	data.append(messageHeader);
	QByteArray body(6,(char)0x00);
	body[0] = m_message_type;
	data.append(body);
}
