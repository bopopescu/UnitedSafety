#include <iostream>
#include <list>
#include <vector>

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include "linux/i2c-dev.h"

#include "atslogger.h"
#include "ats-common.h"
#include "socket_interface.h"
#include "db-monitor.h"
#include "RedStone_IPC.h"
#include "AFS_Timer.h"
#include "NMEA_Client.h"

void ClearI2CLowBatteryFlag();

int g_dbg = 0;
ATSLogger g_log;

int main(int argc, char* argv[])
{
	REDSTONE_IPC redstone_ipc;
	NMEA_Client NMEA;
	AFS_Timer gpsTimer;
	SocketError err;

	g_log.open_testdata(argv[0]);
	g_log.set_global_logger(&g_log);

	ats_logf(&g_log, "%s Started", argv[0]);

	wait_for_app_ready("power-monitor");
	send_msg("localhost", 41009, &err, "set_work set_work_priority=1 key=crit-battery-check\r");  // keep system alive until check is finished
	wait_for_app_ready("message-assembler");
	ats_logf(&g_log, "message-assembler detected");
	
	while (!ats::file_exists("/tmp/logdir/wakeup.txt") )
		sleep(5);
	
  ats::String strWakeup;
  if (ats::get_file_line(strWakeup, "/tmp/logdir/wakeup.txt", 1, 0) == 0)
  {
		ats_logf(&g_log, "Critical Battery Wakeup - strWakeup %s", strWakeup.c_str());
		
		if (strWakeup == "crit_batt")
		{
			ats_logf(&g_log, "Critical Battery Wakeup - will send CriticalBatt message");

			// keep the system alive until we have gps and send the message
	
			// wait up to 2 minutes for valid GPS (remember we are in a critically low battery situation - can't wait forever)
			while (!NMEA.IsValid() && gpsTimer.DiffTime() < 120)
			{
				sleep(1);
			}
			// send the low battery message
			send_redstone_ud_msg("message-assembler", 0, "msg crit_batt set_work_priority=1\r");
			ClearI2CLowBatteryFlag();
		}
		else
			ats_logf(&g_log, "Critical Battery Wakeup - wakeup is not crit_batt");
  }
	send_msg("localhost", 41009, &err, "unset_work key=crit-battery-check\r");  // system can now go to sleep if it needs to.

	return 0;
}

void ClearI2CLowBatteryFlag()
{
	const char* dev_fname = "/dev/i2c-0";
	const int dev_addr = 0x30;
	const int fd = open(dev_fname, O_RDWR);

	if(fd < 0)
	{
		fprintf(stderr, "Failed to open \"%s\". (%d) %s\n", dev_fname, errno, strerror(errno));
		printf("error\n");
		return;
	}

	if(ioctl(fd, I2C_SLAVE, dev_addr))
	{
		fprintf(stderr, "%s,%d: ERR (%d) %s\n", __FILE__, __LINE__, errno, strerror(errno));
		printf("error\n");
		return;
	}

	if(ioctl(fd, I2C_SLAVE, dev_addr))
	{
		fprintf(stderr, "%s,%d: ERR (%d): %s", __FILE__, __LINE__, errno, strerror(errno));
		printf("error\n");
		return;
	}
	i2c_smbus_write_byte_data(fd, 0x26, 1);
}

