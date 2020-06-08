#include <QSocketNotifier>
#include <QMutex>
#include <QQueue>
#include <QMap>

#include <map>
#include <string>
#include <sstream>

#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <syslog.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "ats-common.h"
#include "atslogger.h"
#include "datacollector.h"
#include "ipcserver.h"

int g_dbg = 0;
extern ATSLogger g_log;

bool IpcServer::m_seatbelt_buckled = false;
QString IpcServer::m_rpm = "0";
QString IpcServer::m_speed = "0";
IpcServer* IpcServer::m_ipc_server = 0;
static pthread_mutex_t g_mutex;

IpcServer::EventTypeMap IpcServer::m_event_type;
IpcServer::MessageTypeMap IpcServer::m_msg_type;

void IpcServer::TrakEvent::on_event(IpcServer&, const TrakEvent&, const CommandBuffer&)
{
}

void IpcServer::TrakEvent::on_event_rpm(IpcServer& p_ipc, const TrakEvent&, const CommandBuffer& p_cb)
{

	if(p_cb.m_argc >= 2)
	{
		pthread_mutex_lock(&g_mutex);
		const char* val = p_cb.m_argv[1];
		p_ipc.m_rpm = val;
		DataCollector::setRpm(p_ipc.m_rpm);
		pthread_mutex_unlock(&g_mutex);
	}

}

void IpcServer::TrakEvent::on_event_speed(IpcServer& p_ipc, const TrakEvent&, const CommandBuffer& p_cb)
{

	if(p_cb.m_argc >= 2)
	{
		pthread_mutex_lock(&g_mutex);
		const char* val = p_cb.m_argv[1];
		p_ipc.m_speed = val;
		DataCollector::setSpeed(p_ipc.m_speed);
		pthread_mutex_unlock(&g_mutex);
	}

}

void IpcServer::TrakEvent::on_event_seatbelt(IpcServer&, const TrakEvent&, const CommandBuffer& p_cb)
{

	if(p_cb.m_argc >= 2)
	{
		pthread_mutex_lock(&g_mutex);
		const char* val = p_cb.m_argv[1];
		m_seatbelt_buckled = strtol(val, 0, 0) ? true : false;
		DataCollector::setSeatbelt(m_seatbelt_buckled);
		pthread_mutex_unlock(&g_mutex);
	}

}

void IpcServer::TrakMessage::on_proc(IpcServer&, const TrakMessage& p_msg, const ats::StringMap& p_sm)
{
	DataCollector::insertMessage(p_msg.m_id, p_sm);
}

void IpcServer::TrakMessage::on_proc_ignition_on(IpcServer&, const TrakMessage& p_msg, const ats::StringMap& p_sm)
{
	pthread_mutex_lock(&g_mutex);
	DataCollector::setIgnition(true);
	pthread_mutex_unlock(&g_mutex);
	DataCollector::insertMessage(p_msg.m_id, p_sm);
}

void IpcServer::TrakMessage::on_proc_ignition_off(IpcServer&, const TrakMessage& p_msg, const ats::StringMap& p_sm)
{
	pthread_mutex_lock(&g_mutex);
	DataCollector::setIgnition(false);
	pthread_mutex_unlock(&g_mutex);
	DataCollector::insertMessage(p_msg.m_id, p_sm);
}

IpcServer::IpcServer(QObject *parent) :
	QObject(parent)
{
	DataCollector::init();
}

