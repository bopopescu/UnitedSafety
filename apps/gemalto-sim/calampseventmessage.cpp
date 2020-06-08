#include "ats-common.h"
#include "calampseventmessage.h"

extern const ats::String g_CALAMPDBcolumnname[];

CalAmpsEventMessage::CalAmpsEventMessage(ats::StringMap& data, quint8 event) :
	CalAmpsMessage()
{
	m_optionsHeader.setMobileId(data.get("mobile_id"), (OptionsHeader::MOBILE_ID_TYPE)data.get_int("mobile_id_type"));

	m_messageHeader.setServiceType(MessageHeader::ServiceType::ACKNOWLEDGED_REQUEST);
	m_messageHeader.setMessageType(MessageHeader::MessageType::EVENT_REPORT);

	updateTime = QDateTime::fromString(QString(data.get("event_time").c_str()), "yyyy-MM-dd HH:mm:ss");
	fixTime = QDateTime::fromString(QString(data.get("fix_time").c_str()), "yyyy-MM-dd HH:mm:ss");
	latitude = qRound(data.get_double("latitude") * 10000000);
	longitude = qRound(data.get_double("longitude") * 10000000);
	altitude = qRound(data.get_double("altitude") * 100);
	speed = (data.get_int("speed")*1000)/36; //Convert to cm/s
	rssi = (data.get_int("rssi"));
	heading = qRound(data.get_double("heading"));
	hdop = qRound(data.get_double("hdop") * 10);
	satellites = data.get_int("satellites");
	// AWARE360 FIXME: Delete comment or add more detail because it is not understood.
	//	Perhaps explain operation of this function to make comment more relevant.
	fix_status = data.get_int("fix_status");  // uses the value as set by message-assembler (modified there to meet cams spec)
	inputs = CALAMPS_INPUTS_DEFAULT;
	encodeInputsField(data.get_int("inputs"));

	if(!data.has_key("event_index"))
	{
		if (event == 100 || event == 101 || event == 200)
			event_index = 1;
		else
			event_index = 254;
	}
	else
	{
		event_index = data.get_int("event_index");
	}

	if(!data.has_key("event_code"))
	{
		event_code = event;
	}
	else
	{
		event_code = data.get_int("event_code");
	}

	accums = data.get_int("accums");
	spare = data.get_int("spare");
}

void CalAmpsEventMessage::WriteData(QByteArray& data)
{
	qint16 data16;
	qint32 data32;
	ats::String optionsHeader = m_optionsHeader.data();
	ats::String messageHeader = m_messageHeader.data();
	const int offset = optionsHeader.length() + messageHeader.length();
	data.append(optionsHeader.c_str(), optionsHeader.length());
	data.append(messageHeader.c_str(), messageHeader.length());
	data32 = qToBigEndian((qint32)(updateTime.toMSecsSinceEpoch()/1000));
	for(int i=0; i<4; i++)
		data[offset+i]=((uchar *)&data32)[i];
	data32 = qToBigEndian((qint32)(fixTime.toMSecsSinceEpoch()/1000));
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
	data[offset+36] = event_index;
	data[offset+37] = event_code;
	data[offset+38] = 0; // accums
	data[offset+39] = 0; // spare
}

ats::String CalAmpsEventMessage::getMessage()
{
	ats::String s;
	QByteArray ba;
	WriteData(ba);
	s.append(ba.begin(),ba.end());
	return s;
}
