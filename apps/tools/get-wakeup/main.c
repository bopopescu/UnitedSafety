#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/wait.h>
#include "linux/i2c-dev.h"

static const int g_rtc_bit = 0x01;
static const int g_accel_bit = 0x02;
static const int g_inp1_bit = 0x04;
static const int g_inp2_bit = 0x08;
static const int g_inp3_bit = 0x10;
static const int g_can_bit = 0x020;
static const int g_batt_volt_bit = 0x40;
static const int g_low_batt_bit = 0x80;

static int called_as(const char* p_argv_entry, const char* p_program_name)
{
	const char* s = strstr(p_argv_entry, p_program_name);
	// If name found, and is last part of string, and (name occurs at beginning of string, or follows '/')
	return (s && (0 == strcmp(p_program_name, s)) && ((s == p_argv_entry) || ('/' == s[-1])));
}

static void h_set_wakeup_mask(const char* p_arg, int* p_wakeup)
{
	const int invert = ('~' == p_arg[0]);

	if(invert)
	{
		++p_arg;
	}

	const int wakeup = *p_wakeup;

	if(!strcasecmp("rtc", p_arg))
	{
		*p_wakeup = (invert) ? (wakeup & (~g_rtc_bit)) : (wakeup | g_rtc_bit);
	}
	else if(!strcasecmp("accel", p_arg))
	{
		*p_wakeup = (invert) ? (wakeup & (~g_accel_bit)) : (wakeup | g_accel_bit);
	}
	else if(!strcasecmp("inp1", p_arg))
	{
		*p_wakeup = (wakeup | g_inp1_bit); //ISCP-296- Always enabled the Ignition turning on
	}
	else if(!strcasecmp("inp2", p_arg))
	{
		*p_wakeup = (invert) ? (wakeup & (~g_inp2_bit)) : (wakeup | g_inp2_bit);
	}
	else if(!strcasecmp("inp3", p_arg))
	{
		*p_wakeup = (invert) ? (wakeup & (~g_inp3_bit)) : (wakeup | g_inp3_bit);
	}
	else if(!strcasecmp("can", p_arg))
	{
		*p_wakeup = (invert) ? (wakeup & (~g_can_bit)) : (wakeup | g_can_bit);
	}
	else if(!strcasecmp("batt_volt", p_arg))
	{
		*p_wakeup = (invert) ? (wakeup & (~g_batt_volt_bit)) : (wakeup | g_batt_volt_bit);
	}
	else if(!strcasecmp("low_batt", p_arg))
	{
		*p_wakeup = (invert) ? (wakeup & (~g_low_batt_bit)) : (wakeup | g_low_batt_bit);
	}
	else
	{
		fprintf(stderr, "unknown wakeup \"%s\", cannot set\n", p_arg);
	}

}

