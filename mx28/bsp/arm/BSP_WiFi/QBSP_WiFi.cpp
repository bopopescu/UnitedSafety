#include <QTcpServer>
#include <QRegExp>

#include <stdio.h>
#include <syslog.h>

#include "QBSP_WiFi.h"

using namespace nsWIFI;

static int cmd_quit(QBsp::BspClient &p_client, const QBsp::ArgList &p_arg, QString &p_emsg)
{
	p_client.m_quit = true;
	return 0;
}

static int cmd_enable(QBsp::BspClient &p_client, const QBsp::ArgList &p_arg, QString &p_emsg)
{
	if(p_arg.size() < 2) {
		p_client.m_socket->write(QString("enable=<undefined>\n").toLatin1());
		return 0;
	}
	{
		QRegExp r("^y|yes|1|true|on$");
		r.setCaseSensitivity(Qt::CaseInsensitive);
		if(0 == r.indexIn(p_arg[1])) {
			system("echo \"H\" > /dev/set-gpio");
			p_client.m_socket->write(QString("WiFi is now enabled\n").toLatin1());
			return 0;
		}
	}
	{
		QRegExp r("^n|no|0+|false|off$");
		r.setCaseSensitivity(Qt::CaseInsensitive);
		if(0 == r.indexIn(p_arg[1])) {
			system("echo \"h\" > /dev/set-gpio");
			p_client.m_socket->write(QString("WiFi is now disabled\n").toLatin1());
			return 0;
		}
	}
	p_emsg = "expected <boolean>\n";
	return -1;
}

static int cmd_card_detect(QBsp::BspClient &p_client, const QBsp::ArgList &p_arg, QString &p_emsg)
{
	if(p_arg.size() < 2) {
		p_client.m_socket->write(QString("card_detect=<undefined>\n").toLatin1());
		return 0;
	}
	{
		QRegExp r("^y|yes|1|true|on$");
		r.setCaseSensitivity(Qt::CaseInsensitive);
		if(0 == r.indexIn(p_arg[1])) {
			system("echo \"I\" > /dev/set-gpio");
			p_client.m_socket->write(QString("WiFi card-detect pin is now 1 (high)\n").toLatin1());
			return 0;
		}
	}
	{
		QRegExp r("^n|no|0+|false|off$");
		r.setCaseSensitivity(Qt::CaseInsensitive);
		if(0 == r.indexIn(p_arg[1])) {
			system("echo \"i\" > /dev/set-gpio");
			p_client.m_socket->write(QString("WiFi card-detect pin is now 0 (low)\n").toLatin1());
			return 0;
		}
	}
	p_emsg = "expected <boolean>\n";
	return -1;
}

static int cmd_power(QBsp::BspClient &p_client, const QBsp::ArgList &p_arg, QString &p_emsg)
{
	if(p_arg.size() < 2) {
		FILE *f = popen("iwconfig wlan0|grep Tx-Power|sed -r s/\\ +//", "r");
		if(f) {
			QString s;
			for(;;) {
				char c;
				const size_t nread = fread(&c, 1, 1, f);
				if(!nread) break;
				s += c;
			}
			pclose(f);
			p_client.m_socket->write(s.toLatin1());
		} else {
			p_client.m_socket->write(QString("Failed to query power\n").toLatin1());
			return -1;
		}
		return 0;
	}
	QString s;
	s.sprintf("iwconfig wlan0 power %s", p_arg[1].toLatin1().constData());
	system(s.toLatin1().constData());
	return 0;
}

