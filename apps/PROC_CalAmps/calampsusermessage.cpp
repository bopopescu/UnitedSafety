#include "calampsusermessage.h"
#include "datacollector.h"

CalAmpsUserMessage::CalAmpsUserMessage(struct dev_data *rec, CALAMP_WAT_MESSAGE msg)
{
	user_msg_id = 1;	// Needs to be 1 to talk to CAMS
	messageHeader = QByteArray(4, 0);
	messageHeader[0]=1;	// Acknowledged Request
	messageHeader[1]=4; // User Data Message

	optionsHeader[0] = 0x83; // 1000 0011 bit7: always set,  bit0:Mobile ID, bit1: Mobile ID type
	QByteArray esn = convertToBCD(rec->esn);
	optionsHeader[1] = esn.length();
	optionsHeader.append(esn);
	optionsHeader[esn.length()+2] = 1;
	optionsHeader[esn.length()+3] = 2; // IMEI type

	imei = rec->esn;
	updateTime = QDateTime::currentDateTime();
	fixTime = rec->gpstime;
	latitude = qRound(rec->latitude * 10000000);
	longitude = qRound(rec->longitude * 10000000);
	altitude = rec->altitude;
	speed = rec->speed;
	heading = rec->heading;
	hdop = rec->hdop;
	fix_status = 0;
	user_msg = determineNmeaMessage(msg);
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

	}

	return QString();
}


QByteArray CalAmpsUserMessage::data()
{
	QByteArray data = QByteArray();
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
	data[offset+26] = 0; //Satilites
	data[offset+27] = fix_status; // Fix Status
	data[offset+28] = 0; // Carrier MS byte
	data[offset+29] = 0; // Carrier LS byte
	data[offset+30] = 0; // RSSI MS byte
	data[offset+31] = 0; // RSSI LS byte
	data[offset+32] = 0; // Comm State
	data[offset+33] = hdop;
	data[offset+34] = 1; // Inputs
	data[offset+35] = 0; // Unit Status
	data[offset+36] = 0; // User Msg Route 0 = User 0(Host) Port
	data[offset+37] = user_msg_id;
	data16 = qToBigEndian((qint16)(user_msg.length()));
	for(int i=0; i<2; i++)
		data[offset+38+i]=((uchar *)&data16)[i];
	data.append(user_msg);
	return data;
}
