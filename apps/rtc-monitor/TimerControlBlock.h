#pragma once

#include "ats-common.h"
#include "SocketReference.h"

class MyData;
class TimerEngine;

class TimerControlBlock
{
public:
	TimerControlBlock();

	virtual~ TimerControlBlock();

	virtual bool on_timeout(int p_sec, int p_msec);

	TimerEngine* m_te;
	const ats::String* m_key;
};

class TimerControlBlock_socket_alarm : public TimerControlBlock
{
public:
	TimerControlBlock_socket_alarm(int p_sockfd);

	virtual~ TimerControlBlock_socket_alarm();

	virtual bool on_timeout(int p_sec, int p_msec);

	int m_sockfd;
};

class TimerControlBlock_log_ontime : public TimerControlBlock
{
public:
	TimerControlBlock_log_ontime(MyData& p_md, const ats::String& p_key, const ats::String& p_des);

	virtual~ TimerControlBlock_log_ontime();

	virtual bool on_timeout(int p_sec, int p_msec);

	MyData& m_md;
	const ats::String m_key;
	const ats::String m_des;
};


#if 0
class TimerControlBlock : public SocketReference
{
public:
	TimerControlBlock(TimerEngine& p_te, SocketReferenceManager& p_srm);

	virtual~ TimerControlBlock();

	virtual void on_SocketReference_shutdown();

	virtual bool on_timeout();

	const ats::String* m_key;
	int m_sockfd;

	TimerEngine& m_te;
};
#endif
