#include "ats-common.h"
#include "calampseventmessage.h"

extern const ats::String g_CALAMPDBcolumnname[];

CalAmpsEventMessage::CalAmpsEventMessage(ats::StringMap& data, quint8 event) :
	CalAmpsMessage()
{
	m_mobile_id = QString(data.get(g_CALAMPDBcolumnname[36]).c_str());
	m_mobile_id_type = data.get_int(g_CALAMPDBcolumnname[37]);

	if(m_mobile_id.isEmpty() || ((m_mobile_id_type < 1) && (m_mobile_id_type > 6)))
	{
		auto_setImei();
		m_mobile_id_type = CALAMP_MOBILE_ID_IMEI;
	}

	messageHeader = QByteArray(4, 0);
	messageHeader[0]=1;	// Acknowledged Request
	messageHeader[1]=CALAMP_MESSAGE_EVENT_REPORT; // Event Report Message

	optionsHeader[0] = 0x83; // 1000 0011 bit7: always set,  bit0:Mobile ID, bit1: Mobile ID type
	QByteArray esn = convertToBCD(m_mobile_id);
	optionsHeader[1] = esn.length();
	optionsHeader.append(esn);
	optionsHeader[esn.length()+2] = 1; //always 1 as per CalAMPs spec.
	optionsHeader[esn.length()+3] = m_mobile_id_type; // default is IMEI type
	updateTime = QDateTime::fromString(QString(data.get(g_CALAMPDBcolumnname[2]).c_str()), "yyyy-MM-dd HH:mm:ss");
	fixTime = QDateTime::fromString(QString(data.get(g_CALAMPDBcolumnname[3]).c_str()), "yyyy-MM-dd HH:mm:ss");
	latitude = qRound(data.get_double(g_CALAMPDBcolumnname[4]) * 10000000);
	longitude = qRound(data.get_double(g_CALAMPDBcolumnname[5]) * 10000000);
	altitude = qRound(data.get_double(g_CALAMPDBcolumnname[6]) * 100);
	speed = (data.get_int(g_CALAMPDBcolumnname[7])*1000)/36; //Convert to cm/s
	rssi = (data.get_int(g_CALAMPDBcolumnname[34]));
	heading = qRound(data.get_double(g_CALAMPDBcolumnname[8]));
	hdop = qRound(data.get_double(g_CALAMPDBcolumnname[11]) * 10);
	satellites = data.get_int(g_CALAMPDBcolumnname[9]);
	fix_status = getFixStatus(data.get_int(g_CALAMPDBcolumnname[10]));
	inputs=0x7E;
	encodeInputsField(data.get_int(g_CALAMPDBcolumnname[12]));

  if (event == 100 || event == 101 || event == 200)
		event_index = 1;
	else
		event_index = 254;
	
	event_code = event;
	accums = 0;
	spare = 0;
}

void CalAmpsEventMessage::WriteData(QByteArray& data)
{
	qint16 data16;
	qint32 data32;
	const int offset = optionsHeader.length() + messageHeader.length();
	data.append(optionsHeader);
	data.append(messageHeader);
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
