#include <sys/time.h>

#include "TimerControlBlock.h"
#include "TimerEngine.h"
#include "socket_interface.h"
#include "MyData.h"

TimerControlBlock::TimerControlBlock() : m_te(0), m_key(0)
{
	m_key = 0;
}

TimerControlBlock::~TimerControlBlock()
{
}

bool TimerControlBlock::on_timeout(int, int)
{
	return false;
}

TimerControlBlock_socket_alarm::TimerControlBlock_socket_alarm(int p_sockfd) : m_sockfd(p_sockfd)
{
}

TimerControlBlock_socket_alarm::~TimerControlBlock_socket_alarm()
{
}

bool TimerControlBlock_socket_alarm::on_timeout(int p_sec, int p_msec)
{

	if(send_cmd(m_sockfd, MSG_NOSIGNAL, "<alarm: %d.%03d>\n\r", p_sec, p_msec) > 0)
	{
		return true;
	}

	return false;
}

TimerControlBlock_log_ontime::TimerControlBlock_log_ontime(
	MyData& p_md,
	const ats::String& p_key,
	const ats::String& p_des)
	:
	m_md(p_md),
	m_key(p_key),
	m_des(p_des)
{
}

TimerControlBlock_log_ontime::~TimerControlBlock_log_ontime()
{
}

bool TimerControlBlock_log_ontime::on_timeout(int p_sec, int p_msec)
{
	const ats::String& emsg = m_md.request_log(
		m_key,
		m_des,
		p_sec,
		p_msec);

	return emsg.empty();
}
