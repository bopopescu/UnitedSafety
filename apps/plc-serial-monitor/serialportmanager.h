#pragma once
#include <QString>

#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <fcntl.h>
#include <stdio.h>

#define SOH 0x01

typedef enum {
	DAILY_REPORT_MSG                = 0x1,
	CENTRIFUGE_MAINTENANCE_REQ_MSG  = 0x2,
	CABINET_MAINTENANCE_REQ_MSG     = 0x3,
	CENTRIFUGE_MAINTENANCE_PERF_MSG = 0x4,
	CABINET_MAINTENANCE_PERF_MSG    = 0x5,
	CRITICAL_FAULT_DETECTED_MSG     = 0x6,
	ACKNOWLEDGEMENT_MSG             = 0x7,
	DATA_FORM_MSG                   = 0x8,
	PASSWORD_ACCESS_MSG             = 0xD
} SERIAL_MESSAGE_TYPE;


class SerialPortManager
{
public:
	SerialPortManager();
	int start();
	int readMessage(QByteArray &data);
	void sendAck();

private:

	void loadConfig();
	int  baudrate(int baud);
	bool checkMessage(QByteArray data);
	int m_baud_rate;
	int m_msg_delay_ms;
	QString m_serial_port;
	struct termios m_sertio;
	int m_fd;

};
