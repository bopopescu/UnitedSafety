#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QHostInfo>
#include "trakmessagehandler.h"
#include "trakconfig.h"


TrakMessageHandler::TrakMessageHandler(QObject *parent) :
	QObject(parent)
{
	host = TrakConfig::readParam("ct_host");
	port = TrakConfig::readParam("ct_port").toUInt();
	busy = false;
	sequence_num = 1;
}

TrakMessageHandler::~TrakMessageHandler()
{
	while(!messageQueue.isEmpty())
	{
		dequeueMessage(0);
	}
}


void TrakMessageHandler::dequeueMessage(qint64)
{
	TrakMessage * currMessage;
	currMessage = messageQueue.dequeue();
	delete currMessage;
	busy = false;
}

void TrakMessageHandler::sendMessage(TRAK_MESSAGE_TYPE msg_type)
{
	if(messageQueue.isEmpty())
	{
		//connect
		QTcpSocket trak_conn;

		trak_conn.connectToHost(host,port);
		if(!trak_conn.waitForConnected(CONNECTION_TIMEOUT))
		{
			qDebug()<<"TrakMessageHandler::sendMessage() - Cannot connect to " <<
					  host <<":"<<port<< "Error:"<< trak_conn.errorString()<<".\n";
			return;
		}
		qDebug()<<"TrakMessageHandler::sendMessage() - Connected to "<< host << ":"<<port<<".\n";
		//send message
		TrakMessage message(msg_type);
		message.setSequenceNum(sequence_num++);
		trak_conn.write(message.data());
		busy = true;
		qDebug() << "TrakMessageHandle::sendMessage() - Sending raw data:" << message.data().toHex() << "\n";
		if(!trak_conn.waitForBytesWritten(CONNECTION_TIMEOUT))
		{
			qDebug()<<"TrakMessageHandler::sendMessage() - Connection timed out writing to " <<
					  host <<":"<<port<< ".\n";
			return;
		}

		//read ack

		if(!trak_conn.waitForReadyRead())
		{
			qDebug()<<"TrakMessageHandler::sendMessage() - Connection timed out wating for ready read on " <<
					  host <<":"<<port<< ".\n";
			return;
		}

		QByteArray buf;
		while(buf.length() < 4)
		{
			buf = trak_conn.readAll();
			qDebug()<<"TrakMessageHandler::sendMessage() - Reading bytes:"<< buf.toHex() << ".\n";
		}

		//close connection
		trak_conn.close();
	}

}
