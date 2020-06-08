#pragma once

#include <QObject>

#include "ipcserver.h"
#include "datacollector.h"

class MessageAssembler: public QObject
{
	Q_OBJECT
public:
	MessageAssembler(QObject* parent=0);
	void start();

public slots:
//	void processData(int);
//	void processMessage(int);
private:
//    DataCollector	m_dc;
	IpcServer		m_ipc_server;
};