static int cmd_mode(QBsp::BspClient &p_client, const QBsp::ArgList &p_arg, QString &p_emsg)
{
	if(p_arg.size() < 2) {
		FILE *f = fopen("/tmp/flags/wifi-mode-infra", "r");
		if(f) {
			fclose(f);
			p_client.m_socket->write(QString("mode=infrastructure\n").toLatin1());
			return 0;
		}
		p_client.m_socket->write(QString("mode=ad-hoc\n").toLatin1());
		return 0;
	}
	{
		QRegExp r("^ad-?hoc$");
		r.setCaseSensitivity(Qt::CaseInsensitive);
		if(0 == r.indexIn(p_arg[1])) {
			FILE *f = fopen("/tmp/flags/wifi-mode-infra", "r");
			if(!f) {
				p_emsg = "Already running in Ad-Hoc (access-point) mode\n";
				return 1;
			}
			fclose(f);
			p_client.m_socket->write(QString("Switching to Ad-Hoc (access-point) mode\n").toLatin1());
			system("killall -9 /sbin/udhcpc");
			system("echo 1 > /proc/sys/net/ipv4/ip_forward");
			system(
				"export LD_LIBRARY_PATH=/usr/local/lib"
				";export XTABLES_LIBDIR=/usr/local/libexec/xtables"
				";iptables -t nat -A POSTROUTING -o ppp0 -j MASQUERADE"
				";iptables -A FORWARD -i ppp0 -o wlan0 -m state --state RELATED,ESTABLISHED -j ACCEPT"
				";iptables -A FORWARD -i wlan0 -o ppp0 -j ACCEPT"
				);
			return 0;
		}
	}
	{
		QRegExp r("^infra(structure)?|!ad-?hoc$");
		r.setCaseSensitivity(Qt::CaseInsensitive);
		if(0 == r.indexIn(p_arg[1])) {
			FILE *f = fopen("/tmp/flags/wifi-mode-infra", "r");
			if(f) {
				fclose(f);
				p_emsg = "Already running in infrastructure mode\n";
				return 1;
			}
			p_client.m_socket->write(QString("Switching to infrastructure mode\n").toLatin1());
			system("touch /tmp/flags/wifi-mode-infra");
			system(
				"export LD_LIBRARY_PATH=/usr/local/lib"
				";export XTABLES_LIBDIR=/usr/local/libexec/xtables"
				";iptables -t nat -F"
				";iptables -F"
				);
			if(!fork()) {
				setsid();
				chdir("/");
				freopen( "/dev/null", "r", stdin);
				freopen( "/dev/null", "w", stdout);
				freopen( "/dev/null", "w", stderr);
				execl("/sbin/udhcpc", "/sbin/udhcpc", "-i", "wlan0", NULL);
				exit(1);
			}
			return 0;
		}
	}

	return 0;
}

static int cmd_iwconfig(QBsp::BspClient &p_client, const QBsp::ArgList &p_arg, QString &p_emsg)
{
	QString cmd;
	cmd += "iwconfig wlan0";
	{
		int i;
		for(i = 1; i < int(p_arg.size()); ++i) {
			cmd += " " + p_arg[i];
		}
	}

	FILE *f = popen(cmd.toLatin1().constData(), "r");
	if(f) {
		QString s;
		for(;;) {
			char c;
			const size_t nread = fread(&c, 1, 1, f);
			if(!nread) break;
			s += c;
		}
		pclose(f);
		p_client.m_socket->write(s.toLatin1());
	} else {
		p_client.m_socket->write((QString("Failed to run \"") + cmd + "\"\n").toLatin1());
		return -1;
	}
	return 0;
}

static int cmd_start(QBsp::BspClient &p_client, const QBsp::ArgList &p_arg, QString &p_emsg)
{
	p_client.m_socket->write(QString("Preparing to start WiFi...\n").toLatin1());
	p_client.executeCommand("card_detected 0", p_emsg);
	p_client.executeCommand("enable 0", p_emsg);
	sleep(2);
	p_client.m_socket->write(QString("Starting WiFi...\n").toLatin1());
	p_client.executeCommand("enable 1", p_emsg);
	p_client.executeCommand("card_detected 1", p_emsg);
	sleep(3);
	p_client.executeCommand("iwconfig mode ad-hoc essid m3g1104", p_emsg);
	system("ifconfig wlan0 hw ether 08:00:28:00:61:39");
	system("ifconfig wlan0 192.168.1.1");
	p_client.m_socket->write(QString("WiFi started\n").toLatin1());

	return 0;
}

static int cmd_stop(QBsp::BspClient &p_client, const QBsp::ArgList &p_arg, QString &p_emsg)
{
	p_client.m_socket->write(QString("Stopping WiFi...\n").toLatin1());
	system("ifconfig wlan0 down");
	p_client.executeCommand("enable 0", p_emsg);
	p_client.executeCommand("card_detected 0", p_emsg);
	p_client.m_socket->write(QString("WiFi stopped\n").toLatin1());

	return 0;
}


