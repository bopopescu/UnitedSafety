#ifndef TRAKMESSAGEHANDLER_H
#define TRAKMESSAGEHANDLER_H

#include <QObject>
#include <QQueue>
#include "trakmessage.h"

#define CONNECTION_TIMEOUT 5000

class TrakMessageHandler : public QObject
{
	Q_OBJECT
public:
	explicit TrakMessageHandler(QObject *parent = 0);
    ~TrakMessageHandler();
    void sendMessage(TRAK_MESSAGE_TYPE msg_type);
signals:
	
private slots:
	void dequeueMessage(qint64);
private:
    DataCollector datacollector;
    QQueue<TrakMessage *> messageQueue;
	bool busy;
	quint32 sequence_num;
	QString host;
	quint16 port;
};

#endif // TRAKMESSAGEHANDLER_H
