/******************************************************************************
* DataCollector Class														  *
* This class retrieves information from the device to be sent to sent to the  *
* server                                                                      *
******************************************************************************/

#ifndef DATACOLLECTOR_H
#define DATACOLLECTOR_H

#include <QTime>
#include <QObject>

struct gps_coord{
	int longitude;
	int latitude;
};

class DataCollector : public QObject
{
	Q_OBJECT
public:
	explicit DataCollector(QObject *parent = 0);
    QString getImei(void);
	QString getNmeaGGA(void);
	QString getNmeaHDT(void);
	QString getNmeaRMC(void);
	QString getCurrentSpeed();
	QString getCurrentRpm();
    qint8   getDigitalRegister();
	QString getBatteryVoltage();
	bool    getSeatbeltStatus();
signals:
	
public slots:

private:

	
};

#endif // DATACOLLECTOR_H
