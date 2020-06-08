#include <sys/socket.h>
#include <sys/types.h>
#include <syslog.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <QSocketNotifier>
#include "ipcserver.h"
#include "ats-common.h"
#include <QMutex>
#include <pthread.h>
#include <QQueue>
#include <semaphore.h>

extern QQueue<int> g_sig;
extern sem_t g_sem;
int IpcServer::m_dbg = 0;
bool IpcServer::m_seatbelt_buckled = false;
QString IpcServer::m_rpm = "0";
QString IpcServer::m_speed = "0";
IpcServer *IpcServer::m_ipc_server;
QMutex IpcServer::write_lock;
pthread_mutex_t g_mutex;


IpcServer::IpcServer(QObject *parent) :
	QObject(parent)
{
}

void IpcServer::start()
{
	m_ipc_server = new IpcServer(0);
	pthread_mutex_init(&g_mutex, 0);
	ServerData &sd = sd_standard_command;
	init_ServerData(&sd, 8192);
	sd.m_port = 41104;
	sd.m_cs = IpcServer::serv_client;

	if(start_server(&sd_standard_command)) {
		syslog(LOG_ERR, "%s,%d: Error starting client/device server: %s", __FILE__, __LINE__, sd_standard_command.m_emsg);
	}
}

void IpcServer::send_event(CA_MAIN_EVENT p_event, const QString &p_val)
{
	pthread_mutex_lock(&g_mutex);
	{
		write_lock.lock();
		switch(p_event)
		{
		case CA_RPM_EVENT: m_rpm = p_val; break;
		case CA_SPEED_EVENT: m_speed = p_val; break;
		case CA_SEATBELT_EVENT: m_seatbelt_buckled = strtol(p_val.toLatin1().constData(), 0, 0) ? true : false; break;
		default: break;
		}
		write_lock.unlock();
		g_sig.enqueue(p_event);
		sem_post(&g_sem);
	}
	pthread_mutex_unlock(&g_mutex);
}

void * IpcServer::serv_client(void *p)
{
	const size_t max_cmd_length = 2048;
	ClientData &cd = *((ClientData *)p);

	CommandBuffer cb;
	init_CommandBuffer(&cb);
	alloc_dynamic_buffers(&cb, 256, 65536);

	bool command_too_long = false;
	ats::String cmd;

	ClientDataCache cdc;
	init_ClientDataCache(&cdc);

	for(;;)
	{
		char ebuf[256];
		const int c = client_getc_cached(&cd, ebuf, sizeof(ebuf), &cdc);

		if(c < 0)
		{

			if(c != -ENODATA)
			{
				syslog(LOG_ERR, "Client %p: client_getc failed: %s", &cd, ebuf);
			}

			break;
		}

		if((c != '\r') && (c != '\n')) {
			if(cmd.length() >= max_cmd_length) command_too_long = true;
			else cmd += c;
			continue;
		}
		if(command_too_long) {
			syslog(LOG_ERR, "Client %p: command too long (%64s...)", &cd, cmd.c_str());
			cmd.clear();
			command_too_long = false;
			continue;
		}

		const char *err;
		if((err = gen_arg_list(cmd.c_str(), cmd.length(), &cb))) {
			syslog(LOG_ERR, "Client %p: gen_arg_list failed (%s)", &cd, err);
			break;
		}

		const ats::String full_command(cmd);
		cmd.clear();

		if(cb.m_argc < 1) continue;

		{
			const ats::String cmd(cb.m_argv[0]);
			if("test" == cmd)
			{
				send_cmd(cd.m_sockfd, MSG_NOSIGNAL, "%s,%d: Hello, World!\n", __FILE__, __LINE__);
			}
			else if("speed" == cmd)
			{
				if(cb.m_argc >= 2)
					IpcServer::instance()->send_event(CA_SPEED_EVENT, cb.m_argv[1]);
			}
			else if("rpm" == cmd)
			{
				if(cb.m_argc >= 2)
					IpcServer::instance()->send_event(CA_RPM_EVENT, cb.m_argv[1]);
			}
			else if("seatbelt" == cmd)
			{
				if(cb.m_argc >= 2)
					IpcServer::instance()->send_event(CA_SEATBELT_EVENT, cb.m_argv[1]);
			}
			else if( ("debug" == cmd) || ("dbg" == cmd))
			{
				if(cb.m_argc >= 2)
				{
					m_dbg = strtol(cb.m_argv[1], 0, 0);
				}
				else
				{
					send_cmd(cd.m_sockfd, MSG_NOSIGNAL, "%s=%d\n", cmd.c_str(), m_dbg);
				}
			}
		}
	}

	free_dynamic_buffers(&cb);
	shutdown(cd.m_sockfd, SHUT_WR);
	// FIXME: Race-condition exists because "shutdown" does not guarantee that the receiver will receive all the data.
	close_client(&cd);
	return 0;
}

void IpcServer::emitSignal(int signal)
{
	emit data_recieved(signal);
}
