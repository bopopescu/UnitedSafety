#pragma once

#include "calampsmessage.h"
#include "ats-common.h"

#define USER_MESSAGE_SIZE_MIN 44
#define USER_MESSAGE_SIZE_MAX 848

//User Message Defines
#define CALAMP_USER_MESSAGE_CHECK_IN		"PGEMCI*15"
#define CALAMP_USER_MESSAGE_ASSIST			"PGEMAS*0D"
#define CALAMP_USER_MESSAGE_ASSIST_CANCEL	"PGEMAC*1D"
#define CALAMP_USER_MESSAGE_SOS				"PGEMEM*17"
#define	CALAMP_USER_MESSAGE_SOS_CANCEL		"PGEMEC*19"
#define	CALAMP_USER_MESSAGE_MONITOR_OFF		"PGEMMO*1D"
#define	CALAMP_USER_MESSAGE_STATE_REQUEST	"PGEMCC*1F"
#define CALAMP_USER_AUTHENTICATION_REQUEST	"PGEMAU,"

typedef enum {
	CALAMP_WAT_CHECK_IN_MESSAGE,
	CALAMP_WAT_ASSIST_MESSAGE,
	CALAMP_WAT_ASSIST_CANCEL_MESSAGE,
	CALAMP_WAT_SOS_MESSAGE,
	CALAMP_WAT_SOS_CANCEL_MESSAGE,
	CALAMP_WAT_SOS_CANCEL_HELP_MESSAGE,
	CALAMP_WAT_MONITOR_OFF_MESSAGE,
	CALAMP_WAT_STATE_REQUEST_MESSAGE,
	CALAMP_WAT_AUTH_REQUEST_MESSAGE
} CALAMP_WAT_MESSAGE;

class CalAmpsUserMessage : public CalAmpsMessage
{
public:
	CalAmpsUserMessage(const ats::StringMap& );
	virtual void WriteData(QByteArray& data);

private:

	QString determineNmeaMessage(CALAMP_WAT_MESSAGE );

	QDateTime	updateTime;
	QDateTime	fixTime;
	qint32		latitude;
	qint32		longitude;
	qint32		altitude;
	qint32		speed;
	qint16		heading;
	quint8		satellites;
	quint8		fix_status;
	qint16		rssi;
	quint8		comm_state;
	quint8		hdop;
	quint8		unit_status;
	quint8		user_msg_route;
	quint8		user_msg_id;
	quint16		user_msg_length;
	QString		user_msg;

};
