#include "datacollector.h"

DataCollector::DataCollector(QObject *parent) :
	QObject(parent)
{
	;
}

//Dummy functions that return test data.  Real implementation will retrieve
// information from the device the application is running on.
QString DataCollector::getImei()
{

	return "300234010957620";
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

    return "32";
}

QString DataCollector::getCurrentRpm()
{
	return "0";
}


QString DataCollector::getBatteryVoltage()
{
    return "23";
}

qint8 DataCollector::getDigitalRegister()
{

	return 0x0;
}

bool DataCollector::getSeatbeltStatus()
{
	return false;
}
