#include <fstream>

#include "ats-common.h"
#include "atslogger.h"
#include "calampsmessage.h"

CalAmpsMessage::CalAmpsMessage()
{


}

#define	DC_IMEI_DATA_FILE "/tmp/config/imei"

ats::String CalAmpsMessage::readImei()
{
	ats::String imei;

	if(ats::get_file_line(imei, DC_IMEI_DATA_FILE, 1) < 0)
        {
		imei.clear();
	}

	return imei;
}

bool CalAmpsMessage::OptionsHeader::auto_setImei()
{
	const ats::String& imei = readImei();

	if(!imei.empty())
	{
		setMobileId(imei, OptionsHeader::MOBILE_ID_IMEI);
		return true;
	}
	return false;
}

void CalAmpsMessage::encodeInputsField(quint8 p_inputs)
{
	inputs &= (p_inputs & 0xFC) / 2;
	if(p_inputs & 0x02)  // CAMS has this whole flag inverted except for this bit so 1 is on and 0 is off and everything else inverts
	{
		inputs |= 0x01;
	}
	if(p_inputs & 0x01)
	{
		inputs |= 0x80;
	}
}

QString CalAmpsMessage::getImei(const QByteArray& msg)
{
	quint8 option_header_mask = CALAMP_OPTION_HEADER_DEFINE_BIT | \
								OptionsHeader::OPTION_MOBILE_ID | \
								OptionsHeader::OPTION_MOBILE_ID_TYPE;
	if((option_header_mask & msg[0]) == option_header_mask)
	{
		uint length = msg[1];
		QByteArray array = CalAmpsMessage::convertfromBCD(msg.mid(2, length));
		return QString(array);
	}
	return QString();
}

void CalAmpsMessage::setSequenceNumber(quint16 num)
{
	m_messageHeader.setSequenceNumber(num);
}

QByteArray CalAmpsMessage::convertToBCD(QString str)
{
	QByteArray data;
	if((str.length() % 2) != 0)
		str += "F";
	for(int i=0; i < (str.length()/2); i++)
	{
		data[i] = str.midRef(2*i, 2).toString().toUInt(0,16);
	}

	return data;
}

QByteArray CalAmpsMessage::convertfromBCD(const QByteArray& data)
{

	if(data.length() <= 0)
	{
		return data;
	}

	QByteArray ret(data.toHex());

	if(ret[ret.length() - 1] == 'f')
	{
		ret.chop(1);
	}

	return ret;
}

int CalAmpsMessage::getMessageType(const QByteArray& data)
{
	int offset = 0;
	if(data[offset] & CALAMP_OPTION_HEADER_DEFINE_BIT)
	{
		offset++;
		quint8 mask =1;
		for(int i=0; i<6; i++)
		{
			if(data[0] & mask)
				offset += data[offset] + 1;
			if(data.size() < (offset+1))
				return -1;
			mask = mask << 1;
		}
	}
	if(data.size() < (offset + 2))
	{
		return -1;
	}
	return data[offset + 1];
}

ats::String CalAmpsMessage::MessageHeader::data()
{
	ats::String s;
	s.append(1, (unsigned char)m_service_type);
	s.append(1, (unsigned char) m_message_type);

	{
		const quint16 seq = getSequenceNum();
		s.append(1, (seq >> 8) & 0xff);
		s.append(1, seq & 0xff);
	}

	return s;
}

CalAmpsMessage::OptionsHeader::OptionsHeader()
{}

void CalAmpsMessage::OptionsHeader::setOption(OPTION p_option, bool p_enabled)
{
	m_options[p_option] = p_enabled;
}

bool CalAmpsMessage::OptionsHeader::getOption(OPTION p_option)
{
	return m_options[p_option];
}

void CalAmpsMessage::OptionsHeader::setMobileId(ats::String p_mobile_id, MOBILE_ID_TYPE p_type)
{
	setOption(OPTION_MOBILE_ID, true);
	setOption(OPTION_MOBILE_ID_TYPE, true);

	if(p_mobile_id.empty())
	{
		auto_setImei();
		m_mobile_id_type = MOBILE_ID_IMEI;
		return;
	}

	m_mobile_id = p_mobile_id;
	m_mobile_id_type = p_type;
}

void CalAmpsMessage::OptionsHeader::setMobileId(ats::String p_mobile_id)
{
	setOption(OPTION_MOBILE_ID, true);
	m_mobile_id = p_mobile_id;
	if(p_mobile_id.empty())
	{
		this->auto_setImei();
		m_mobile_id_type = MOBILE_ID_IMEI;
	}
}

void CalAmpsMessage::OptionsHeader::setMobileIdType(MOBILE_ID_TYPE p_type)
{
	setOption(OPTION_MOBILE_ID_TYPE, true);
	m_mobile_id_type = p_type;
}

ats::String CalAmpsMessage::OptionsHeader::data()
{
	ats::String s;
	s.reserve(64);
	uchar b  = CALAMP_OPTION_HEADER_DEFINE_BIT;
	std::map<OPTION,bool>::iterator i;
	for(i = m_options.begin(); i != m_options.end(); i++)
	{
		if (i->second) b |= i->first;
	}
	s.append(1,b);

	if(getOption(OPTION_MOBILE_ID))
	{
		QByteArray bcd = CalAmpsMessage::convertToBCD(QString(getMobileId().c_str()));
		s.append(1,(uchar)(bcd.length()));
		s.append(bcd.begin(),bcd.end());
	}

	if(getOption(OPTION_MOBILE_ID_TYPE))
	{
		s.append(1,1);
		s.append(1,(uchar)getMobileIdType());
	}
	return s;
}
