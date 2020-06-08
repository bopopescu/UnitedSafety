#ifndef CALAMPSMESSAGE_H
#define CALAMPSMESSAGE_H

#include <QByteArray>
#include <QtEndian>
#include <QTime>
#include <QDateTime>
//#include "solaraxmldatahandler.h"
#include "datacollector.h"

//OPTION HEADER Option Byte
#define CALAMP_OPTION_HEADER_DEFINE_BIT		0x80
#define CALAMP_OPTION_HEADER_RESP_REDIRCTN	0x20
#define CALAMP_OPTION_HEADER_FORWARDING		0x10
#define CALAMP_OPTION_HEADER_ROUTING		0x08
#define CALAMP_OPTION_HEADER_AUTHEN_WORD	0x04
#define CALAMP_OPTION_HEADER_MOBILE_ID_TYPE	0x02
#define CALAMP_OPTION_HEADER_MOBILE_ID		0x01

typedef enum {
	CALAMP_SERVICE_UNACKNOWLEDGED_REQUEST =	0,
	CALAMP_SERVICE_ACKNOWLEDGED_REQUEST =	1,
	CALAMP_SERVICE_RESPONSE =				2
} CALAMP_SERVICE_TYPE;


typedef enum {
	CALAMP_MESSAGE_NULL =						0,
	CALAMP_MESSAGE_ACK_NACK =					1,
	CALAMP_MESSAGE_EVENT_REPORT =				2,
	CALAMP_MESSAGE_ID_REPORT =					3,
	CALAMP_MESSAGE_USER_DATA =					4,
	CALAMP_MESSAGE_APPLICATION_DATA	=			5,
	CALAMP_MESSAGE_CONFIG_PARAM =				6,
	CALAMP_MESSAGE_UNIT_REQUEST =				7,
	CALAMP_MESSAGE_LOCATE_REPORT =				8,
	CALAMP_MESSAGE_USER_DATA_W_ACCUMULATORS =	9,
	CALAMP_MESSAGE_MINI_EVENT_REPORT =			10
} CALAMP_MESSAGE_TYPE;

typedef enum {
	CALAMP_GPS_FIX_INVALID =	0x08,
	CALAMP_GPS_FIX_2D =			0x10,
	CALAMP_GPS_FIX_3D =			0x01
} CALAMP_GPS_FIX_STATUS;

class CalAmpsMessage
{
public:
	CalAmpsMessage();
	//CalAmpsMessage(struct solara_record *);
	CalAmpsMessage(QByteArray );
// Set Methods
	void				setSequenceNumber(quint16 num);
//Get Methods
	virtual QByteArray	data(void) = 0;
	CALAMP_SERVICE_TYPE	getServiceType(void);
	CALAMP_MESSAGE_TYPE	getMessageType(void);
	QString				getImei(void) {return imei;}
protected:
	quint8		getFixStatus(QString);
	QByteArray	convertToBCD(QString );

	QByteArray	optionsHeader;
	QByteArray	messageHeader;
	QString		imei;
	quint16		sequence_num;


};

#endif // CALAMPSMESSAGE_H
