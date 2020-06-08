#pragma once

#include "ats-common.h"
#include "atslogger.h"

#define TGT_CMD_RESET_MCU           0x00
#define TGT_CMD_GET_VERSION         0x01
#define TGT_CMD_ERASE_FLASH_PAGE    0x02
#define TGT_CMD_WRITE_FLASH_BYTES   0x03
#define TGT_CMD_READ_FLASH_BYTES    0x04
#define TGT_CMD_ENTER_BL_MODE       0x05

// ---------------------------------
// Target BL Response Codes
// ---------------------------------
#define TGT_RSP_OK                  0x00 // RSP_OK should always be 0
#define TGT_RSP_PARAMETER_INVALID   0x01
#define TGT_RSP_UNSUPPORTED_CMD     0x02
#define TGT_RSP_BL_MODE             0x03
#define TGT_RSP_ERROR               0x80

//---------------------------------------------
// Error Codes
//---------------------------------------------
#define ERR_TGT_INFO_MISMATCH    0x01
#define ERR_TGT_BL_MODE          0x02
#define ERR_TGT_UNEXPECTED_RSP   0x03
#define ERR_TGT_CRC_MISMATCH     0x04

#define ERR_SRC_INFO_MISMATCH    0x10
#define ERR_SRC_CRC_MISMATCH     0x11
#define ERR_SRC_UNEXPECTED_RSP   0x12

#define ERR_TGT_SRC_INFO_MISMATCH   0x20
#define ERR_NUM_PAGES_MISMATCH      0x21

#define MAX_BUF_BYTES 27
#define SMB_ADDR 0x30
#define CMD_PKG_SIZE 6

// Flash Write/Erase Key Codes for Target MCU
#define FLASH_KEY0      0xA5
#define FLASH_KEY1      0xF1

// User applicate code
#define APP_FW_START_ADDR        0x0200
#define APP_FW_END_ADDR          0x3DFF
#define PAGE_SIZE 512

class targetWrite
{
public:
	static const ats::String gpio_dev_address;

	targetWrite();

	~targetWrite();

	int open_device();
	uint8_t TGT_Write_Flash(const uint8_t *buf, const uint32_t addr, const uint16_t numbytes);
	uint8_t TGT_Erase_Page(const uint32_t addr);
	uint8_t TGT_Enter_BL_Mode();
	void TGT_SW_Reset(void);

	int i2c_write(const int nBytes)const;
	int i2c_read_response()const;
	int i2c_read(const int command)const;

private:
	int m_fd;
	uint8_t SMB_Tx_Buf[MAX_BUF_BYTES + CMD_PKG_SIZE];
};
