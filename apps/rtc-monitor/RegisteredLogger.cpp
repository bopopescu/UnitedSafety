#include <iostream>
#include <map>
#include <list>
#include <fstream>

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/ioctl.h>

#include "ConfigDB.h"
#include "ats-common.h"
#include "ats-string.h"
#include "atslogger.h"
#include "socket_interface.h"
#include "command_line_parser.h"
#include "OnTime.h"
#include "TimerEngine.h"
#include "MyData.h"
#include "SocketReferenceManager.h"
#include "RegisteredLogger_console.h"
#include "RegisteredLogger_socket.h"
#include "RegisteredLogger_sql.h"
#include "TimerControlBlock.h"

RegisteredLogger::~RegisteredLogger()
{
	m_md.deregister_logger(*m_des, *m_key);
}

bool RegisteredLogger_socket::send_log(const ats::String& p_log)
{
	return active() ? (send_cmd(m_sockfd, MSG_NOSIGNAL, "%s\n\r", p_log.c_str()) > 0) : false;
}

RegisteredLogger_console::RegisteredLogger_console(MyData& p_md) : RegisteredLogger(p_md)
{
	m_f = fopen("/dev/ttyAM0", "r");
}

RegisteredLogger_console::~RegisteredLogger_console() 
{

	if(m_f)
	{
		fclose(m_f);
	}

}

bool RegisteredLogger_console::send_log(const ats::String& p_log)
{

	if(active() && m_f)
	{
		fprintf(m_f, "%s\n", p_log.c_str());
		return true;
	}

	return false;
}
