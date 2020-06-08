#pragma once

#define MOBILE_ID_ENABLED_MASK              0x01
#define MOBILE_ID_TYPE_ENABLED_MASK         0x02
#define AUTHENTICATION_ENABLED_MASK         0x04
#define ROUTING_ENABLED_MASK                0x08
#define FORWARDING_ENABLED_MASK             0x10
#define RESPONSE_REDIRECTION_ENABLED_MASK   0x20
#define OPTIONS_VALID_MASK                  0x80
#define MAX_USERMESSAGE_LENGTH              848
//Service Types
typedef enum
{
	SERVICE_UNKNOWN         = -1,       // Unknown Service type (or unsupported)
	SERVICE_UNACK_RQST      =  0,       // Unacknowledge request (no ack expected)
	SERVICE_ACK_RQST        =  1,       // Acknowledge request (ack expected)
	SERVICE_ACK_RSP         =  2        // Response to an Ack message request
} ServiceTypes;

//Message Types
typedef enum
{
	MESSAGE_UNKNOWN             = -1,       // Unknown Message Type (or unsupported)
	MESSAGE_NULL                =  0,       // NULL message - WARNING there is no Message body for this message
	MESSAGE_ACK_NAK             =  1,       // Ack or Nak message
	MESSAGE_EVENT               =  2,       // Event Report message
	MESSAGE_ID_REPORT           =  3,       // ID Report Message
	MESSAGE_USER_DATA           =  4,       // User Dtaa message
	MESSAGE_APP_DATA            =  5,       // Application Data message
	MESSAGE_CFG_PARAM           =  6,       // Configuration Parameter message
	MESSAGE_UNIT_RQST           =  7,       // Unit Action Requenst message
	MESSAGE_LOCATE_REPORT       =  8,       // Locate Report Request message
	MESSAGE_USER_DATA_ACCUM     =  9,       // User Data with Accumulators message
	MESSAGE_MINI_EVENT          =  10,      // Mini Event Report Message
} MessageTypes;

typedef enum
{
	replyType_PGEMCI                         = 0,
	replyType_PGEMAS                         = 1,
	replyType_PGEMAC                         = 2,
	replyType_PGEMEM                         = 3,
	replyType_PGEMEC                         = 4,
	replyType_PGEMEE                         = 5,
	replyType_PGEMHA                         = 6,
	replyType_PGEMCC                         = 7,
	replyType_MANUAL                         = 8
} ReplyType;

typedef enum
{
	WAT_MESSAGE_UNKNOWN             = -1,
	WAT_MESSAGE_NULL                = 0,
	WAT_MESSAGE_OK                  = 1,
	WAT_MESSAGE_ASSIST              = 2,
	WAT_MESSAGE_ASSIST_CANCEL       = 3,
	WAT_MESSAGE_SOS                 = 4,
	WAT_MESSAGE_SOS_CANCEL          = 5,
	WAT_MESSAGE_SOS_CANCEL_HELP     = 6,
	WAT_MESSAGE_MONITOR_OFF         = 7,
	WAT_MESSAGE_STATE_REQUEST       = 8,
	WAT_MESSAGE_AUTHENTICATION      = 9,
	WAT_MESSAGE_HAZARD_ON           = 10,
	WAT_MESSAGE_HAZARD_OFF          = 11,
	WAT_MESSAGE_STATE_RESPONSE      = 12,
} WATMessageType;

class calampDecode
{
public:
	calampDecode();
	int decodePacket(const ats::String& message);
	const ats::String getMobileIDStr() const;
	uint16_t getSequenceNum() const {return m_sequenceNum;}

	struct userMsgData
	{
		WATMessageType type;
		union msgData{
			uint8_t state;
			char str[256];
		}mdata;
	};

	int parseUserPacket(userMsgData&);
protected:
	bool extractMsgAckNak(uint8_t *buffer, int16_t count);
	bool extractMsgUserData(uint8_t *buffer, int16_t count);
	bool sanityCheck() const;

private:
	ats::String m_originMsg;
	ats::String m_userMsg;
	uint64_t m_mobileID;
	uint8_t m_mobileIdLength;
	uint8_t m_mobileIDtype;
	uint8_t m_serviceType;
	uint8_t m_messageType;
	uint16_t m_sequenceNum;
	uint16_t m_userMsgLength;
	uint8_t m_userMessage[MAX_USERMESSAGE_LENGTH];
};
