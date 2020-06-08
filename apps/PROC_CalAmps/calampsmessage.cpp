#include "calampsmessage.h"

CalAmpsMessage::CalAmpsMessage()
{


}



quint8 CalAmpsMessage::getFixStatus(QString fix_type)
{
	if(fix_type == "Invalid")
		return CALAMP_GPS_FIX_INVALID;

	if(fix_type == "2D")
		return CALAMP_GPS_FIX_2D;

	if(fix_type == "3D")
		return CALAMP_GPS_FIX_3D;

	return 0;
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
