#include "lmdirect/message.h"

void LMDIRECT::Message::set_message(const ats::String& p_msg)
{
	m_msg = p_msg;
}

ats::String LMDIRECT::Message::get_gms_format() const
{
	ats::String s;
	s.append(1, (unsigned char)m_service_type);
	s.append(1, (unsigned char)m_message_type);

	{
		const int seq = get_sequence();
		s.append(1, (seq >> 8) & 0xff);
		s.append(1, seq & 0xff);
	}

	if(MessageType::NULL_MSG != m_message_type)
	{
		s.append(m_msg);
	}

	return s;
}