void IpcServer::init()
{
	// ATS FIXME: Where do the event numbers (P_num) come from? Replace the numbers with ENUM or named
	//	variable/definition.
	#define TMP_EVENT(P_msg, P_num, P_fn) \
		m_event_type.insert(EventTypePair(#P_msg, TrakEvent(P_num, TrakEvent::P_fn)))
	TMP_EVENT(speed, 0, on_event_speed);
	TMP_EVENT(rpm, 1, on_event_rpm);
	TMP_EVENT(seatbelt, 2, on_event_seatbelt);
	TMP_EVENT(odometer, 3, on_event);
	TMP_EVENT(ping, 4, on_event);
	TMP_EVENT(ignition_on, 5, on_event);
	TMP_EVENT(ignition_off, 6, on_event);
	TMP_EVENT(start_cond, 7, on_event);
	TMP_EVENT(stop_cond, 8, on_event);
	TMP_EVENT(scheduled, 9, on_event);
	TMP_EVENT(speed_exceeded, 10, on_event);
	TMP_EVENT(acceptable_speed, 11, on_event);
	TMP_EVENT(sensor, 12, on_event);
	TMP_EVENT(heartbeat, 13, on_event);
	TMP_EVENT(power_on, 14, on_event);
	TMP_EVENT(default, 15, on_event);
	#undef TMP_EVENT

	#define TMP_MSG(P_num, P_proc_fn) \
		m_msg_type.insert(MessageTypePair(DataCollector::message_name(P_num).c_str(), \
			TrakMessage(P_num, TrakMessage::P_proc_fn)))
	TMP_MSG(TRAK_SCHEDULED_MSG, on_proc);
	TMP_MSG(TRAK_SPEED_EXCEEDED_MSG, on_proc);
	TMP_MSG(TRAK_PING_MSG, on_proc);
	TMP_MSG(TRAK_STOP_COND_MSG, on_proc);
	TMP_MSG(TRAK_START_COND_MSG, on_proc);
	TMP_MSG(TRAK_IGNITION_ON_MSG, on_proc);
	TMP_MSG(TRAK_IGNITION_OFF_MSG, on_proc);
	TMP_MSG(TRAK_HEARTBEAT_MSG, on_proc);
	TMP_MSG(TRAK_SENSOR_MSG, on_proc);
	TMP_MSG(TRAK_POWER_ON_MSG, on_proc);
	TMP_MSG(TRAK_ACCEPTABLE_SPEED_MSG, on_proc);
	TMP_MSG(TRAK_TEXT_MSG, on_proc);
	TMP_MSG(TRAK_DIRECTION_CHANGE_MSG, on_proc);
	TMP_MSG(TRAK_ACCELERATION_MSG, on_proc);
	TMP_MSG(TRAK_HARD_BRAKE_MSG, on_proc);
	TMP_MSG(TRAK_SOS_MSG, on_proc);
	TMP_MSG(TRAK_HELP_MSG, on_proc);
	TMP_MSG(TRAK_OK_MSG, on_proc);
	TMP_MSG(TRAK_POWER_OFF_MSG, on_proc);
	TMP_MSG(TRAK_CHECK_IN_MSG, on_proc);
	TMP_MSG(TRAK_FALL_DETECTED_MSG, on_proc);
	TMP_MSG(TRAK_CHECK_OUT_MSG, on_proc);
	TMP_MSG(TRAK_NOT_CHECK_IN_MSG, on_proc);
	TMP_MSG(TRAK_GPSFIX_INVALID_MSG, on_proc);
	TMP_MSG(TRAK_FUEL_LOG_MSG, on_proc);
	TMP_MSG(TRAK_DRIVER_STATUS_MSG, on_proc);
	TMP_MSG(TRAK_ENGINE_ON_MSG, on_proc);
	TMP_MSG(TRAK_ENGINE_OFF_MSG, on_proc);
	TMP_MSG(TRAK_ENGINE_TROUBLE_CODE_MSG, on_proc);
	TMP_MSG(TRAK_ENGINE_PARAM_EXCEED_MSG, on_proc);
	TMP_MSG(TRAK_ENGINE_PERIOD_REPORT_MSG, on_proc);
	TMP_MSG(TRAK_OTHER_MSG, on_proc);
	TMP_MSG(TRAK_SWITCH_INT_POWER_MSG, on_proc);
	TMP_MSG(TRAK_SWITCH_WIRED_POWER_MSG, on_proc);
	TMP_MSG(TRAK_ODOMETER_UPDATE_MSG, on_proc);
	TMP_MSG(TRAK_ACCEPT_ACCEL_RESUMED_MSG, on_proc);
	TMP_MSG(TRAK_ACCEPT_DECCEL_RESUMED_MSG, on_proc);
	TMP_MSG(TRAK_ENGINE_PARAM_NORMAL_MSG, on_proc);
	TMP_MSG(TRAK_J1939_MSG, on_proc);
	TMP_MSG(TRAK_J1939_FAULT_MSG, on_proc);
	TMP_MSG(TRAK_J1939_STATUS2_MSG, on_proc);
	TMP_MSG(TRAK_IRIDIUM_OVERLIMIT_MSG, on_proc);
	TMP_MSG(TRAK_CALAMP_USER_MSG, on_proc);
	TMP_MSG(TRAK_INET_ERROR, on_proc);
	TMP_MSG(TRAK_INET_MSG, on_proc);
	TMP_MSG(TRAK_LOW_BATTERY_MSG, on_proc);
	TMP_MSG(TRAK_SEATBELT_ON, on_proc);
	TMP_MSG(TRAK_SEATBELT_OFF, on_proc);
	TMP_MSG(TRAK_CRITICAL_BATTERY_MSG, on_proc);
	#undef TMP_MSG

	m_ipc_server = new IpcServer(0);
	pthread_mutex_init(&g_mutex, 0);
	init_ServerData(&m_client_server, 64);
	m_client_server.m_cs = IpcServer::serv_client;

	m_client_server.m_shutdown_callback = IpcServer::shutdown_callback;
}

int IpcServer::msg_to_id(const QString& p_msg)
{
	MessageTypeMap::const_iterator i = m_msg_type.find(p_msg);

	if(i != m_msg_type.end())
	{
		const TrakMessage& tm = i->second;
		return tm.m_id;
	}

	return -1;
}

void IpcServer::start()
{

	if(start_redstone_ud_server(&m_client_server, "message-assembler", 1))
	{
		ats_logf(ATSLOG_ERROR, "%s,%d: Error starting client/device server: %s", __FILE__, __LINE__, m_client_server.m_emsg);
		exit(1);
	}

	signal_app_unix_socket_ready("message-assembler", "message-assembler");
	signal_app_ready("message-assembler");
}

void* IpcServer::serv_client(void* p)
{
	const size_t max_cmd_length = 2048;
	ClientData& cd = *((ClientData*)p);

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
				ats_logf(ATSLOG_ERROR, "Client %p: client_getc failed: %s", &cd, ebuf);
			}
			break;
		}

		if((c != '\r') && (c != '\n'))
		{
			if(cmd.length() >= max_cmd_length)
			{
				command_too_long = true;
			}
			else
			{
				cmd += c;
			}

			continue;
		}

		if(command_too_long)
		{
			ats_logf(ATSLOG_DEBUG, "Client %p: command too long (%64s...)", &cd, cmd.c_str());
			cmd.clear();
			command_too_long = false;
			continue;
		}

		const char* err;
		if((err = gen_arg_list(cmd.c_str(), cmd.length(), &cb)))
		{
			qCritical("Client %p: gen_arg_list failed (%s)", &cd, err);
			ats_logf(ATSLOG_DEBUG, "Client %p: gen_arg_list failed (%s)", &cd, err);
			break;
		}

		cmd.clear();

		if(cb.m_argc < 1)
		{
			continue;
		}

		{
			const ats::String cmd(cb.m_argv[0]);

			IpcServer::EventTypeMap::const_iterator i = IpcServer::m_event_type.find(cmd.c_str());

			if(i != IpcServer::m_event_type.end())
			{
				const IpcServer::TrakEvent& te = i->second;
				te.m_on_event(*(IpcServer::instance()), te, cb);
			}
			else if("test" == cmd)
			{
				send_cmd(cd.m_sockfd, MSG_NOSIGNAL, "%s,%d: Hello, World!\n", __FILE__, __LINE__);
			}
			else if("msg" == cmd)
			{
				if(cb.m_argc >=2)
				{
					IpcServer::instance()->h_processMessageCmd(cb.m_argc - 1, cb.m_argv + 1);
				}
			}
			else if( ("debug" == cmd) || ("dbg" == cmd))
			{
				if(cb.m_argc >= 2)
				{
					g_dbg = strtol(cb.m_argv[1], 0, 0);
				}
				else
				{
					send_cmd(cd.m_sockfd, MSG_NOSIGNAL, "%s=%d\n", cmd.c_str(), g_dbg);
				}

			}
			else if("stats" == cmd)
			{
				// ATS FIXME: Output useful application statistics
			}

		}

	}

	free_dynamic_buffers(&cb);
	shutdown(cd.m_sockfd, SHUT_WR);
	// ATS FIXME: Race-condition exists because "shutdown" does not gaurantee that the receiver will receive all the data.
	close_client(&cd);
	return 0;
}


void IpcServer::h_processMessageCmd(int p_argc, char* p_argv[])
{
	const QString msg(p_argv[0]);
	ats::StringMap s;
	s.from_args(p_argc - 1, p_argv + 1);

	ats_logf(ATSLOG_INFO, "%s with %d arguments", msg.toLatin1().constData(), p_argc -1);

	MessageTypeMap::const_iterator i = m_msg_type.find(msg);

	if(i != m_msg_type.end())
	{
		const TrakMessage& tm = i->second;
		tm.m_on_proc(*this, tm, s);
	}
	else
		ats_logf(ATSLOG_ERROR, "Unknown message type: %s with %d arguments", msg.toLatin1().constData(), p_argc -1);

}

void* IpcServer::shutdown_callback(ServerData *p_data)
{
	ats_logf(ATSLOG_ERROR, "%s, %d: Error client/server closing server: %s", __FILE__, __LINE__, p_data->m_emsg);
	ats_logf(ATSLOG_ERROR, "Attempting to restart socket server.");
	exit(1);
	return 0;
}
