#pragma once

#include <QByteArray>
#include <QtEndian>
#include <QTime>
#include <QDateTime>

#include "ats-common.h"

// ATS FIXME: Why is this not an enum?
//OPTION HEADER Option Byte
#define CALAMP_OPTION_HEADER_DEFINE_BIT     0x80
#define CALAMP_OPTION_HEADER_RESP_REDIRCTN  0x20
#define CALAMP_OPTION_HEADER_FORWARDING     0x10
#define CALAMP_OPTION_HEADER_ROUTING        0x08
#define CALAMP_OPTION_HEADER_AUTHEN_WORD    0x04
#define CALAMP_OPTION_HEADER_MOBILE_ID_TYPE 0x02
#define CALAMP_OPTION_HEADER_MOBILE_ID      0x01

typedef enum {
	CALAMP_SERVICE_UNACKNOWLEDGED_REQUEST =	0,
	CALAMP_SERVICE_ACKNOWLEDGED_REQUEST =   1,
	CALAMP_SERVICE_RESPONSE =               2
} CALAMP_SERVICE_TYPE;

typedef enum {
	CALAMP_MOBILE_ID_ESN = 1,
	CALAMP_MOBILE_ID_IMEI = 2,
	CALAMP_MOBILE_ID_IMSI = 3,
	CALAMP_MOBILE_ID_USER = 4,
	CALAMP_MOBILE_ID_PHONE = 5,
	CALAMP_MOBILE_ID_IP = 6
} CALAMP_MOBILE_ID_TYPE;

typedef enum {
	CALAMP_MESSAGE_INVALID =                  -1,
	CALAMP_MESSAGE_NULL =                      0,
	CALAMP_MESSAGE_ACK_NACK =                  1,
	CALAMP_MESSAGE_EVENT_REPORT =              2,
	CALAMP_MESSAGE_ID_REPORT =                 3,
	CALAMP_MESSAGE_USER_DATA =                 4,
	CALAMP_MESSAGE_APPLICATION_DATA	=          5,
	CALAMP_MESSAGE_CONFIG_PARAM =              6,
	CALAMP_MESSAGE_UNIT_REQUEST =              7,
	CALAMP_MESSAGE_LOCATE_REPORT =             8,
	CALAMP_MESSAGE_USER_DATA_W_ACCUMULATORS =  9,
	CALAMP_MESSAGE_MINI_EVENT_REPORT =         10
} CALAMP_MESSAGE_TYPE;

typedef enum {
	CALAMP_GPS_FIX_PREDICTION =    0x01,
	CALAMP_GPS_FIX_DIFFERENTIAL =  0x02,
	CALAMP_GPS_FIX_INVALID =       0x08,
	CALAMP_GPS_FIX_2D =            0x10,
	CALAMP_GPS_FIX_HISTORIC =      0x20,
	CALAMP_GPS_FIX_INVALID_TIME =  0x40,
} CALAMP_GPS_FIX_STATUS;

class CalAmpsMessage
{
public:
	CalAmpsMessage();
	CalAmpsMessage(QByteArray &);

// Set Methods
	void setSequenceNumber(quint16 num);
	void setOptionHeader(QByteArray oh);
	void setMessageHeader(QByteArray mh);

//Get Methods
	virtual void        WriteData(QByteArray& ) = 0;
	CALAMP_SERVICE_TYPE getServiceType();
	CALAMP_MESSAGE_TYPE getMessageType();
	QString             getImei() const {return m_mobile_id;}
	static QString      getImei(const QByteArray&);
	static int          getMessageType(const QByteArray&);

	static ats::String  readImei();
	bool                auto_setImei();

protected:
	quint8            getFixStatus(int) const;
	QByteArray        convertToBCD(QString );
	static QByteArray convertfromBCD(const QByteArray&);
	void              encodeInputsField(quint8 p_inputs);
	QByteArray        optionsHeader;
	QByteArray        messageHeader;
	QString           m_mobile_id;
	qint8             m_mobile_id_type;
	quint16           sequence_num;
	quint8            inputs;
};
