#include <QtCore/QCoreApplication>
#include <QtTest/QTest>
#include "trakmessagehandler.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
	TrakMessageHandler tmh;
	tmh.sendMessage(TRAK_POWER_ON_MSG);
	//QTest::qSleep(2000);
	tmh.sendMessage(TRAK_START_COND_MSG);
	//QTest::qSleep(2000);
	tmh.sendMessage(TRAK_STOP_COND_MSG);
	//QTest::qSleep(2000);
	tmh.sendMessage(TRAK_POWER_OFF_MSG);

	return a.exec();
}
