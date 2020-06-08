#pragma once

#include <map>

#include <syslog.h>
#include <pthread.h>

#include "SocketReference.h"

class SocketReferenceManager
{
public:
	typedef std::map <void*, SocketReference*> SockRefMap;
	typedef std::pair <void*, SocketReference*> SockRefPair;

	SocketReferenceManager(int p_sockfd)
	{
		m_sockfd = p_sockfd;
		pthread_mutex_init(&m_mutex, 0);
	}

	~SocketReferenceManager()
	{
		shutdown();
	}

	void shutdown()
	{
		SockRefMap::iterator i = m_sock.begin();
syslog(LOG_NOTICE, "%s,%d:%s: m_sock.size()=%d", __FILE__, __LINE__, __FUNCTION__, int(m_sock.size()));

		while(i != m_sock.end())
		{
			(i->second)->on_SocketReference_shutdown();
			++i;
		}

		m_sock.clear();
	}

	void add(void* p_key, SocketReference* p_sr)
	{

		if(&(p_sr->m_srm) == this)
		{
			m_sock.insert(SockRefPair(p_key, p_sr));
		}

	}

	void lock()
	{
		pthread_mutex_lock(&m_mutex);
	}

	void unlock()
	{
		pthread_mutex_unlock(&m_mutex);
	}

	SockRefMap m_sock;
	pthread_mutex_t m_mutex;
	int m_sockfd;
};
