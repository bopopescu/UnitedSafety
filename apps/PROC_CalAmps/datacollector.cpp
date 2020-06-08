#include "datacollector.h"

#include "datacollector.h"
#include "ats-common.h"
#include "QDebug"
#include <QFile>

DataCollector::DataCollector(QObject *parent) :
	QObject(parent)
{
	speed = "0";
	rpm = "0";
	imei = "";
	seatbelt_buckled = false;
	readImei();
	ipc_server.start();
	connect(ipc_server.instance(), SIGNAL(data_recieved(int)), this, SLOT(processData(int)));
	//
}

bool DataCollector::readImei()
{
	QFile imei_file(DC_IMEI_DATA_FILE);
	if(imei_file.exists())
	{
		imei_file.open(QIODevice::ReadOnly);
		imei = QString(imei_file.readAll()).trimmed();
		imei_file.close();
		return true;
	}

	return false;
}


//Dummy functions that return test data.  Real implementation will retrieve
// information from the device the application is running on.
QString DataCollector::getImei()
{
	if(imei.isEmpty())
	{
		if(readImei())
			return imei;

		return DEFAULT_IMEI;
	}

	return imei;
}

void DataCollector::getDevData(struct dev_data* data)
{
	data->esn = imei;
	data->latitude = nmea_client.Lat();
	data->longitude = nmea_client.Lon();
	data->hdop = nmea_client.HDOP();
	data->heading = nmea_client.COG();
	data->altitude = nmea_client.H_ortho();
	data->gpstime.fromString(QString("%1-%2-%3T%4:%5:%6Z").arg("20"+QString::number(nmea_client.Year()))
							 .arg(QString::number(nmea_client.Month()), 2,'0')
							 .arg(QString::number(nmea_client.Day()), 2, '0')
							 .arg(QString::number(nmea_client.Hour()), 2, '0')
							 .arg(QString::number(nmea_client.Minute()), 2, '0')
							 .arg(QString::number((int)(nmea_client.Seconds())), 2, '0'), Qt::ISODate);
	data->speed = speed.toUInt();
	data->fix_type = nmea_client.GPS_Quality();
}

QString DataCollector::getNmeaGGA()
{
	return "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47";
}

QString DataCollector::getNmeaHDT()
{
	return "";
}


QString DataCollector::getNmeaRMC()
{

	return "$GPRMC,220516,A,5133.82,N,00042.24,W,173.8,231.8,130694,004.2,W*70";
}

QString DataCollector::getCurrentSpeed()
{

	return speed;
}

QString DataCollector::getCurrentRpm()
{
	return rpm;
}


QString DataCollector::getBatteryVoltage()
{
	return "23";
}

qint8 DataCollector::getDigitalRegister()
{
	qint8 ret = 0x0;

	//seatbelt;
	if(seatbelt_buckled) ret |= 0x2;

	return ret;
}

bool DataCollector::getSeatbeltStatus()
{
	return seatbelt_buckled;
}

void DataCollector::processData(int event)
{
	switch(event)
	{
	case CA_SPEED_EVENT:
		speed = ipc_server.speed();
		//qDebug()<<"Speed Event occured.\nSpeed: " << speed <<"\n";
		break;
	case CA_RPM_EVENT:
		rpm = QString::number(ipc_server.rpm().toUInt()/4);
		//qDebug()<<"Rpm Event occured.\nRPM: " << rpm << "\n";
		break;
	case CA_SEATBELT_EVENT:
		seatbelt_buckled = ipc_server.seatbelt_buckled();
		//qDebug()<<"Seatbelt Event occured.\n";
		break;
	default:
		;
	}
	emit eventReceived(event);

}
