#pragma once
#include "ats-common.h"
#include "calampsmessage.h"

class CalAmpsAckMessage : public CalAmpsMessage
{
public:
	CalAmpsAckMessage();
	CalAmpsAckMessage(ats::StringMap& );
	static int fromData(CalAmpsAckMessage *msg,QByteArray p_data);
	void setMessageType(uint p_msg_type) { m_message_type = p_msg_type;}
	virtual void WriteData(QByteArray& );
	virtual ats::String getMessage();

private:
	int m_message_type;
};
