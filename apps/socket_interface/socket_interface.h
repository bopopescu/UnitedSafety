#pragma once
#include <net/if.h>

#include "socket_interface_core.h"

struct ClientSocket
{
	// AWARE360 FIXME: Use standard IP size, not hardcoded value "32".
	char m_resolved_ip[32];
	int m_domain;
	int m_fd;
	int m_keepalive_count;
	int m_keepalive_idle;
	int m_keepalive_interval;
	struct ifreq m_ifr;
	char m_emsg[256];
};
