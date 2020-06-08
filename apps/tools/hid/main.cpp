#include <stdio.h>
#include <MovingAverage.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define VREF 3.7

static double ConvertADC(double value)
{
	return value*VREF/4096;
}

int main(int argc, char* argv[])
{
	MovingAverage avg;
	avg.SetSize(5);
	unsigned int count = 0;

	const char* dev_fname = "/dev/hid";

	for(;;)
	{
		const int fd = open(dev_fname, O_RDONLY);

		if(fd < 0)
		{
			fprintf(stderr, "Failed to open \"%s\". (%d) %s\n", dev_fname, errno, strerror(errno));
			printf("error\n");
			return 1;
		}

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

			if (count++ > 4)
			{
				break;
			}

			close(fd);
		}

		usleep(1000);
	}

	const double voltage = (double)ConvertADC(avg.Average());

	if(voltage >= 0.2 && voltage < 0.3)
	{
		printf("tl2500-2.0\n");
	}
	else if(voltage >= 0.3 && voltage <= 0.5 )
	{
		printf("tl2500-2.00\n");
	}
	else if(voltage >= 0.9 && voltage <= 1.1 )
	{
		printf("tl3000-2.00\n");
	}
	else if(voltage >= 1.6 && voltage <= 1.8  )
	{
		printf("tl3000-2.0\n");
	}
	else if(voltage >= 1.2 && voltage <= 1.4  )
	{
		printf("tl5000\n");
	}
	else if(voltage >= 0.80 && voltage <= 0.899  )
	{
		printf("tl5000-3.01\n");
	}
	else
	{
		printf("unknown %02f\n", voltage);
	}

	return 0;
}
