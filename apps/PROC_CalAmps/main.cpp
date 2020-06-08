#include <QtCore/QCoreApplication>
#include <QQueue>
#include <semaphore.h>
#include "ipcserver.h"

sem_t g_sem;
QQueue <int> g_sig;

extern pthread_mutex_t g_mutex;

int main(int argc, char *argv[])
{
	QCoreApplication a(argc, argv);

	for (;;)
	{
		sem_wait(&g_sem);
		pthread_mutex_lock(&g_mutex);
		IpcServer::instance()->emitSignal(g_sig.dequeue());
		a.processEvents();
		pthread_mutex_unlock(&g_mutex);
	}
	return 0;
}
