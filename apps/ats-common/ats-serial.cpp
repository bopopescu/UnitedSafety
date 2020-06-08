#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>

#include "ats-serial.h"

int ats::setup_modem_serial_port(const ats::String& p_port)
{
	const int fd = open(p_port.c_str(), O_RDWR);

	if(fd < 0)
	{
		return -errno;
	}

	struct termios t;

	if(-1 == tcgetattr(fd, &t))
	{
		const int err = errno;
		close(fd);
		return -err;
	}

	t.c_iflag &= ~BRKINT;
	t.c_iflag &= ~ICRNL;
	t.c_iflag &= ~IMAXBEL;
	t.c_oflag &= ~OPOST;
	t.c_lflag &= ~ISIG;
	t.c_lflag &= ~ICANON;
	t.c_lflag &= ~IEXTEN;
	t.c_lflag &= ~ECHO;

	if(-1 == tcsetattr(fd, 0, &t))
	{
		const int err = errno;
		close(fd);
		return -err;
	}

	close(fd);
	return 0;
}
