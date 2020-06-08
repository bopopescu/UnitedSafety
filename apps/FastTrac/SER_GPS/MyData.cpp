#include <iostream>
#include <list>
#include <vector>

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/ioctl.h>

#include "ats-common.h"
#include "atslogger.h"
#include "socket_interface.h"
#include "command_line_parser.h"
#include "MyData.h"
#include "SER_GPS.h"

extern SER_GPS * pdev;
static const ats::String g_app_name("SER_GPS");

int g_dbg = 0;

// set_input
int ac_set_input(AdminCommandContext &p_acc, const AdminCommand&, int p_argc, char *p_argv[])
{
	MyData& md = p_acc.my_data();

	ats::StringMap s;
	s.from_args(p_argc - 1, p_argv + 1);

	const ats::String& err = md.set_input(s);
  pdev->AddNewInput();
  
	if(err.empty())
	{
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "OK\n\r");
	}
	else
	{
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "Failed: %s\n\r", err.c_str());
	}

	return 0;
}


int ac_debug(AdminCommandContext &p_acc, const AdminCommand&, int p_argc, char *p_argv[])
{
	if(p_argc <= 1)
	{
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "debug=%d\n", g_dbg);
	}
	else
	{
		g_dbg = strtol(p_argv[1], 0, 0);
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "debug=%d\n", g_dbg);
	}
	return 0;
}

COMMON_EVENT_DEFINITION(,InputEvent, AppEvent)

InputEvent::InputEvent()
{
}

InputEvent::~InputEvent()
{
}

void InputEvent::start_monitor()
{
}

void InputEvent::stop_monitor()
{
}

MyData::MyData()
{
	m_mutex = new pthread_mutex_t;
	pthread_mutex_init(m_mutex, 0);

	m_work_mutex = new pthread_mutex_t;
	pthread_mutex_init(m_work_mutex, 0);

	m_command.insert(AdminCommandPair("input", AdminCommand(ac_set_input)));
	m_command.insert(AdminCommandPair("debug", AdminCommand(ac_debug)));
}

ats::String MyData::set_input(const ats::StringMap& p_args)
{
	ats::String ret;
	lock_work();
	
	unlock_work();
	return ret;
}



