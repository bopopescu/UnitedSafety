#include <fstream>

#include "ats-common.h"
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

bool CalAmpsMessage::auto_setImei()
{
	const ats::String& imei = readImei();

	if(!imei.empty())
	{
		m_mobile_id = QString(imei.c_str());
		m_mobile_id_type = CALAMP_MOBILE_ID_IMEI;
		return true;
	}

	m_mobile_id.clear();
	// ATS FIXME: Should "m_mobile_id_type" also be set to something?
	return false;
}

void CalAmpsMessage::setOptionHeader(QByteArray oh)
{
	optionsHeader.clear();
	optionsHeader.append(oh);
}

void CalAmpsMessage::setMessageHeader(QByteArray mh)
{
	if(mh.size() < 4)
	{
		return;
	}
	messageHeader.clear();
	messageHeader.append(mh.data(), 4);

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

// The following fix status is available from the FASTRAX
//  1 = GPS fix (SPS)
//  2 = DGPS fix
//  3 = PPS fix
//  4 = Real Time Kinematic
//  5 = Float RTK
//  6 = estimated (dead reckoning) (2.3 feature)
//  7 = Manual input mode
//  8 = Simulation mode
//
quint8 CalAmpsMessage::getFixStatus(int fix_status) const
{
	quint8 val = 0;
  
	switch(fix_status)
	{
	case 1:
		break;
	case 2:
	case 3:
	case 4:
	case 5:
		val = CALAMP_GPS_FIX_DIFFERENTIAL;
		break;
	case 6:
		val = CALAMP_GPS_FIX_PREDICTION;
		// Fall through...
	default:
		break;
	}

	return val;
}

QString CalAmpsMessage::getImei(const QByteArray& data)
{
	if((0x83 & data[0]) == 0x83)
	{
		uint length = data[1];
		QByteArray array = CalAmpsMessage::convertfromBCD(data.mid(2, length));
		return QString(array);
	}
	return QString();
}

void CalAmpsMessage::setSequenceNumber(quint16 num)
{
	qint16 data16 = qToBigEndian(num);
	messageHeader[2] = ((uchar*)&data16)[0];
	messageHeader[3] = ((uchar*)&data16)[1];

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