// Description: Sets the wakeup mask register on the micro to match the settings in
//	"argv" or from the system configuration database (only if the setting would be different than
//	what is currently set in the micro).
//
//	If "argc" is greater than 1, then "argv" will be the source used for the wakeup mask
//	settings. Otherwise the system configuration database will be used.
//
//	"p_fd" is the file descriptor to the micro's I2C interface.
//
// Return: 0 is returned on success and 1 is returned on error.
static int do_set_wakeup_mask(int argc, char* argv[], int p_fd)
{
	const int current_wakeup = i2c_smbus_read_byte_data(p_fd, 0x15) & 0xff;
	int wakeup = current_wakeup;

	if(argc <= 1)
	{
		int pipe_fd[2];
		pipe(pipe_fd);
		const int pid = fork();
		const char* db_config = "/usr/bin/db-config";

		if(!pid)
		{
			close(pipe_fd[0]);
			dup2(pipe_fd[1], STDOUT_FILENO);
			execl(db_config, db_config, "-n", "-v", "get", "wakeup", "mask", (char*)NULL);
			exit(1);
		}

		close(pipe_fd[1]);

		static const int max_wakeup_mask_string = 256;
		char mask[max_wakeup_mask_string];
		{
			ssize_t remain = sizeof(mask) - 1;
			char* s = mask;

			for(;remain;)
			{
				char buf[256];
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
				fprintf(stderr, "mask string is too long\n");
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

		const char* delim = ", \t\n";
		char* next_flag = strtok(mask, delim);

		for(;next_flag;)
		{
			h_set_wakeup_mask(next_flag, &wakeup);
			next_flag = strtok(NULL, delim);
		}

	}
	else
	{
		int i;

		for(i = 1; i < argc; ++i)
		{
			const char* arg = argv[i];

			if('-' == arg[0])
			{
				// XXX: Ignoring options for now
				continue;
			}

			h_set_wakeup_mask(arg, &wakeup);
		}

	}

	if(wakeup != current_wakeup)
	{
		i2c_smbus_write_byte_data(p_fd, 0x20, wakeup);
	}

	return 0;
}

static int do_get_wakeup_mask(int argc, char* argv[], int p_fd)
{
	int human_readable = 0;
	int i;

	for(i = 1; i < argc; ++i)
	{
		const char* arg = argv[i];

		if('-' == (*arg))
		{

			while(*(++arg))
			{

				switch(*arg)
				{
				case 'h': human_readable = 1; break;
				}

			}

			continue;
		}

	}

	const int mask = i2c_smbus_read_byte_data(p_fd, 0x15) & 0xff;

	if(human_readable)
	{
		const char* wakeup = "\x1b[1;32;40mWakeup on ";
		const char* ignore = "\x1b[1;31;40mIgnore on ";
		const char* reset = "\x1b[0m";
		printf(
			"%srtc%s\n"
			"%saccel%s\n"
			"%sinp1%s\n"
			"%sinp2%s\n"
			"%sinp3%s\n"
			"%scan%s\n"
			"%sbatt_volt%s\n"
			"%slow_batt%s\n",
			(mask & g_rtc_bit) ? wakeup : ignore, reset,
			(mask & g_accel_bit) ? wakeup : ignore, reset,
			(mask & g_inp1_bit) ? wakeup : ignore, reset,
			(mask & g_inp2_bit) ? wakeup : ignore, reset,
			(mask & g_inp3_bit) ? wakeup : ignore, reset,
			(mask & g_can_bit) ? wakeup : ignore, reset,
			(mask & g_batt_volt_bit) ? wakeup : ignore, reset,
			(mask & g_low_batt_bit) ? wakeup : ignore, reset);
		return 0;
	}

	const char* complement = "~";
	printf(
		"%srtc,%saccel,%sinp1,%sinp2,%sinp3,%scan,%sbatt_volt,%slow_batt\n",
		(mask & g_rtc_bit) ? "" : complement,
		(mask & g_accel_bit) ? "" : complement,
		(mask & g_inp1_bit) ? "" : complement,
		(mask & g_inp2_bit) ? "" : complement,
		(mask & g_inp3_bit) ? "" : complement,
		(mask & g_can_bit) ? "" : complement,
		(mask & g_batt_volt_bit) ? "" : complement,
		(mask & g_low_batt_bit) ? "" : complement
	);
	return 0;
}

static int do_get_batt_wakeup_threshold(int argc, char* argv[], int p_fd)
{
	const int mV = i2c_smbus_read_word_data(p_fd, 0x16) & 0xffff;
	printf( "%d\n", mV);
	return 0;
}

static int do_get_low_batt_limit(int argc, char* argv[], int p_fd)
{
	const int mV = i2c_smbus_read_word_data(p_fd, 0x17) & 0xffff;
	printf( "%d\n", mV);
	return 0;
}

static int do_set_batt_wakeup_threshold(int argc, char* argv[], int p_fd)
{

	if(argc < 2)
	{
		fprintf(stderr, "usage: %s <battery wakeup threshold in mV>\n", argv[0]);
		return 1;
	}

	const unsigned int mV = strtol(argv[1], 0, 0);
	i2c_smbus_write_byte_data(p_fd, 0x21, mV >> 8);
	i2c_smbus_write_byte_data(p_fd, 0x22, mV);
	return 0;
}

static int do_set_low_batt_limit(int argc, char* argv[], int p_fd)
{

	if(argc < 2)
	{
		fprintf(stderr, "usage: %s <low battery limit in mV>\n", argv[0]);
		return 1;
	}

	const unsigned int mV = strtol(argv[1], 0, 0);
	i2c_smbus_write_byte_data(p_fd, 0x23, mV >> 8);
	i2c_smbus_write_byte_data(p_fd, 0x24, mV);
	return 0;
}

static int do_get_batt_voltage(int argc, char* argv[], int p_fd)
{
	const int mV = i2c_smbus_read_word_data(p_fd, 0x13) & 0xffff;
	printf("%d\n", mV);
	return 0;
}

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

	if((argc >= 1) && called_as(argv[0], "get-batt-voltage"))
	{
		return do_get_batt_voltage(argc, argv, fd);
	}

	if((argc >= 1) && called_as(argv[0], "set-wakeup-mask"))
	{
		return do_set_wakeup_mask(argc, argv, fd);
	}

	if((argc >= 1) && called_as(argv[0], "get-wakeup-mask"))
	{
		return do_get_wakeup_mask(argc, argv, fd);
	}

	if((argc >= 1) && called_as(argv[0], "get-batt-wakeup-threshold"))
	{
		return do_get_batt_wakeup_threshold(argc, argv, fd);
	}

	if((argc >= 1) && called_as(argv[0], "get-low-batt-limit"))
	{
		return do_get_low_batt_limit(argc, argv, fd);
	}

	if((argc >= 1) && called_as(argv[0], "set-batt-wakeup-threshold"))
	{
		return do_set_batt_wakeup_threshold(argc, argv, fd);
	}

	if((argc >= 1) && called_as(argv[0], "set-low-batt-limit"))
	{
		return do_set_low_batt_limit(argc, argv, fd);
	}

	// AWARE360 FIXME: The Micro/Kernel I2C interface is broken, and the below solution is a work-around.
	//	Fix the Micro/Kernel I2C interface!
	//
	// just get the wakeup - need two readings consecutive to be the same to ensure we are not getting cross-talk on the i2c bus
	int w1, w2;
	w1 = i2c_smbus_read_byte_data(fd, 0x12) & 0xff;
	w2 = i2c_smbus_read_byte_data(fd, 0x12) & 0xff;
  int count = 0;
  
	while (w1 != w2 && ++count < 5)
	{
		w1 = i2c_smbus_read_byte_data(fd, 0x12) & 0xff;
		w2 = i2c_smbus_read_byte_data(fd, 0x12) & 0xff;
	}

	switch(w1)
	{
		case 0x00: printf("power\n"); break;
		case 0x01: printf("rtc\n"); break;
		case 0x02: printf("accel\n"); break;
		case 0x04: printf("inp1\n"); break;
		case 0x08: printf("inp2\n"); break;
		case 0x10: printf("inp3\n"); break;
		case 0x20: printf("can\n"); break;
		case 0x40: printf("batt_volt\n"); break;
		case 0x80: printf("crit_batt\n"); break;
		default: printf("unknown 0x%02X\n", w1); break;
	}

	return 0;
}
