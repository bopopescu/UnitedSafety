/***************************************************************************
 * Author: Amour Hassan <amour@absolutetrac.com>
 * Date: June 29, 2012
 * Copyright 2008-2012 AbsoluteTrac
 *
 * Description: SocketCAN API library.
 *
 * Network code derived from:
 *      "Beej's Guide to Network Programming":
 *      http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html#socket
 ****************************************************************************/

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
#include "linux/can.h"
#include "linux/can/raw.h"

/* At time of writing, these constants are not defined in the headers */
#ifndef PF_CAN
#define PF_CAN 29
#endif
 
#ifndef AF_CAN
#define AF_CAN PF_CAN
#endif

#include "redstone-socketcan.h"

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


int CAN_connect( CANSocket *p, const char *p_interface)
{
	if(!(p && p->m_idata)) return -EINVAL;
	if(!p_interface) return -EINVAL;
	MyCANSocket &mcs = *((MyCANSocket *)(p->m_idata));

	int &skt = mcs.m_socket = socket( PF_CAN, SOCK_RAW, CAN_RAW );

	/* Locate the interface you wish to use */
	struct ifreq ifr;
	strcpy(ifr.ifr_name, p_interface);
	ioctl(skt, SIOCGIFINDEX, &ifr); /* ifr.ifr_ifindex gets filled 
                                  * with that device's index */
 
	/* Select that CAN interface, and bind the socket to it. */
	struct sockaddr_can addr;
	addr.can_family = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;
	bind( skt, (struct sockaddr*)&addr, sizeof(addr));
  
	return 0;
}

int CAN_filter( CANSocket *p, unsigned int p_id, unsigned int p_mask)
{
	MyCANSocket &mcs = *((MyCANSocket *)(p->m_idata));
	struct can_filter f;
	f.can_id = p_id;
	f.can_mask = p_mask;
	setsockopt(mcs.m_socket, SOL_CAN_RAW, CAN_RAW_FILTER, &f, sizeof(f));
}

CAN_WriteResult CAN_write( CANSocket *p, int p_can_id, const void *p_data, size_t p_len)
{
	CAN_WriteResult r;
	r.m_err = -EINVAL;
	r.m_nwrite = 0;

	if(!(p && p->m_idata)) return r;
	if((!p_data) && p_len) return r;
	MyCANSocket &mcs = *((MyCANSocket *)(p->m_idata));

	r.m_err = 0;
	struct can_frame frame;
	frame.can_id = p_can_id;
	while(p_len) {
		/* Send a message to the CAN bus */
		const size_t len = (p_len < sizeof(frame.data)) ? p_len : sizeof(frame.data);
		memcpy(frame.data, p_data, len);
		frame.can_dlc = len;
		ssize_t n = write( mcs.m_socket, &frame, sizeof(frame));
		if(!n)
		{
			r.m_err = -EIO;
			break;
		}
		if(n < 0)
		{
			r.m_err = -errno;
			break;
		}
		r.m_nwrite += len;
		p_data = ((char *)p_data) + len;
		p_len -= len;
	}
	return r;
}

EXTERN_C int CAN_read( CANSocket *p, struct can_frame *p_msg)
{
	if(!(p && p->m_idata)) return -EINVAL;
	MyCANSocket &mcs = *((MyCANSocket *)(p->m_idata));

	struct can_frame f;
	if(!p_msg) p_msg = &f;
	return read(mcs.m_socket, p_msg, sizeof(*p_msg));
}


void destroy_CANSocket(CANSocket *p)
{
	if(p) {
		if(p->m_idata) delete (MyCANSocket *)(p->m_idata);
		delete p;
	}
}
