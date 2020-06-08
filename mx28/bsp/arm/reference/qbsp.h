#ifndef QBSP_H
#define QBSP_H

#include <QObject>
#include <QTcpSocket>

#include <map>
#include <vector>

#define BSP_SERVER_PORT 43857

class QTcpServer;

class QBsp  : public QObject
{
     Q_OBJECT

public:
	QBsp();

	void wait_for_pc_connect(
		int p_port = BSP_SERVER_PORT);

	typedef std::vector <QString> ArgList;

	class BspClient;
	
	typedef int (*BspCommandFn)(BspClient &, const ArgList &, QString &);

	class BspCommand
	{
	public:
//		BspCommand();
		BspCommand( BspCommandFn);

		BspCommandFn m_command;
	};


	typedef std::map <const QString, BspCommand> CommandMap;
	typedef std::pair <const QString, BspCommand> CommandPair;

	class BspClient
	{
	public:
		BspClient(QBsp &p_bsp, QTcpSocket &p_socket);

		QString m_cmd_buf;

		static const int m_max_command_length = 256;

		QBsp *m_bsp;
		CommandMap *m_cmd;
		QTcpSocket *m_socket;

		bool m_command_too_long_state;
		bool m_quit;
	};

public slots:
	void slot_newConnection();
	void slot_readyRead();
//	void slot_connect_to_arm_error(QAbstractSocket::SocketError);
//	void slot_connected();
//	void slot_disconnected();

private:
	QTcpServer *m_bsp_socket;

	typedef std::map <QTcpSocket *, BspClient *> ClientMap;
	typedef std::pair <QTcpSocket *, BspClient *> ClientPair;

	ClientMap m_client;

	int parseCommand( const char *p_cmd, int p_len, ArgList &p_arg);

	int executeCommand(BspClient &p_c, const ArgList &p_arg, QString &p_emsg);

	CommandMap m_cmd;

};

#endif
