#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "linux/i2c-dev.h"
#include "ats-common.h"
#include "MyData.h"

//-----------------------------------------------------------------------------------------------
int write_expander_hw(MyData& p_md, int p_expander, int p_byte)
{
	const int exp_addr = p_md.m_exp_addr[p_expander];
	p_md.lock();
	GPIO* gv = p_md.m_gpio->get_gpio(exp_addr, p_byte);

	int val =
		(gv[0].get_value() << 7)
		|(gv[1].get_value() << 6)
		| (gv[2].get_value() << 5)
		| (gv[3].get_value() << 4)
		| (gv[4].get_value() << 3)
		| (gv[5].get_value() << 2)
		| (gv[6].get_value() << 1)
		| gv[7].get_value();
//	val = ~val;

	if(ioctl(p_md.m_fd, I2C_SLAVE, exp_addr))
	{
		p_md.unlock();
		ats_logf(ATSLOG(0), "%s,%d: ERR (%d): %s", __FILE__, __LINE__, errno, strerror(errno));
		ats_logf(ATSLOG(0), "expander:%d  exp_addr %d ", p_expander, exp_addr);
		return 1;
	}

	i2c_smbus_write_byte_data(p_md.m_fd, p_byte, val);
	p_md.unlock();
	return 0;
}

//-----------------------------------------------------------------------------------------------
void write_expander(MyData& p_md, const ats::String* p_owner, int p_expander, int p_byte, int p_pin, int p_val)
{
	const int exp_addr = p_md.m_exp_addr[p_expander];

	p_md.lock();
	GPIO* gv = p_md.m_gpio->get_gpio(exp_addr, p_byte);
	gv[7 - (p_pin & 0x7)].set_value(p_owner, p_val);
	p_md.unlock();
}

//-----------------------------------------------------------------------------------------------
int read_expander_hw(MyData& p_md, int p_expander, int p_byte, int& p_val)
{
	const int exp_addr = p_md.m_exp_addr[p_expander];

	p_md.lock();

	if(ioctl(p_md.m_fd, I2C_SLAVE, exp_addr))
	{
		p_md.unlock();
		ats_logf(ATSLOG(0), "%s,%d: ERR (%d): %s", __FILE__, __LINE__, errno, strerror(errno));
		return 1;
	}

	p_val = i2c_smbus_read_byte_data(p_md.m_fd, p_byte);

	p_md.unlock();
	return 0;
}

