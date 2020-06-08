#include "calampsusermessage.h"

extern const ats::String g_CALAMPDBcolumnname[];

CalAmpsUserMessage::CalAmpsUserMessage(const ats::StringMap& data) :
	CalAmpsMessage()
{
	// ATS FIXME: Magic column numbers used. Should use just names of the columns.
	m_mobile_id = QString(data.get(g_CALAMPDBcolumnname[36]).c_str());
	m_mobile_id_type = data.get_int(g_CALAMPDBcolumnname[37]);

	if(m_mobile_id.isEmpty() || ((m_mobile_id_type < 1) && (m_mobile_id_type > 6)))
	{
		auto_setImei();
		m_mobile_id_type = CALAMP_MOBILE_ID_IMEI;
	}

	user_msg_id = 1;	// Needs to be 1 to talk to CAMS
	messageHeader = QByteArray(4, 0);
	messageHeader[0]=1;	// Acknowledged Request
	messageHeader[1]=4; // User Data Message

	optionsHeader[0] = 0x83; // 1000 0011 bit7: always set,  bit0:Mobile ID, bit1: Mobile ID type
	QByteArray esn = convertToBCD(m_mobile_id);
	optionsHeader[1] = esn.length();
	optionsHeader.append(esn);
	optionsHeader[esn.length()+2] = 1;
	optionsHeader[esn.length()+3] = m_mobile_id_type; // default is IMEI type

	updateTime = QDateTime::fromString(QString(data.get(g_CALAMPDBcolumnname[2]).c_str()), "yyyy-MM-dd HH:mm:ss");
	fixTime = QDateTime::fromString(QString(data.get(g_CALAMPDBcolumnname[3]).c_str()), "yyyy-MM-dd HH:mm:ss");
	latitude = qRound(data.get_double(g_CALAMPDBcolumnname[4]) * 10000000);
	longitude = qRound(data.get_double(g_CALAMPDBcolumnname[5]) * 10000000);
	altitude = qRound(data.get_double(g_CALAMPDBcolumnname[6]) * 100);
	speed = (data.get_int(g_CALAMPDBcolumnname[7])*1000)/36; //convert to cm/s
	rssi = (data.get_int(g_CALAMPDBcolumnname[34]));
	heading = qRound(data.get_double(g_CALAMPDBcolumnname[8]));
	hdop = qRound(data.get_double(g_CALAMPDBcolumnname[11]) * 10);
	satellites = data.get_int(g_CALAMPDBcolumnname[9]);
	fix_status = getFixStatus(data.get_int(g_CALAMPDBcolumnname[10]));
	inputs = 0x7E;
	encodeInputsField(data.get_int(g_CALAMPDBcolumnname[12]));
	user_msg = QString(data.get(g_CALAMPDBcolumnname[33]).c_str());
}

QString CalAmpsUserMessage::determineNmeaMessage(CALAMP_WAT_MESSAGE msg)
{
	switch(msg)
	{
	case CALAMP_WAT_CHECK_IN_MESSAGE:
		return QString(CALAMP_USER_MESSAGE_CHECK_IN);
	case CALAMP_WAT_ASSIST_MESSAGE:
		return QString(CALAMP_USER_MESSAGE_ASSIST);
	case CALAMP_WAT_ASSIST_CANCEL_MESSAGE:
		return QString(CALAMP_USER_MESSAGE_ASSIST_CANCEL);
	case CALAMP_WAT_SOS_MESSAGE:
		return QString(CALAMP_USER_MESSAGE_SOS);
	case CALAMP_WAT_SOS_CANCEL_MESSAGE:
		return QString(CALAMP_USER_MESSAGE_SOS_CANCEL);
	case CALAMP_WAT_MONITOR_OFF_MESSAGE:
		return QString(CALAMP_USER_MESSAGE_MONITOR_OFF);
	default:
	  return QString();
	}
}


void CalAmpsUserMessage::WriteData(QByteArray& data)
{
	qint16 data16;
	qint32 data32;
	const int offset = optionsHeader.length() + 4;
	data.append(optionsHeader);
	data.append(messageHeader);
	data32 = qToBigEndian((qint32)(updateTime.toUTC().toMSecsSinceEpoch()/1000));
	for(int i=0; i<4; i++)
		data[offset+i]=((uchar *)&data32)[i];
	data32 = qToBigEndian((qint32)(fixTime.toUTC().toMSecsSinceEpoch()/1000));
	for(int i=0; i<4; i++)
		data[offset+4+i]=((uchar *)&data32)[i];
	data32 = qToBigEndian(latitude);
	for(int i=0; i<4; i++)
		data[offset+8+i]=((uchar *)&data32)[i];
	data32 = qToBigEndian(longitude);
	for(int i=0; i<4; i++)
		data[offset+12+i]=((uchar *)&data32)[i];
	data32 = qToBigEndian(altitude);
	for(int i=0; i<4; i++)
		data[offset+16+i]=((uchar *)&data32)[i];
	data32 = qToBigEndian(speed);
	for(int i=0; i<4; i++)
		data[offset+20+i]=((uchar *)&data32)[i];
	data16 = qToBigEndian(heading);
	for(int i=0; i<2; i++)
		data[offset+24+i]=((uchar *)&data16)[i];
	data[offset+26] = satellites; //Satilites
	data[offset+27] = fix_status; // Fix Status
	data[offset+28] = 0; // Carrier MS byte
	data[offset+29] = 0; // Carrier LS byte
	data16 = qToBigEndian(rssi);
	for(int i=0; i<2; i++)
		data[offset+30+i]=((uchar *)&data16)[i];
	data[offset+32] = 0; // Comm State
	data[offset+33] = hdop;
	data[offset+34] = inputs; // Inputs
	data[offset+35] = 0; // Unit Status
	data[offset+36] = 0; // User Msg Route 0 = User 0(Host) Port
	data[offset+37] = user_msg_id;
	QByteArray user_decoded_data = QByteArray::fromHex(user_msg.toUtf8());
	data16 = qToBigEndian((qint16)(user_decoded_data.length()));
	for(int i=0; i<2; i++)
		data[offset+38+i]=((uchar *)&data16)[i];
	data.append(user_decoded_data);
}
