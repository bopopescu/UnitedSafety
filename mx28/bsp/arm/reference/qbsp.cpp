#include <QTcpServer>

#include <stdio.h>
#include <syslog.h>

#include "qbsp.h"

static int cmd_quit(QBsp::BspClient &p_client, const QBsp::ArgList &p_arg, QString &p_emsg)
{
	p_client.m_quit = true;
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

QBsp::QBsp()
{
	m_cmd.insert(CommandPair("quit", BspCommand(cmd_quit)));
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
