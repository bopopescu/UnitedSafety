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

class SafetyLinkMessage : public CalAmpsMessage
{
public:
    SafetyLinkMessage(ats::StringMap& data);
    virtual void WriteData(QByteArray& );
    virtual ats::String getMessage();
private:
    CALAMP_EVENT_CODE determineEventCode(QString);
    QDateTime	updateTime;
    QDateTime	fixTime;
    qint32		latitude;
    qint32		longitude;
    qint32		altitude;
    qint32		speed;
    qint16		heading;
    qint16    h_accuracy;
    qint16    v_accuracy;
    quint8		satellites;
    quint8		fix_status;
    quint16		unit_status;
    quint16		event_type;
};
