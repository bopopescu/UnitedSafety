#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include "linux/socket.h"

/* At time of writing, these constants are not defined in the headers */
#ifndef PF_CAN
#define PF_CAN 29
#endif
 
#ifndef AF_CAN
#define AF_CAN PF_CAN
#endif

#include "redstone-socketcan-j1939.h"

class MyCANSocket
{
public:

	MyCANSocket();

	int m_socket;
};

MyCANSocket::MyCANSocket()
{
	m_socket = -1;
}

CANSocket *create_new_CANSocket()
{
	CANSocket *cs = new CANSocket;
	cs->m_idata = new MyCANSocket();
	return cs;
}

int CAN_connect( CANSocket *p, const char *p_interface, int sa)
{
	if(!(p && p->m_idata)) return -EINVAL;
	if(!p_interface) return -EINVAL;
	MyCANSocket &mcs = *((MyCANSocket *)(p->m_idata));

	int &skt = mcs.m_socket = socket( PF_CAN, SOCK_DGRAM, CAN_J1939);

	/* Locate the interface you wish to use */
	struct ifreq ifr;
	strcpy(ifr.ifr_name, p_interface);
	ioctl(skt, SIOCGIFINDEX, &ifr); /* ifr.ifr_ifindex gets filled 
                                  * with that device's index */

	/* Select that CAN interface, and bind the socket to it. */
	struct sockaddr_can addr;
	addr.can_family = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;
	addr.can_addr.j1939.name = J1939_NO_NAME;
	//default source address for off board diagnistic-service tool #1, from page 81 of SAE J1939 Devised Jun 2012
	addr.can_addr.j1939.addr = sa & 0xFF;
	addr.can_addr.j1939.pgn = J1939_NO_PGN;
	bind(skt, (struct sockaddr*)&(addr), sizeof(addr));

	//ATS FIXME: do we need promisc mode ? should be very noisy.
	//int promisc = 1; /* 0 = disabled (default), 1 = enabled */
	//setsockopt(skt, SOL_CAN_J1939, SO_J1939_PROMISC, &promisc, sizeof(promisc));

	return 0;
}

int CAN_filter( CANSocket *p, unsigned int p_id, unsigned int p_mask)
{
	MyCANSocket &mcs = *((MyCANSocket *)(p->m_idata));
	struct j1939_filter filt;
	memset(&filt, 0, sizeof(filt));
	filt.pgn = p_id;
	filt.pgn_mask = p_mask;
	setsockopt(mcs.m_socket, SOL_CAN_J1939, SO_J1939_FILTER, &filt, sizeof(filt));
}

EXTERN_C CAN_WriteResult CAN_write( CANSocket *p, int pgn, const void *p_data, size_t p_len, size_t dstaddr)
{
	struct sockaddr_can addr;
	CAN_WriteResult r;
	r.m_err = -EINVAL;
	r.m_nwrite = 0;

	if(!(p && p->m_idata)) return r;
	if((!p_data) && p_len) return r;

	MyCANSocket &mcs = *((MyCANSocket *)(p->m_idata));
	r.m_err = 0;

	memset(&addr, 0, sizeof(addr));
	addr.can_family = AF_CAN;
	addr.can_addr.j1939.addr = dstaddr;
	addr.can_addr.j1939.pgn = pgn;

	ssize_t n = sendto(mcs.m_socket, p_data, p_len, 0, (const sockaddr *)&addr, sizeof(addr));
	if(!n)
	{
		r.m_err = -EIO;
		return r;
	}
	if(n < 0)
	{
		r.m_err = -errno;
		return r;
	}

	r.m_nwrite += p_len;
	return r;
}

EXTERN_C int CAN_read( CANSocket *p, unsigned char *p_msg, size_t len, int timeout)
{
	if(!(p && p->m_idata)) return -EINVAL;
	if(!(p_msg)) return -EINVAL;

	// buffer contain at least 4 bytes for pgn and 8 bytes for data.
	if( len < (4 + 8)) return -EINVAL;

	MyCANSocket &mcs = *((MyCANSocket *)(p->m_idata));

	unsigned char * pp = p_msg + 4 * sizeof(unsigned char);
	memset(p_msg, 0, 4);

	struct sockaddr_can saddr;
	socklen_t slen = sizeof(saddr);

	struct timeval tv;
	if(timeout > 0)
	{
		tv.tv_sec = timeout;
		tv.tv_usec = 0;
		setsockopt(mcs.m_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));
	}

	int ret = recvfrom(mcs.m_socket, pp, len - 4, 0, ( sockaddr *)&saddr, &slen);
	if (ret < 0) return -errno;

	p_msg[0] = (saddr.can_addr.j1939.pgn);
	p_msg[1] = (saddr.can_addr.j1939.pgn) >> 8;
	p_msg[2] = (saddr.can_addr.j1939.pgn) >> 16;
	p_msg[3] = 0x00;
	if(saddr.can_addr.j1939.addr <= 0xfe)
     p_msg[3] = saddr.can_addr.j1939.addr;
	ret += 4;

#ifdef datadump 
	for(int i = 0; i < ret; ++i)
		printf("%p ", buf[i]);

	printf("\n");
#endif

	return ret;
}

void destroy_CANSocket(CANSocket *p)
{
	if(p) {
		if(p->m_idata) delete (MyCANSocket *)(p->m_idata);
		delete p;
	}
}