#if 0
QBsp::BspCommand::BspCommand()
{
	m_command = 0;
}
#endif

QBsp::BspCommand::BspCommand(BspCommandFn p_fn)
{
	m_command = p_fn;
}

QBsp::BspClient::BspClient(QBsp &p_bsp, QTcpSocket &p_socket)
{
	m_bsp = &p_bsp;
	m_socket = &p_socket;

	m_cmd = &(m_bsp->m_cmd);
	m_quit = false;
	m_command_too_long_state = false;
}

int QBsp::BspClient::executeCommand( const QString &p_cmd, QString &p_emsg)
{
	return m_bsp->executeCommand(*this, p_cmd, p_emsg);
}

QBsp::QBsp()
{
	m_cmd.insert(CommandPair("quit", BspCommand(cmd_quit)));
	m_cmd.insert(CommandPair("enable", BspCommand(cmd_enable)));
	m_cmd.insert(CommandPair("card-detect", BspCommand(cmd_card_detect)));
	m_cmd.insert(CommandPair("power", BspCommand(cmd_power)));
	m_cmd.insert(CommandPair("mode", BspCommand(cmd_mode)));
	m_cmd.insert(CommandPair("iwconfig", BspCommand(cmd_iwconfig)));
	m_cmd.insert(CommandPair("start", BspCommand(cmd_start)));
	m_cmd.insert(CommandPair("stop", BspCommand(cmd_stop)));
}

void QBsp::wait_for_pc_connect(
	int p_port)
{
	m_bsp_socket = new QTcpServer();
	QTcpServer &s = *m_bsp_socket;

	connect(
		&s,
		SIGNAL(newConnection()),
		this,
		SLOT(slot_newConnection()));

#if 0
	connect(
		&s,
		SIGNAL(error(QAbstractSocket::SocketError)),
		this,
		SLOT(slot_connect_to_arm_error(QAbstractSocket::SocketError)));
#endif
	if(!s.listen( QHostAddress::Any, p_port)) {
		syslog(LOG_ERR, "%s,%d:%s: Failed to listen on port %d", __FILE__, __LINE__, __FUNCTION__, p_port);
	}
}

void QBsp::slot_newConnection()
{
	syslog(LOG_ERR, "%s,%d:%s:\n", __FILE__, __LINE__, __FUNCTION__);

	QTcpServer &s = *m_bsp_socket;
	QTcpSocket *c = s.nextPendingConnection();
	m_client.insert(ClientPair(c, new BspClient(*this, *c)));

	connect(
		c,
		SIGNAL(readyRead()),
		this,
		SLOT(slot_readyRead()));

	
}

void QBsp::slot_readyRead()
{
	QObject *o = sender();
	QTcpSocket *c = qobject_cast<QTcpSocket *>(o);
	syslog(LOG_ERR, "%s,%d:%s: o=%p, c=%p\n", __FILE__, __LINE__, __FUNCTION__, o, c);

	BspClient *client = 0;
	{
		ClientMap::iterator i = m_client.find(c);
		if(i != m_client.end()) client = i->second;
	}
	if(!client) return;

	const int maxlen = 128;
	char buf[maxlen];
	int nread = c->read(buf, maxlen);
	{
		int i;
		for(i = 0; i < nread; ++i) {
			const char chr = buf[i];
			if(('\n' == chr) || ('\r' == chr)) {
				if(!(client->m_command_too_long_state || client->m_cmd_buf.isEmpty())) {
					syslog(LOG_ERR, "%s,%d:%s: Parse \"%s\"", __FILE__, __LINE__, __FUNCTION__,
						client->m_cmd_buf.toLatin1().constData());
					ArgList arg;
					parseCommand(client->m_cmd_buf.toLatin1().constData(), client->m_cmd_buf.size(), arg);
					{
						int i;
						for(i = 0; i < int(arg.size()); ++i) {
							syslog(LOG_ERR, "      arg[%d]: \"%s\"", i, arg[i].toLatin1().constData());
						}
					}
					QString emsg;
					int ret = executeCommand(*client, arg, emsg);
					if(client->m_quit) {
						c->disconnectFromHost();
					}
					if(ret) {
						c->write(emsg.toLatin1());
					}
				}
				client->m_cmd_buf = "";
				client->m_command_too_long_state = false;

			} else if(!client->m_command_too_long_state) {
				if(client->m_cmd_buf.size() > BspClient::m_max_command_length) {
					syslog(LOG_ERR, "%s,%d:%s: Command is too long", __FILE__, __LINE__, __FUNCTION__);
					client->m_command_too_long_state = true;
				} else {
					client->m_cmd_buf += chr;
				}
			}
		}
	}
}

