#include "calampscompressedmessage.h"

CalAmpsCompressedMessage::CalAmpsCompressedMessage(const ats::String& p_imei, ats::String p_data)
{
	m_optionsHeader.setMobileId(p_imei, OptionsHeader::MOBILE_ID_IMEI);

	m_messageHeader.setServiceType(MessageHeader::ServiceType::ACKNOWLEDGED_REQUEST);
	m_messageHeader.setMessageType(MessageHeader::MessageType::COMPRESSED);

	m_data = p_data;
}

ats::String CalAmpsCompressedMessage::getMessage()
{
	return m_optionsHeader.data() + m_messageHeader.data() + m_data;
}

void CalAmpsCompressedMessage::WriteData(QByteArray &p_ba)
{
	ats::String s = getMessage();
	p_ba.clear();
	p_ba.append(s.c_str(), s.length());
}
