#pragma once

#include <syslog.h>

class SocketReferenceManager;

class SocketReference
{
public:
	SocketReference(SocketReferenceManager& p_srm) : m_srm(p_srm)
	{
	}

	virtual~ SocketReference(){}

	// Description: Called when the socket will be closed.
	virtual void on_SocketReference_shutdown()
	{
		syslog(LOG_NOTICE, "%s,%d:%s: %p", __FILE__, __LINE__, __FUNCTION__, this);
	}

	SocketReferenceManager& m_srm;
};
