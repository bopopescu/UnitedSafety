#pragma once
#define MAX_USERMESSAGE_LENGTH              848

typedef uint8_t unsignedByte;
typedef uint16_t unsignedShort;
typedef uint32_t DateTime;

//refer to struct responseMessage
/* format example: "02AEC43E02006F0D00040000A407C00DABB3BF0D000000000100100EA4A5BF0D024349" */
#define WATRESPONSEMINLENGTH 66 // 2 + 16 + 2 + 2 + 2 + 8 + 8 + 8 + 2 + 2 + 4 + 8 + ( 2 + 0 ) 
#define WATRESPONSEFIXTYPELENGTH 64
#define refDateGMT2008 1199145600
#define MAXUNIXTIMESTAMP 0x7FFFFFFF 

class iridiumDecode
{
public:
	iridiumDecode();
	int decodePacket(const ats::String& message);
	const ats::String getMobileIDStr() const;

	struct userMsgData
	{
		union msgData{
			int data;
			char str[128];
		}mdata;
	};

	struct responseMessage
	{
		ats::String mobileID;
		unsignedByte mobileType;
		unsignedByte prevState;
		unsignedByte curState;
		DateTime offWorkTimeGMT;
		DateTime checkinTimeGMT;
		DateTime hazardTimeGMT;
		unsignedByte checkinState;
		unsignedByte inControl;
		unsignedShort manualIntervalMins;
		DateTime sendTimeGMT;
		ats::String reply_to;
	};

	int parseUserPacket(userMsgData&);
	int decodeWATStateResponseMsg(const ats::String& str, ats::String&, ats::String&);

protected:
	bool sanityCheck() const;

private:
	ats::String m_originMsg;
	uint64_t m_mobileID;
	uint8_t m_mobileIdLength;
	uint16_t m_dataLength;
	uint8_t m_data[MAX_USERMESSAGE_LENGTH];
};
