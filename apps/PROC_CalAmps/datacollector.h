/******************************************************************************
* DataCollector Class														  *
* This class retrieves information from the device to be sent to sent to the  *
* server                                                                      *
******************************************************************************/

#ifndef DATACOLLECTOR_H
#define DATACOLLECTOR_H

#include <QTime>
#include <QObject>
#include <ipcserver.h>
#include <NMEA_Client.h>

#define DC_RPM_DATA_FILE "/tmp/ipc/CANTrak/rpm"
#define DC_SPEED_DATA_FILE "/tmp/ipc/CANTrak/speed_kph"
#define	DC_IMEI_DATA_FILE "/tmp/config/gobi-imei"

#define DEFAULT_IMEI "300234010957620"

struct gps_coord{
	int longitude;
	int latitude;
};

struct dev_data{
	QString		esn;
	double		latitude;
	double		longitude;
	int			speed;
	int			altitude;
	int			heading;
	QString		fix_type;
	int			hdop;
	QDateTime	gpstime;
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
	double	getLat(){return nmea_client.Lat();}
	double	getLon(){return nmea_client.Lon();}
	double	getHeading(){return nmea_client.COG();}
	QString getCurrentSpeed();
	QString getCurrentRpm();
	qint8	getDigitalRegister();
	QString getBatteryVoltage();
	bool	getSeatbeltStatus();
	void	getDevData(struct dev_data *);

signals:
	void eventReceived(int);

public slots:
	void processData(int);

private:
	QString speed;
	QString rpm;
	QString imei;
	bool seatbelt_buckled;
	IpcServer ipc_server;
	NMEA_Client nmea_client;

	bool readImei(void);

};

#endif // DATACOLLECTOR_H
