#include <QTimer>
#include <QElapsedTimer>

#include <errno.h>

#include "socket_interface.h"
#include "ConfigDB.h"
#include "atslogger.h"

#include "serialportmanager.h"

extern ATSLogger g_log;


SerialPortManager::SerialPortManager()
{
	loadConfig();
}

int SerialPortManager::baudrate(int baud)
{
	switch(baud)
	{
	case 110:
		return B110;
	case 150:
		return B150;
	case 300:
		return B300;
	case 1200:
		return B1200;
	case 2400:
		return B2400;
	case 4800:
		return B4800;
	case 9600:
		return B9600;
	case 19200:
	default:
		return B19200;
	case 38400:
		return B38400;
	case 57600:
		return B57600;
	case 115200:
		return B115200;
	case 230400:
		return B230400;
	case 460800:
		return B460800;
	case 921600:
		return B921600;
	}
}

void SerialPortManager::loadConfig()
{
	db_monitor::ConfigDB db;
	const ats::String app_name("plc-serial-monitor");
	m_serial_port = db.GetValue(app_name, "m_serial_port", "/dev/ttySP0").c_str();
	m_baud_rate = db.GetInt(app_name, "m_baud_rate", 19200);
	m_msg_delay_ms = db.GetInt(app_name, "m_msg_delay_ms", 500);
}

// Code based on example at http://www.tldp.org/HOWTO/Serial-Programming-HOWTO/x115.html#AEN129
int SerialPortManager::start()
{
	m_fd = open(m_serial_port.toUtf8().constData(), O_RDWR | O_NOCTTY);
	if (m_fd < 0)
	{
		perror(m_serial_port.toUtf8().constData());
		ats_logf(&g_log, "%s, %d: Cannot open %s. Error m_fd=%d", __FILE__, __LINE__,  m_serial_port.toUtf8().constData(), m_fd);
		exit(-1);
	}

	bzero(&m_sertio, sizeof(m_sertio));
	m_sertio.c_cflag = baudrate(m_baud_rate) | CS8 | CLOCAL | CREAD;
	m_sertio.c_iflag = IGNPAR;
	m_sertio.c_oflag = 0;

	m_sertio.c_lflag = 0;

	m_sertio.c_cc[VTIME] = 0;
	m_sertio.c_cc[VMIN] = 1; //Blocking read until 1 character is read

	tcflush(m_fd, TCIFLUSH);
	tcsetattr(m_fd, TCSANOW, &m_sertio);

	return 0;
}

int SerialPortManager::readMessage(QByteArray &data)
{
	int error = 0;
	bool soh = false;
	int bytes_read = 0;
	int message_length = 0;
	QElapsedTimer timer;

	ats_logf(&g_log, "%s, %d:readMessage() start. Timeout=%d ms ", __FILE__, __LINE__, m_msg_delay_ms);
	while(!error)
	{
		char buf;
		timer.start();
		int res = read(m_fd,&buf, 1); //Read one byte
		ats_logf(&g_log, "%s, %d,:read() read one byte :%02X. bytes_read = %d, length=%d", __FILE__, __LINE__, buf, bytes_read,message_length);
		if (res <= 0)
		{
			ats_logf(&g_log, "%s, %d:Read error. ", __FILE__, __LINE__);
			return -3;
		}

		if((buf == SOH) && (soh == false))
		{
			soh = true;
			data.append(buf);
			bytes_read++;
		}
		else if(soh)
		{
			if(timer.hasExpired(m_msg_delay_ms))
			{
				ats_logf(&g_log, "%s, %d:Message timeout.", __FILE__, __LINE__);
				data.clear();
				if(buf == SOH)
				{
					bytes_read = 0;
				}
				else
				{
					return -2;
				}
			}
			data.append(buf);
			bytes_read++;
			if(bytes_read == 3)
			{
				message_length = buf;
			}
			else if(bytes_read > 3)
			{
				message_length --;
				if( message_length <= 0)
				{
					if(checkMessage(data))
					{
						return 0;
					}
					else
					{
						ats_logf(&g_log, "%s, %d:Checksum error.", __FILE__, __LINE__);
						return -1;
					}
				}
			}
		}

	}
	return error;
}

bool SerialPortManager::checkMessage(QByteArray data)
{
	int i =0;
	int length = data.length() - 1;
	int checksum = 0;

	for(i=1;i<length;i++)
	{
		checksum ^=data[i];
	}

	if((int)(data[length]) == checksum)
	{
		return true;
	}

	return false;

}

void SerialPortManager::sendAck()
{
	QByteArray data;
	data[0] = 0x1; //SOH
	data[1] = 0x7; //Message type ACK
	data[2] = 0x1; //length
	data[3] = 0x6; //Checksum

	write(m_fd, data.constData(), data.length());

}
