#include <errno.h>
#include <fcntl.h>

#include "linux/i2c-dev.h"
#include "targetwrite.h"

const ats::String targetWrite::gpio_dev_address("/dev/i2c-0");

targetWrite::targetWrite()
{
	m_fd = -1;
}

targetWrite::~targetWrite()
{
	close(m_fd);
	m_fd = -1;
}

int targetWrite::open_device()
{
	m_fd = open(gpio_dev_address.c_str(), O_RDWR);

	if(m_fd < 0)
	{
		ats_logf(ATSLOG(0), "ERR: Failed to open %s: (%d) %s", gpio_dev_address.c_str(), errno, strerror(errno));
		return 1;
	}

	return 0;
}

void targetWrite::TGT_SW_Reset()
{
	SMB_Tx_Buf[0] = TGT_CMD_RESET_MCU;
	i2c_write(CMD_PKG_SIZE);
	i2c_read_response();
}

uint8_t targetWrite::TGT_Enter_BL_Mode()
{
	uint8_t response;
	SMB_Tx_Buf[0] = TGT_CMD_ENTER_BL_MODE;
	i2c_write(1);
	response = i2c_read_response();
	return response;
}

uint8_t targetWrite::TGT_Erase_Page(const uint32_t addr)
{
	uint8_t response;

	if ((addr < APP_FW_START_ADDR) || (addr > APP_FW_END_ADDR))
	{
		response = TGT_RSP_PARAMETER_INVALID;
	}
	else
	{
		SMB_Tx_Buf[0] = TGT_CMD_ERASE_FLASH_PAGE;
		SMB_Tx_Buf[1] = FLASH_KEY0;
		SMB_Tx_Buf[2] = FLASH_KEY1;
		SMB_Tx_Buf[3] = addr & 0xFF;
		SMB_Tx_Buf[4] = (addr >> 8) & 0xFF;

		i2c_write(CMD_PKG_SIZE);
		response = i2c_read_response();
	}

	return response;
}

// SMB_Tx_Buf[0] is command, SMB_Tx_Buf[1] to SMB_Tx_Buf[5] are reserved for parameter, and left 27 bytes for real application data.
uint8_t targetWrite::TGT_Write_Flash(const uint8_t* buf, const uint32_t addr, uint16_t numbytes)
{
	uint8_t response = -1;

	if(numbytes > MAX_BUF_BYTES)
	{
		ats_logf(ATSLOG(0), "%s,%d: Invalid parameter, data length should not greater than 27", __FILE__, __LINE__);
		return response;
	}

	if((addr + numbytes - 1) > APP_FW_END_ADDR)
	{
		numbytes = APP_FW_END_ADDR - addr + 1;
	}

	if ((addr < APP_FW_START_ADDR) || (addr > APP_FW_END_ADDR))
	{
		response = TGT_RSP_PARAMETER_INVALID;
	}
	else
	{
		// Command Format:
		// [0] Command
		// [1] flash key code0
		// [2] flash key code1
		// [3] addr0 (LSB)
		// [4] addr1 (MSB)
		// [5] numbytes
		SMB_Tx_Buf[0] = TGT_CMD_WRITE_FLASH_BYTES;
		SMB_Tx_Buf[1] = FLASH_KEY0;
		SMB_Tx_Buf[2] = FLASH_KEY1;
		SMB_Tx_Buf[3] = addr & 0xFF;
		SMB_Tx_Buf[4] = (addr >> 8) & 0xFF;
		SMB_Tx_Buf[5] = numbytes;
		int i;

		for(i = 0; i < numbytes; i++)
		{
			SMB_Tx_Buf[i + CMD_PKG_SIZE] = *(buf + i);
		}

		i2c_write(CMD_PKG_SIZE + numbytes - 1);
		response = i2c_read_response();
	}

	return response;
}

//Attn: This function require the nBytes should be not greater than 32 bytes.
//i2c_smbus_write_i2c_block_data could send maximum 33 bytes which including one byte command and 32 bytes data.
int targetWrite::i2c_write(int nBytes)const
{
	int ret = -1;

	if(nBytes > 32)
	{
		ats_logf(ATSLOG(0), "%s,%d: Invalid parameter, Buffer length should not greater than 32", __FILE__, __LINE__);
		return ret;
	}

	if(ioctl(m_fd, I2C_SLAVE, SMB_ADDR))
	{
		ats_logf(ATSLOG(0), "%s,%d: ERR (%d): %s", __FILE__, __LINE__, errno, strerror(errno));
		ret = errno;
		return ret;
	}

	ret = i2c_smbus_write_i2c_block_data(m_fd, SMB_Tx_Buf[0], nBytes, (__u8*)(SMB_Tx_Buf + 1));
	return ret;
}

int targetWrite::i2c_read_response()const
{
	int ret = -1;

	if(ioctl(m_fd, I2C_SLAVE, SMB_ADDR))
	{
		ats_logf(ATSLOG(0), "%s,%d: ERR (%d): %s", __FILE__, __LINE__, errno, strerror(errno));
		ret = errno;
		return ret;
	}

	ret = i2c_smbus_read_byte(m_fd);
	return ret;
}

int targetWrite::i2c_read(int command)const
{
	int ret = -1;

	if(ioctl(m_fd, I2C_SLAVE, SMB_ADDR))
	{
		ats_logf(ATSLOG(0), "%s,%d: ERR (%d): %s", __FILE__, __LINE__, errno, strerror(errno));
		ret = errno;
		return ret;
	}

	ret = i2c_smbus_read_byte_data(m_fd, command);
	return ret;
}
