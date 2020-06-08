#pragma once

#include <QObject>
#include <QSocketNotifier>
#include <QMutex>

#include <map>

#include <signal.h>

#include "socket_interface.h"
#include "command_line_parser.h"

#include "messagetypes.h"

class IpcServer : public QObject
{
	Q_OBJECT
public:
	explicit IpcServer(QObject* parent = 0);
	void init();
	void start();

	//GET methods
	const QString& speed() const
	{
		return m_speed;
	}

	const QString& rpm() const
	{
		return m_rpm;
	}

	bool seatbelt_buckled() const
	{
		return m_seatbelt_buckled;
	}

	static IpcServer* instance()
	{
		return m_ipc_server;
	}

	void emitSignal(int);

	class TrakEvent
	{
	public:

		TrakEvent()
		{
			m_id = -1;
			m_on_event = on_event;
		}

		TrakEvent(
			int p_id,
			void (*p_on_event)(IpcServer&, const TrakEvent&, const CommandBuffer&))
		{
			m_id = p_id;
			m_on_event = p_on_event;
		}

		int m_id;
		void (*m_on_event)(IpcServer&, const TrakEvent&, const CommandBuffer&);

		static void on_event(IpcServer&, const TrakEvent&, const CommandBuffer&);
		static void on_event_speed(IpcServer&, const TrakEvent&, const CommandBuffer&);
		static void on_event_rpm(IpcServer&, const TrakEvent&, const CommandBuffer&);
		static void on_event_seatbelt(IpcServer&, const TrakEvent&, const CommandBuffer&);
	};


	class TrakMessage
	{
	public:

		TrakMessage()
		{
			m_id = -1;
			m_on_proc = on_proc;
		}

		TrakMessage(
			int p_id,
			void (*p_on_proc)(IpcServer&, const TrakMessage&, const ats::StringMap& p_sm)
			)
		{
			m_id = p_id;
			m_on_proc = p_on_proc;
		}

		int m_id;
		void (*m_on_proc)(IpcServer&, const TrakMessage&, const ats::StringMap& p_sm);

		static void on_proc(IpcServer&, const TrakMessage&, const ats::StringMap& p_sm);
		static void on_proc_ignition_on(IpcServer&, const TrakMessage&, const ats::StringMap& p_sm);
		static void on_proc_ignition_off(IpcServer&, const TrakMessage&, const ats::StringMap& p_sm);
	};

signals:
	void data_recieved(int);
	void msg_recieved(int);

public slots:

private:
	typedef std::map <const QString, TrakEvent> EventTypeMap;
	typedef std::pair <const QString, TrakEvent> EventTypePair;
	static EventTypeMap m_event_type;

	typedef std::map <const QString, TrakMessage> MessageTypeMap;
	typedef std::pair <const QString, TrakMessage> MessageTypePair;
	static MessageTypeMap m_msg_type;

	static int msg_to_id(const QString& p_msg);

	static QString m_speed;
	static QString m_rpm;
	static bool m_seatbelt_buckled;
	ServerData m_client_server;
	static IpcServer* m_ipc_server;
	static void* serv_client(void*);
	static void* shutdown_callback(ServerData*);
	void h_processMessageCmd(int argc, char* argv[]);
};
