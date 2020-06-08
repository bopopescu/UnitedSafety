#include <sys/time.h>
#include <unistd.h>

#include "socket_interface.h"

int main(int argc, char* argv[])
{

	for(;;)
	{
		struct timeval t;
		gettimeofday(&t, 0);
		//send_redstone_ud_msg("app-monitor-msg", 0, "producer,consumer,[sec(%d), usec(%d)] \"Hello, World!\"\r", int(t.tv_sec), int(t.tv_usec));
		send_app_msg("producer", "consumer", 0, "[sec(%d), usec(%d)] \"Hello, World!\"\r", int(t.tv_sec), int(t.tv_usec));
		sleep(1);
	}

	return 0;
}
