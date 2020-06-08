#pragma once

#include <QObject>
#include <QUdpSocket>
#include <QMap>
#include "calampseventmessage.h"
#include "calampsusermessage.h"
#include "calampsmessage.h"

#define CALAMPSERVER 1.1.1.1
#define CALAMPS_ACK_MESSAGE_MIN_SIZE 10
#define CALAMPS_MESSAGE_MAX_SIZE 256

#define CALAMPS_SERVER_TMOUT 30000 //30 sec

class CalAmpsMessageHandler :public QObject
{
	Q_OBJECT

public:
	explicit CalAmpsMessageHandler(QObject *parent=0);
	bool sendMessage(CalAmpsMessage &msg);
private:
	QUdpSocket*				calAmpsConnection;
	QMap<QString, quint16>	sequence_number_map;
	QMap<QString, QString>	settings;

	bool loadCalAmpSettings(QString filename);
	int processAckMessage(QByteArray &);

signals:
public slots:
private slots:
};

