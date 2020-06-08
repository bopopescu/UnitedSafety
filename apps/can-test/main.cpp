#include <iostream>

#include <stdio.h>

#include "redstone-socketcan.h"

int main()
{
	CANSocket *s = create_new_CANSocket();
	if(!s) {
		fprintf(stderr, "%s,%d: Failed to create CANSocket\n", __FILE__, __LINE__);
	}

	CAN_connect(s, "can0");

	return 0;
}
