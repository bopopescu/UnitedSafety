#pragma once

#include "ats-common.h"
#include "calampsmessage.h"

typedef enum
{
	CALAMP_EVENT_IGNITION_ON =		100,
	CALAMP_EVENT_IGNITION_OFF =		101,
	CALAMP_EVENT_POSITION_UPDATE =	200,
	CALAMP_EVENT_UNKNOWN =			0
}
CALAMP_EVENT_CODE;

class CalAmpsEventMessage : public CalAmpsMessage
{
public:
	CalAmpsEventMessage(ats::StringMap& data, quint8 code);
	virtual void WriteData(QByteArray& );
private:
	CALAMP_EVENT_CODE determineEventCode(QString);
	QDateTime	updateTime;
	QDateTime	fixTime;
	qint32		latitude;
	qint32		longitude;
	qint32		altitude;
	qint32		speed;
	qint16		heading;
	quint8		satellites;
	quint8		fix_status;
	quint16		carrier;
	qint16		rssi;
	quint8		comm_state;
	quint8		hdop;
	quint8		unit_status;
	quint8		event_index;
	quint8		event_code;
	quint8		accums;
	quint8		spare;
};
