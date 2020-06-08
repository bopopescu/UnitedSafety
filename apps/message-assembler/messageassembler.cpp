
#include "messageassembler.h"


MessageAssembler::MessageAssembler(QObject *parent) :
	QObject(parent)
{
}

void MessageAssembler::start()
{
	m_ipc_server.init();
	m_ipc_server.start();
}
