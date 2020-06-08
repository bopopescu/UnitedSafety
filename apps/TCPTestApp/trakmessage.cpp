#include "trakmessage.h"
#include <QRegExp>
#include <QtEndian>
#include <QtDebug>
#include <math.h>

TrakMessage::TrakMessage(TRAK_MESSAGE_TYPE type)
{
	unit_id = dc.getImei();
	sequence_num = 1;
	ack_req = true;
	gps_res =  8;
	msg_code = type;
	timestamp = QDateTime::currentDateTimeUtc();
	QString rmc = dc.getNmeaRMC();
	QRegExp rmc_match("^\\$GPRMC,\\d*,A,([\\d|\\.]+),([N|S]+),([\\d|\\.]+),([E|W]+),[\\d|\\.]*,([\\d|\\.]+),.*$");
	if(rmc_match.exactMatch(rmc))
	{
		latitude = floor(rmc_match.cap(1).toDouble()/100);
		latitude += ((rmc_match.cap(1).toDouble()/100) - latitude)*5/3;
		latitude *= (rmc_match.cap(2) == "S")? -1:1;
		longitude = floor(rmc_match.cap(3).toDouble()/100);
		longitude += ((rmc_match.cap(3).toDouble()/100) - longitude)*5/3;
		longitude *= (rmc_match.cap(4) == "W")? -1:1;
		heading = rmc_match.cap(5).toFloat();

	} else {
		latitude = 0.0;
		longitude = 0.0;
		heading = 0.0;
	}
	curr_speed = dc.getCurrentSpeed().toInt();
	digital_reg = 0x0;
	project_id = 0; //Trakopolis Project
	message_id = 0;
	add_length = 0;
	add_data = QByteArray();
}

QByteArray TrakMessage::data()
{
	QByteArray data;
	{
		quint64 id = qToLittleEndian(unit_id.toLongLong());
		for(int i=0;i<7;i++)
			data.append(((char*)&id)[i]);
	}
	{
		quint32 seqInt = sequence_num;
		for(int i=0;i<4;i++)
			data.append(((char*)&seqInt)[i]);
	}
	(ack_req)?data.append((quint8)gps_res|0x80):data.append((quint8)gps_res&0x7F);
	data.append((quint8)msg_code);
	{
		qDebug()<< "DateTimeString = " << timestamp.toString() << ".\n";
		quint32 datetimeInt = ((quint32)(timestamp.time().hour())<<27);
		datetimeInt |= (timestamp.time().minute()<<21);
		datetimeInt |= (timestamp.time().second()<<15);
		datetimeInt |= ((timestamp.date().year()-2000)<<10);
		datetimeInt |= (timestamp.date().month()<<6);
		datetimeInt |= (timestamp.date().day());
		datetimeInt = qToBigEndian(datetimeInt);
		for(int i=0;i<4;i++)
			data.append(((char*)&datetimeInt)[i]);
	}
	{
		quint32 lat_int = qToBigEndian(abs((int)(latitude*90000))) >> 8;
		lat_int |= (latitude <  0)? 0x00000080 : 0x0;
		for(int i=0;i<3;i++)
			data.append(((char *)&lat_int)[i]);
	}
	{
		quint32 long_int = qToBigEndian(abs((uint)(longitude*45000))) >> 8;
		long_int |= (longitude < 0)? 0x00000080 : 0x0;
		for(int i=0;i<3;i++)
			data.append(((char *)&long_int)[i]);
	}
	data.append((quint8)(heading /2));
	data.append((quint8)curr_speed);
	data.append(digital_reg);
	data.append((char)0x00);
	data.append((char)0x00);
	data.append((char)0x00);
	data.append((char)0x00);
	data.append((char)project_id);
	data.append((char)message_id);
	data.append(0x77); //manufacturer id
	qint16 len = qToBigEndian(add_length);
	for(int i=0;i<2;i++)
		data.append(((char *)&len)[i]);
	data.append(add_data);

	return data;
}

void TrakMessage::setSequenceNum(uint num)
{
	sequence_num = num;
}

void TrakMessage::setupMessage(TRAK_MESSAGE_TYPE type)
{
	switch(type)
	{
	case TRAK_ACCEPTABLE_SPEED_MSG:
		message_id = 1;
		add_length = 8;
		add_data.append((char)0x0);
		break;
	case TRAK_TEXT_MSG:
		message_id = 2;
		add_length = 0;
		break;
	case TRAK_CHECK_IN_MSG:
	case TRAK_CHECK_OUT_MSG:
	case TRAK_NOT_CHECK_IN_MSG:
		message_id = 3;
		add_length = 16;
		add_data.append((char)0x0);
		add_data.append((char)0x0);
		break;
	case TRAK_FUEL_LOG_MSG:
		message_id = 4;
		add_length = 16;
		add_data.append((char)0x0);
		add_data.append((char)0x0);
		break;
	case TRAK_DRIVER_STATUS_MSG:
		message_id = 5;
		add_length = 16;
		add_data.append((char)0x0);
		add_data.append((char)0x0);
		break;
	case TRAK_ODOMETER_UPDATE_MSG:
		message_id = 6;
		add_length = 16;
		add_data.append((char)0x0);
		add_data.append((char)0x0);
		break;
	case TRAK_SPEED_EXCEEDED_MSG:
	case TRAK_PING_MSG:
	case TRAK_STOP_COND_MSG:
	case TRAK_START_COND_MSG:
	case TRAK_IGNITION_ON_MSG:
	case TRAK_IGNITION_OFF_MSG:
	case TRAK_HEARTBEAT_MSG:
	case TRAK_SENSOR_MSG:
	case TRAK_POWER_ON_MSG:
	case TRAK_DIRECTION_CHANGE_MSG:
	case TRAK_ACCELERATION_MSG:
	case TRAK_HARD_BRAKE_MSG:
	case TRAK_SOS_MSG:
	case TRAK_HELP_MSG:
	case TRAK_OK_MSG:
	case TRAK_POWER_OFF_MSG:
	case TRAK_FALL_DETECTED_MSG:
	case TRAK_GPSFIX_INVALID_MSG:
	case TRAK_ENGINE_ON_MSG:
	case TRAK_ENGINE_OFF_MSG:
	case TRAK_ENGINE_TROUBLE_CODE_MSG:
	case TRAK_ENGINE_PARAM_EXCEED_MSG:
	case TRAK_ENGINE_PERIOD_REPORT_MSG:
	case TRAK_OTHER_MSG:
	case TRAK_SWITCH_INT_POWER_MSG:
	case TRAK_SWITCH_WIRED_POWER_MSG:
	case TRAK_ACCEPT_ACCEL_RESUMED_MSG:
	case TRAK_ACCEPT_DECCEL_RESUMED_MSG:
	case TRAK_ENGINE_PARAM_NORMAL_MSG:
	default:
		message_id = 0;
		add_length = 0;
	}

}
