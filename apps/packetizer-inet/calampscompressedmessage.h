#pragma once
#include "calampsmessage.h"

class CalAmpsCompressedMessage : public CalAmpsMessage
{
public:
	CalAmpsCompressedMessage(const ats::String& p_imei, ats::String p_data);
	virtual void WriteData(QByteArray &);
	virtual ats::String getMessage();
private:
	ats::String m_data;
};

