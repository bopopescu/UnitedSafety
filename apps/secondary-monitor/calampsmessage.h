#pragma once

#include <QByteArray>
#include <QtEndian>
#include <QTime>
#include <QDateTime>

#include <vector>

#include "ats-common.h"

class CalAmpsMessage
{
public:
	class MessageHeader
	{
	public:
		class ServiceType
		{
		public:
			typedef enum {
				UNACKNOWLEDGED_REQUEST =           0,
				ACKNOWLEDGED_REQUEST =             1,
				RESPONSE_TO_ACKNOWLEDGED_REQUEST = 2
			} SERVICE_TYPE;
		};

		class MessageType
		{
		public:
			typedef enum {
				INVALID =                  -1,
				NULL_MSG =                  0,
				ACK_NACK =                  1,
				EVENT_REPORT =              2,
				ID_REPORT =                 3,
				USER_DATA =                 4,
				APPLICATION_DATA =          5,
				CONFIG_PARAM =              6,
				UNIT_REQUEST =              7,
				LOCATE_REPORT =             8,
				USER_DATA_W_ACCUMULATORS =  9,
				MINI_EVENT_REPORT =         10,
				COMPRESSED =                134
			} MESSAGE_TYPE;
		};

		MessageHeader()
		{
			m_service_type = ServiceType::UNACKNOWLEDGED_REQUEST;
			m_message_type = MessageType::NULL_MSG;
			m_sequence_num = 1;
		}

		void setMessageType(MessageType::MESSAGE_TYPE p_type)
		{
			m_message_type = p_type;
		}

		void setServiceType(ServiceType::SERVICE_TYPE p_type)
		{
			m_service_type = p_type;
		}

		void setSequenceNumber(quint16 p_num)
		{
			m_sequence_num = p_num;
			if(!m_sequence_num)
			{
				m_sequence_num = 1;
			}
		}

		void incSequenceNumber()
		{
			if(!(++m_sequence_num))
			{
				m_sequence_num = 1;
			}
		}

		MessageType::MESSAGE_TYPE getMessageType() {return m_message_type;}
		ServiceType::SERVICE_TYPE getServiceType() {return m_service_type;}
		quint16 getSequenceNum() {return m_sequence_num;}
		ats::String data();

	private:
		quint16 m_sequence_num;
		MessageType::MESSAGE_TYPE m_message_type;
		ServiceType::SERVICE_TYPE m_service_type;

	};

	class OptionsHeader
	{
	public:

		//OPTION HEADER Option Byte
		#define CALAMP_OPTION_HEADER_DEFINE_BIT     0x80

		typedef enum
		{
			OPTION_MOBILE_ID = 0x01,
			OPTION_MOBILE_ID_TYPE = 0x02,
			OPTION_AUTHENTICATION_WORD = 0x04,
			OPTION_ROUTING = 0x08,
			OPTION_FORWARDING = 0x10,
			OPTION_RESPONSE = 0x20,
			//
			OPTION_MAX = 7
		} OPTION;

		typedef enum {
			MOBILE_ID_ESN = 1,
			MOBILE_ID_IMEI = 2,
			MOBILE_ID_IMSI = 3,
			MOBILE_ID_USER = 4,
			MOBILE_ID_PHONE = 5,
			MOBILE_ID_IP = 6
		} MOBILE_ID_TYPE;

		OptionsHeader();
		void setOption(OPTION, bool);
		void setMobileId(ats::String p_mobile_id, MOBILE_ID_TYPE p_type);
		void setMobileId(ats::String p_mobile_id);
		void setMobileIdType(MOBILE_ID_TYPE p_type);
		void setAuthenticationWord(const ats::String& p_word);
		void setForwardingAddress(const ats::String& p_addr);
		void setForwardingProtocol(const ats::String& p_proto);
		void setResponseAddress(const ats::String& p_addr);
		void setResponsePort(const ats::String& p_port);

		const ats::String& getMobileId() { return m_mobile_id; }
		const MOBILE_ID_TYPE getMobileIdType() { return m_mobile_id_type;}
		bool getOption(OPTION);

		ats::String data();

	private:
		bool auto_setImei();

		std::map<OPTION, bool> m_options;
		ats::String m_mobile_id;
		MOBILE_ID_TYPE m_mobile_id_type;
		ats::String m_auth_word;

	};


	CalAmpsMessage();
	CalAmpsMessage(QByteArray &);

// Set Methods
	void setSequenceNumber(quint16 num);

//Get Methods
	virtual void        WriteData(QByteArray& ) = 0;
	virtual ats::String getMessage() = 0;
	const ats::String&  getImei(){return m_optionsHeader.getMobileId();}
	static QString      getImei(const QByteArray&);
	static int          getMessageType(const QByteArray&);
	OptionsHeader&      getOptionsHeader() {return m_optionsHeader;}
	MessageHeader&      getMessageHeader() {return m_messageHeader;}

	static ats::String  readImei();

protected:
	static QByteArray convertToBCD(QString );
	static QByteArray convertfromBCD(const QByteArray&);
#define CALAMPS_INPUTS_DEFAULT 0x7E
	void              encodeInputsField(quint8 p_inputs);
	OptionsHeader     m_optionsHeader;
	MessageHeader     m_messageHeader;
	ats::String       m_mobile_id;
	qint8             m_mobile_id_type;
	quint16           sequence_num;
	quint8            inputs;
private:

};