int QBsp::executeCommand(BspClient &p_c, const ArgList &p_arg, QString &p_emsg)
{
	if(p_arg.empty()) return 0;
	if(!p_c.m_cmd) {
		p_emsg = "Invalid command \"" + p_arg[0] + "\" (no command map set)\n";
		return 1;
	}
	CommandMap::const_iterator i = p_c.m_cmd->find(p_arg[0]);
	int ret;
	if(i != p_c.m_cmd->end()) {
		ret = (i->second).m_command(p_c, p_arg, p_emsg);
	} else {
		p_emsg = "Invalid command \"" + p_arg[0] + "\"\n";
		return -1;
	}
	return ret;
}

int QBsp::executeCommand(BspClient &p_c, const QString &p_cmd, QString &p_emsg)
{
	ArgList arg;
	const int ret = parseCommand(p_cmd.toLatin1().constData(), p_cmd.length(), arg);
	if(ret) {
		p_emsg = "Failed to parse command \"" + p_cmd + "\"\n";
		return ret;
	}
	return executeCommand(p_c, arg, p_emsg);
}

static bool h_is_hex_digit(char c)
{
	return ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
}

int QBsp::parseCommand( const char *p_cmd, int p_len, ArgList &p_arg)
{
	p_arg.clear();
	QString token;
	bool in_quote = false;
	bool in_token = false;
	for(;;) {
		if(p_len <= 0) break;
		char c = *(p_cmd++);
		--p_len;

		if('\\' == c) {
			if(p_len <= 0) {
				syslog(LOG_ERR, "expected char after '\\'");
				return -1;
			}
			c = *(p_cmd++);
			--p_len;
			switch(c) {
			case 'n': token += '\n'; break;
			case 'r': token += '\r'; break;
			case 'b': token += '\b'; break;
			case 't': token += '\t'; break;
			case '0': token += '\0'; break;
			case 'x':
				if(p_len < 2) {
					syslog(LOG_ERR, "expected 2 hex-digits");
					return -1;
				}
				{
					char h[3] = {
						p_cmd[0],
						p_cmd[1],
						'\0'
						};
					p_cmd += 2;
					p_len -= 2;
					if(!(h_is_hex_digit(h[0]) && h_is_hex_digit(h[1]))) {
						syslog(LOG_ERR, "expected 2 hex digits");
						return -1;
					}
					token += char(strtol(h, 0, 16));
				}
				break;
			default: token += c; break;
			}
			in_token = true;

		} else if('"' == c) {
			in_quote = !in_quote;
			if(in_quote) in_token = true;

		} else {
			switch(c) {
			case '\r':
			case '\n':
			case '\t':
			case ' ':
				if(in_quote) {
					token += c;
				} else {
					if(in_token) {
						p_arg.push_back(token);
						token = "";
						in_token = false;
					}
				}
				break;

			default:
				in_token = true;
				token += c;
				break;
			}
		}
	}

	if(in_token) {
		p_arg.push_back(token);
		token = "";
	}
	return 0;
}

#if 0
void QBsp::slot_connect_to_arm_error(QAbstractSocket::SocketError)
{
	syslog(LOG_ERR, "%s,%d:%s:\n", __FILE__, __LINE__, __FUNCTION__);
	fprintf(stderr, "%s,%d:%s:\n", __FILE__, __LINE__, __FUNCTION__);
}

void QBsp::slot_connected()
{
	syslog(LOG_ERR, "%s,%d:%s:\n", __FILE__, __LINE__, __FUNCTION__);
	fprintf(stderr, "%s,%d:%s:\n", __FILE__, __LINE__, __FUNCTION__);
}

void QBsp::slot_disconnected()
{
	syslog(LOG_ERR, "%s,%d:%s:\n", __FILE__, __LINE__, __FUNCTION__);
	fprintf(stderr, "%s,%d:%s:\n", __FILE__, __LINE__, __FUNCTION__);
}
#endif
