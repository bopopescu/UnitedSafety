#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include "linux/i2c-dev.h"

int main(int argc, char* argv[])
{
	const char* dev_fname = "/dev/i2c-0";
	const int dev_addr = 0x30;
	const int fd = open(dev_fname, O_RDWR);

	if(fd < 0)
	{
		fprintf(stderr, "Failed to open \"%s\". (%d) %s\n", dev_fname, errno, strerror(errno));
		printf("error\n");
		return 1;
	}

	if(ioctl(fd, I2C_SLAVE, dev_addr))
	{
		fprintf(stderr, "%s,%d: ERR (%d) %s\n", __FILE__, __LINE__, errno, strerror(errno));
		printf("error\n");
		return 1;
	}

	if(ioctl(fd, I2C_SLAVE, dev_addr))
	{
		fprintf(stderr, "%s,%d: ERR (%d): %s", __FILE__, __LINE__, errno, strerror(errno));
		printf("error\n");
		return 1;
	}

	printf("%s\n", (0x56 == (i2c_smbus_read_byte_data(fd, 0x10) & 0xff)) ? "yes" : "no");
	return 0;
}
