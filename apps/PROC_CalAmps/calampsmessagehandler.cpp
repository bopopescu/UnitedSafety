#include <QFile>
#include <QTimer>
#include <QtXml>
#include <iostream>
#include "calampsmessagehandler.h"

CalAmpsMessageHandler::CalAmpsMessageHandler(QObject *parent) :
	QObject(parent)
{
	settings["host"] = "";
	settings["port"] = "";
	settings["timeout"] = "";

	loadCalAmpSettings("calamp_settings.xml");
}

bool CalAmpsMessageHandler::loadCalAmpSettings(QString filename)
{
	QFile file(filename);
	if(!file.exists())
		return false;
	if (!file.open(QIODevice::ReadOnly))
		return false;


	QXmlStreamReader xmlSrc;
	xmlSrc.setDevice(&file);
	while(!xmlSrc.atEnd())
	{
		bool res = xmlSrc.readNext();
		if(!res)
			break;
		if(settings.contains(xmlSrc.name().toString()))
			settings[xmlSrc.name().toString()] = xmlSrc.readElementText();
	}
	if(xmlSrc.hasError())
	{
		QString error = QString("ERROR:") +xmlSrc.error() + "-" + xmlSrc.errorString();
		std::cout << error.toUtf8().constData();
		return false;
	}
	file.close();
	return true;
}


bool CalAmpsMessageHandler::sendMessage(CalAmpsMessage &msg)
{
	calAmpsConnection = new QUdpSocket(this);
	calAmpsConnection->connectToHost(settings["host"],settings["port"].toInt(), QIODevice::ReadWrite);
	if(sequence_number_map.contains(msg.getImei()))
		msg.setSequenceNumber(++sequence_number_map[msg.getImei()]);
	else
	{
		msg.setSequenceNumber(1);
		sequence_number_map[msg.getImei()] = 1;
	}

	QByteArray write_data = QByteArray(msg.data());

	if(calAmpsConnection->write(write_data) < write_data.length())
		return false;

	//Get Ack/Nack size excl header 6 bytes
	int ack = 0;
	QByteArray buf = QByteArray();
	QTimer msg_tmout;
	msg_tmout.setSingleShot(true);
	msg_tmout.start(CALAMPS_SERVER_TMOUT);
	do
	{
		calAmpsConnection->waitForReadyRead();
		if(!msg_tmout.isActive())
			ack = -1;
		buf.append(calAmpsConnection->read(CALAMPS_MESSAGE_MAX_SIZE));
		ack=processAckMessage(buf);
	}
	while(!ack);
	calAmpsConnection->close();
	if((ack < 0) || (ack > 1))
		return false;

	return true;
}

// **********************************************************
//	processAckMessage()
//
//  Returns 0	- needs more data
//			1	- ACK
//			-1	- Error in ACK message
//			>1	- NAK
//***********************************************************
int CalAmpsMessageHandler::processAckMessage(QByteArray &data)
{
	int data_size = data.length();
	if(data_size < CALAMPS_ACK_MESSAGE_MIN_SIZE)
		return 0;
	int offset = 0;
	if(data[offset] & CALAMP_OPTION_HEADER_DEFINE_BIT)
	{
		offset++;
		quint8 mask =1;
		for(int i=0; i<6; i++)
		{
			if(data[0] & mask)
				offset += data[offset] + 1;
			if(data_size < (offset+1))
				return 0;
			mask = mask << 1;
		}
	}
	if((offset + CALAMPS_ACK_MESSAGE_MIN_SIZE) > data_size)
		return 0;
	if((uint)(data[offset + 1]) != CALAMP_MESSAGE_ACK_NACK)
		return -1;
	return data[offset + 5] + 1;
}
