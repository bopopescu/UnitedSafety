#pragma once

#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>

#include "ats-common.h"
#include "socket_interface.h"

namespace ats
{

class SocketInterfaceResponse
{
public:

	SocketInterfaceResponse(int p_sock_fd)
	{
		init(p_sock_fd);
	}

	SocketInterfaceResponse(int p_sock_fd, time_t p_timeout_sec, time_t p_timeout_usec)
	{
		init(p_sock_fd);
		set_timeout(p_timeout_sec, p_timeout_usec);
	}

	void set_timeout(time_t p_timeout_sec, time_t p_timeout_usec)
	{
		m_cache.m_timeout_sec = p_timeout_sec;
		m_cache.m_timeout_usec = p_timeout_usec;
	}

	// Description: Returns a standard RedStone response line.
	//
	// Return: The response received on success (and "error()" will return zero), otherwise the empty string is returned
	//	and "error()" will return non-zero.
	String get()
	{
		return get(0, "\r", 0);
	}

	// Description: Returns a response line. A response line is terminated by the
	//	characters in "p_eol_char".
	//
	// Return: The response received on success (and "error()" will return zero), otherwise the empty string is returned
	//	and "error()" will return non-zero.
	String get(int p_max_len, const String& p_eol_char, int p_flags)
	{
		char* response;
		const int nread = get_standard_response(m_sock_fd, &response, p_max_len, p_eol_char.c_str(), p_flags, &m_cache);

		if(nread < 0)
		{
			m_error = nread;
			return String();
		}
	
		m_error = 0;

		const String s(response);
		free(response);
		return s;
	}

	// Description: Returns the last error.
	int error() const
	{
		return m_error;
	}

	// Descripton: Sets the current error value. 0 means no error, and all other values indicates
	//	some kind of error.
	void set_error(int p_error)
	{
		m_error = p_error;
	}

private:
	void init(int p_sock_fd)
	{
		init_ClientDataCache(&m_cache);
		m_sock_fd = p_sock_fd;
		m_error = 0;
	}

	ClientDataCache m_cache;
	int m_sock_fd;
	int m_error;
};

}
