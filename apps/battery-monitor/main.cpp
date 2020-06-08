#include <stdio.h>
#include <MovingAverage.h>
#include "RedStone_IPC.h"

#include "adc2volt.h"

// Monitor the battery voltage every second, create a 5 second moving
// average.
// Every second - place the moving average in shared memory
int main(int argc, char *argv[])
{
	REDSTONE_IPC redStoneData;

	MovingAverage avg;
	avg.SetSize(5);
	unsigned int count = 0;  
  
	for(;;)
	{
		const int fd = open("/dev/battery", O_RDONLY);

		if(fd >= 0)
		{
			char buf[32];
			const int len = read(fd, buf, 26);

			if (len > 0)
			{
				buf[len] = '\0';
				const double dval = atof(buf);
				avg.Add(dval);
			}

			if (count++ > 4) // set reading valid after at least 4 seconds
			{
				redStoneData.BatteryVoltageValid(true);
			}

			const double voltage = (double)ats::battery::ADC2Voltage((int)(avg.Average())); 
			redStoneData.BatteryVoltage(voltage);

			close(fd);
		}

		sleep(1);
	}

	return 0;
}
