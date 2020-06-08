#ifndef SOCKETCAN_H
#define SOCKETCAN_H

#include "../gcc/gcc.h"
#include "linux/can/j1939.h"

typedef struct 
{
		struct sockaddr_can addr;
		int promisc;
		int time;
		int pkt_len;
} sock_can;

struct CANSocket
{
	void *m_idata;
};

struct CAN_WriteResult
{
	size_t m_nwrite;
	int m_err;
};

// Description: Creates and returns a new CANSocket structure.
//
// Return: A new CANSocket structure is returned on success, and NULL is returned
//	on error. A non-NULL pointer can only be deleted with the "destroy_CANSocket"
//	function.
EXTERN_C CANSocket *create_new_CANSocket();

// Description: Destroys a non-NULL CANSocket structure. Nothing is done if NULL is passed.
EXTERN_C void destroy_CANSocket(CANSocket *);

// Description: Connects the given CAN socket to the specified CAN interface.
//
// Parameters:
//	p_interface	- The name of the CAN interface (example "can0").
//
// Return: Zero is returned on success, and an error value is returned otherwise.
EXTERN_C int CAN_connect(CANSocket *, const char *p_interface, int sa = 0xF9);

// Description: Sends a message to the CAN interface.
//
// Parameters:
//	p_can_id	- The CAN ID for the message
//	p_data		- The data to be sent
//	p_len		- The length of the data to send. Note that if the data to send
//			  is larger than the maximum allowed per CAN message, then the
//			  data will be sent using multiple CAN messages until all the
//			  data has been sent, or until an error occured.
//
// Return: A CAN_WriteResult is returned, which contains the number of bytes written
//	and the error result (if any).
//
EXTERN_C CAN_WriteResult CAN_write( CANSocket *p, int p_can_id, const void *p_data, size_t p_len, size_t dstaddr = 0xFF);

// Description: Reads a CAN message and stores it in "p_msg".
//
// Parameter:
//	p_msg	- Where the CAN message read will be stored. If "p_msg" is NULL, then the CAN
//		  message will be thrown away (message will be read from the interface, but it
//		  will not be returned to the caller).
//
// Return: A negative number is returned on error, otherwise the number of bytes read
//	(along with the CAN message in "p_msg") is returned. A return value of zero would
//	mean that there are no more bytes/messages to be read (communication channel has
//	been closed).
EXTERN_C int CAN_read( CANSocket *p, unsigned char *p_msg, size_t len, int timeout = 0);


EXTERN_C int CAN_filter( CANSocket *p, unsigned int p_id, unsigned int p_mask);

#endif
