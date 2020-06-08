#include <vector>

#include "ats-common.h"
#include "calampsackmessage.h"

CalAmpsAckMessage::CalAmpsAckMessage() :
	CalAmpsMessage()
{
	m_messageHeader.setServiceType(MessageHeader::ServiceType::RESPONSE_TO_ACKNOWLEDGED_REQUEST);
	m_messageHeader.setMessageType(MessageHeader::MessageType::ACK_NACK);
}

CalAmpsAckMessage::CalAmpsAckMessage(ats::StringMap& sm) :
	CalAmpsMessage()
{
	m_optionsHeader.setMobileId(sm.get("mobile_id"), (OptionsHeader::MOBILE_ID_TYPE)sm.get_int("mobile_id_type"));

	m_messageHeader.setServiceType(MessageHeader::ServiceType::RESPONSE_TO_ACKNOWLEDGED_REQUEST);
	m_messageHeader.setMessageType(MessageHeader::MessageType::ACK_NACK);

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
			{
				switch(mask)
				{
				case OptionsHeader::OPTION_MOBILE_ID:
				{
					QByteArray ba(convertfromBCD(p_data.mid(offset+1, p_data[offset])));
					msg->getOptionsHeader().setMobileId(ats::String(ba.begin(),ba.end()));
				}
					break;
				case OptionsHeader::OPTION_MOBILE_ID_TYPE:
					msg->getOptionsHeader().setMobileIdType((OptionsHeader::MOBILE_ID_TYPE)(char)p_data[offset+1]);
					break;
				////////////////////////////////
				// ATS-FIXME: Need to support all the possible options in the message header. Currently we only
				//            support mobile id and mobile id type
				default:
					;
				}
				offset += p_data[offset] + 1;
			}

			if(data_size < (offset+5))
				return -1;
			mask = mask << 1;
		}
	}

	msg->setSequenceNumber((256 * p_data[offset + 2]) + p_data[offset +3]);
	msg->setMessageType(p_data[offset + 1]);
	return 0;
}

void CalAmpsAckMessage::WriteData(QByteArray &data)
{
	ats::String optionsHeader = m_optionsHeader.data();
	ats::String messageHeader = m_messageHeader.data();
	data.append(optionsHeader.c_str(), optionsHeader.length());
	data.append(messageHeader.c_str(), messageHeader.length());
	QByteArray body(6,(char)0x00);
	body[0] = m_message_type;
	data.append(body);
}

ats::String CalAmpsAckMessage::getMessage()
{
	std::vector<char> body;
	body.resize(6, 0);
	body[0] = m_message_type;
	ats::String s = m_optionsHeader.data() + m_messageHeader.data();
	s.append(body.begin(),body.end());
	return s;
}
