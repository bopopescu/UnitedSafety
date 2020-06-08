#include <iostream>

#include <stdio.h>
#include <sys/socket.h>
#include <syslog.h>

#include "redstone-socketcan.h"
#include "linux/can.h"
#include "can-seatbelt.h"

int main()
{
	CANSocket *s = create_new_CANSocket();
	if(!s) {
		fprintf(stderr, "%s,%d: Failed to create CANSocket\n", __FILE__, __LINE__);
	}

	CAN_connect(s, "can0");
//	fprintf(stderr, "%s,%d: CAN is connected.\n", __FILE__, __LINE__);
	while(1)
	{
		struct can_frame msg;
		int bytes_read = CAN_read(s, &msg);
		if(bytes_read > 0)
		{
//			fprintf(stderr, "%s,%d: CAN Message recieved. ID:%0X Length:%d.\n", __FILE__, __LINE__, 
//				msg.can_id, msg.can_dlc);
			if(msg.can_id & 0x80130058)
			{
				if(msg.can_dlc == 6)
				{
					if(msg.data[0] & 0x80)
						fprintf(stderr, "Seatbelt is unbuckled.\n");
					else
						fprintf(stderr, "Seatbelt is buckled.\n");
				}

			}
		} else if(bytes_read == 0)
			break;

	}	

	fprintf(stderr, "%s,%d:CAN Connection is closed\n", __FILE__, __LINE__);
	return 0;
}
