#ifndef IPCHANDLER_H
#define IPCHANDLER_H
#include <signal.h>
#include <QObject>
#include <QSocketNotifier>
#include <QMutex>
#include "socket_interface.h"
#include "command_line_parser.h"


typedef enum {
	CA_SPEED_EVENT,
	CA_RPM_EVENT,
	CA_SEATBELT_EVENT,
	CA_ODOMETER_EVENT,
	CA_DEFAULT_EVENT

} CA_MAIN_EVENT;

class IpcServer : public QObject
{
	Q_OBJECT
public:
	explicit IpcServer(QObject *parent = 0);

	void start();

	//GET methods
	QString speed() {return m_speed;}
	QString rpm() {return m_rpm;}
	bool seatbelt_buckled() {return m_seatbelt_buckled;}
	static IpcServer *instance() {return m_ipc_server;}
	void emitSignal(int);

	void send_event(CA_MAIN_EVENT p_event, const QString &p_val);
signals:
	void data_recieved(int);

public slots:

private:
	static QString m_speed;
	static QString m_rpm;
	static bool m_seatbelt_buckled;
	static int m_dbg;
	ServerData sd_standard_command;
	static QMutex write_lock;
	static IpcServer * m_ipc_server;

	static void* serv_client(void *);
};

#endif // IPCHANDLER_H
