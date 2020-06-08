#include <QtCore/QCoreApplication>
#include "BSP_WiFi.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

	BSP_WiFi *bsp = new BSP_WiFi();
	bsp->m_bsp->wait_for_pc_connect();

    return a.exec();
}
