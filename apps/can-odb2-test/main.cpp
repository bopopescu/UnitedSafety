#include <iostream>

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "redstone-socketcan.h"
#include "linux/can.h"

#define J1979_CAN_ID	0x7DF

unsigned int convertToInt(unsigned char * data, int len)
{
	if(len <= 0)	return 0;
	if(len == 1)	return (unsigned int)data[0];

	unsigned int ret_val = 0;
	for(int i=0;i<len;i++)
	{
		ret_val |= data[i] & 0xFF;
		ret_val <<= 8;
	}
	return ret_val;
}

int main()
{
	CANSocket *s = create_new_CANSocket();
	if(!s) {
		fprintf(stderr, "%s,%d: Failed to create CANSocket\n", __FILE__, __LINE__);
	}

	CAN_connect(s, "can0");
	fprintf(stderr, "%s,%d: CAN is connected.\n", __FILE__, __LINE__);

	pid_t pid = fork();
	if(pid < 0)
		return pid;
	else if(pid ==0)
	{
		unsigned char data[8];
		while(1)
		{
			memset(data, 0, 8);
			data[0] = 2;
			data[1] = 1;
			data[2] = 0x0D;
			CAN_WriteResult res = CAN_write(s, J1979_CAN_ID, data,8);
			fprintf(stderr, "%s,%d,: Written bytes: %d Error:%d\n", __FILE__, __LINE__, res.m_nwrite, res.m_err);
			sleep(5);
		} 
	}else {
		while(1)
		{
			struct can_frame msg;
		
			int bytes_read = CAN_read(s, &msg);
			if(bytes_read > 0)
			{
//				fprintf(stderr, "%s,%d: CAN Message recieved. ID:%0X Length:%d.\n", __FILE__, __LINE__, 
//					msg.can_id, msg.can_dlc);
				if((msg.can_id & 0xFFFFFF00) == 0x700)
				{
					if(msg.can_dlc > 3)
					{
						if((msg.data[1] == 0x41) && (msg.data[0] < 7) && (msg.data[0] >=3))
						{
							int length = msg.data[0];
							int val = convertToInt(&msg.data[3], length - 2);
							fprintf(stderr, "%s, %d: J1979 PID recieved. \n\tPID:%0X\n\tValue:%d\n", __FILE__, __LINE__, msg.data[2], val);
						}
					}

				}
			} else if(bytes_read == 0)
				break;

		}
	}	

	fprintf(stderr, "%s,%d:CAN Connection is closed\n", __FILE__, __LINE__);
	return 0;
}
