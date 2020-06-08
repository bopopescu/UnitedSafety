#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/wait.h>
#include "linux/i2c-dev.h"

#define AC_WHOAMI                0x0D
#define AC_DEVICE_ID             0x3A
#define AC_DEVICE_ADD            0x1C
#define AC_REG_CTRL1             0x2A
#define AC_REG_CTRL4             0x2D
#define AC_REG_CTRL5             0x2E
#define AC_REG_TRANSIENT_CFG     0x1D
#define AC_REG_TRANSIENT_THR     0x1F
#define AC_REG_TRANSIENT_CNT     0x20

#define AC_DEFAULT_ACCELERATION  2.0
#define AC_TRANSIENT_COUNT       5
#define BIT0                     0x0001
#define AC_ACTIVE_MODE           BIT0

#define AC_THRESHOLD_RESOLUTION  0.063

//itraker.gps1.com/issues/1795
static float g_acceleration = 80.0;

static int read_dbconfig(const char* app, const char* key)
{
	int pipe_fd[2];
	pipe(pipe_fd);
	const int pid = fork();
	const char* db_config = "/usr/bin/db-config";

	if(!pid)
	{
		close(pipe_fd[0]);
		dup2(pipe_fd[1], STDOUT_FILENO);
		execl(db_config, db_config, "-n", "-v", "get", app, key, (char*)NULL);
		exit(1);
	}

	close(pipe_fd[1]);

	static const int max_string = 32;
	char v[max_string];
	{
		ssize_t remain = sizeof(v) - 1;
		char* s = v;

		for(;remain;)
		{
			char buf[32];
			const ssize_t nread = read(pipe_fd[0], buf, (remain > sizeof(buf)) ? sizeof(buf) : remain);

			if(nread <= 0)
			{
				break;
			}

			memcpy(s, buf, nread);
			s += nread;
			remain -= nread;
		}

		if(!remain)
		{
			fprintf(stderr, "string is too long\n");
			return 1;
		}

		*s = '\0';
	}

	int status;
	waitpid(pid, &status, 0);

	if(WIFEXITED(status))
	{
		const int ret = WEXITSTATUS(status);

		if(ret)
		{
			fprintf(stderr, "%s returned %d\n", db_config, ret);
			return 1;
		}

	}
	else
	{
		fprintf(stderr, "%s failed: status is 0x%08X\n", db_config, status);
		return 1;
	}

	float r = atof(v);
	if( r > 0 && r <= 80.0 )
		g_acceleration = r;

	return 0;
}

static int read_register( int p_fd, int reg )
{
	return i2c_smbus_read_byte_data(p_fd, reg ) & 0xff;
}

static int write_register( int p_fd, int reg, int value )
{
	int status = i2c_smbus_write_byte_data(p_fd, reg, value );
	if( status )
	{
		fprintf( stderr, "%s,%d: ERR: Write Register: %x Fail", __FILE__, __LINE__, reg );
		return 1;
	}

	return 0;
}

int main(int argc, char* argv[])
{
	const char* dev_fname = "/dev/i2c-0";
	const int dev_addr = AC_DEVICE_ADD;
	const int fd = open(dev_fname, O_RDWR);

	if(fd < 0)
	{
		fprintf(stderr, "Failed to open \"%s\". (%d) %s\n", dev_fname, errno, strerror(errno));
		return 1;
	}

	if(ioctl(fd, I2C_SLAVE, dev_addr))
	{
		fprintf(stderr, "%s,%d: ERR (%d): %s", __FILE__, __LINE__, errno, strerror(errno));
		return 1;
	}

	if( read_dbconfig("wakeup", "AccelTriggerG" ))
		return 1;

	if( read_register( fd, AC_WHOAMI ) != AC_DEVICE_ID)
	{
		fprintf(stderr, "%s,%d: ERR: Accelerometer device not found", __FILE__, __LINE__);
		return 1;
	}

	//enter standby mode
	int status = read_register( fd, AC_REG_CTRL1 );
	status &= ~AC_ACTIVE_MODE;
	write_register(fd, AC_REG_CTRL1, status);

	write_register( fd, AC_REG_TRANSIENT_CFG, 0x1E);
	int g = (int)(0.5 + (g_acceleration/10.0)/(AC_THRESHOLD_RESOLUTION));
	write_register( fd, AC_REG_TRANSIENT_THR, g );
	write_register( fd, AC_REG_TRANSIENT_CNT, 0x05);
	write_register( fd, AC_REG_CTRL4, 0x20);
	write_register( fd, AC_REG_CTRL5, 0x20);

	//enter active mode
	status = read_register( fd, AC_REG_CTRL1 );
	status |= AC_ACTIVE_MODE;
	write_register(fd, AC_REG_CTRL1, status);

	close(fd);
	return 0;
}
