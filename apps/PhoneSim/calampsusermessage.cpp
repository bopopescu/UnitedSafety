#include "calampsusermessage.h"

extern const ats::String g_CALAMPDBcolumnname[];

CalAmpsUserMessage::CalAmpsUserMessage(const ats::StringMap& data) :
	CalAmpsMessage()
{
	m_optionsHeader.setMobileId(data.get("mobile_id"), (OptionsHeader::MOBILE_ID_TYPE)data.get_int("mobile_id_type"));

	m_messageHeader.setServiceType(MessageHeader::ServiceType::ACKNOWLEDGED_REQUEST);
	m_messageHeader.setMessageType(MessageHeader::MessageType::USER_DATA);

	user_msg_id = 1;	// Needs to be 1 to talk to CAMS
	updateTime = QDateTime::fromString(QString(data.get("event_time").c_str()), "yyyy-MM-dd HH:mm:ss");
	fixTime = QDateTime::fromString(QString(data.get("fix_time").c_str()), "yyyy-MM-dd HH:mm:ss");
	latitude = qRound(data.get_double("latitude") * 10000000);
	longitude = qRound(data.get_double("longitude") * 10000000);
	altitude = qRound(data.get_double("altitude") * 100);
	speed = (data.get_int("speed")*1000)/36; //convert to cm/s
	rssi = (data.get_int("rssi"));
	heading = qRound(data.get_double("heading"));
	hdop = qRound(data.get_double("hdop") * 10);
	satellites = data.get_int("satellites");
	fix_status = data.get_int("fix_status");
	inputs = CALAMPS_INPUTS_DEFAULT;
	encodeInputsField(data.get_int("inputs"));
	user_msg = QString(data.get("usr_msg_data").c_str());
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
	ats::String messageHeader = m_messageHeader.data();
	ats::String optionsHeader = m_optionsHeader.data();
	const int offset = optionsHeader.length() + messageHeader.length();
	data.append(optionsHeader.c_str(), optionsHeader.length());
	data.append(messageHeader.c_str(), messageHeader.length());
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

ats::String CalAmpsUserMessage::getMessage()
{
	ats::String s;
	QByteArray ba;
	WriteData(ba);
	s.append(ba.begin(),ba.end());
	return s;
}
