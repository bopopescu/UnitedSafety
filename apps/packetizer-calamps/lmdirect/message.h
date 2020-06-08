#pragma once

#include "ats-common.h"

namespace LMDIRECT
{
	
// Description: Implements the "Message" portion of the LMDirect packet.
class Message
{
public:

	class ServiceType
	{
	public:
		typedef enum
		{
			UNACKNOWLEDGED_REQUEST = 0,
			ACKNOWLEDGED_REQUEST,
			RESPONSE_TO_ACKNOWLEDGED_REQUEST
		}
		SERVICE_TYPE;
	};

	class MessageType
	{
	public:
		typedef enum
		{
			NULL_MSG = 0,
			ACK_NAK,
			EVENT_REPORT,
			ID_REPORT,
			USER_DATA,
			APP_DATA,
			CONF_PARAM,
			UNIT_REQ,
			LOCATE_REPORT,
			USER_DATA_WITH_ACCUMULATORS,
			MINI_EVENT_REPORT,
			MINI_USER,
			COMPRESSED = 134

		}
		MESSAGE_TYPE;
	};

	Message()
	{
		m_service_type = ServiceType::UNACKNOWLEDGED_REQUEST;
		m_message_type = MessageType::NULL_MSG;
		m_sequence = 1;
	}

	void set_message_type(MessageType::MESSAGE_TYPE p_type)
	{
		m_message_type = p_type;
	}

	MessageType::MESSAGE_TYPE get_message_type() {return m_message_type;}
	MessageType::MESSAGE_TYPE get_message_type() const {return m_message_type;}

	void set_service_type(ServiceType::SERVICE_TYPE p_type)
	{
		m_service_type = p_type;
	}

	ServiceType::SERVICE_TYPE get_service_type() {return m_service_type;}
	ServiceType::SERVICE_TYPE get_service_type() const {return m_service_type;}

	void set_message(const ats::String& p_msg);

	int set_sequence(int p_sequence)
	{
		m_sequence = p_sequence & 0xffff;

		if(!m_sequence)
		{
			m_sequence = 1;
		}

		return m_sequence;
	}

	void inc_sequence()
	{
		if((++m_sequence) > 65535)
		{
			m_sequence = 1;
		}

	}

	int get_sequence() {return m_sequence;}
	int get_sequence() const {return m_sequence;}

	const ats::String& get_message() {return m_msg;}
	const ats::String& get_message() const {return m_msg;}

	// Description: Returns the message header data in Generic Message Structure (GMS) format.
	ats::String get_gms_format() const;

private:
	ServiceType::SERVICE_TYPE m_service_type;
	MessageType::MESSAGE_TYPE m_message_type;
	int m_sequence;
	ats::String m_msg;
};

}
