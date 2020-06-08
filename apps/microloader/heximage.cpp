#include <errno.h>

#include "heximage.h"

const ats::String InfoBlock::BLtype_UART("UART");
const ats::String InfoBlock::BLtype_CAN("CAN");
const ats::String InfoBlock::BLtype_LIN("LIN");
const ats::String InfoBlock::BLtype_SMBUS("SMBUS");
const ats::String InfoBlock::BLtype_Invalid("Invalid");

HexImage::HexImage(const ats::String& name):
	m_filename(name),
	m_hexImageValid(false),
	m_lowestSpecifiedAddr(0xFFFFFFFF),
	m_highestSpecifiedAddr(0)
{
	m_hexImage = new unsigned char[MaxFlashSize];
	m_pageSpecified512 = new bool[MaxFlashSize/512];
	m_pageSpecified1024 = new bool[MaxFlashSize/1024];
}

HexImage::~HexImage()
{
	delete []m_hexImage;
	delete []m_pageSpecified512;
	delete []m_pageSpecified1024;
}

void HexImage::initHexImage()
{
	for (int i = 0; i < MaxFlashSize; i++)
	{
		m_hexImage[i] = 0xFF;
	}

	for (int i = 0; i < (MaxFlashSize/512); i++)
	{
		m_pageSpecified512[i] = false;
	}

	for (int i = 0; i < (MaxFlashSize/1024); i++)
	{
		m_pageSpecified1024[i] = false;
	}
}

bool HexImage::openHexFile()
{
	bool ret = false;

	initHexImage();

	if(readHexFile() == true)
	{
		//ats_logf(ATSLOG(0),"Hex file read successfully");

		m_hexImageValid = true;

		// The first byte of AppFW space has to be the lowest specified addr in the hex file
		AppFWstartAddr(m_lowestSpecifiedAddr);

		// Add number of signature/CRC bytes to highest specified addr to get app FW end addr
		AppFWendAddr(m_highestSpecifiedAddr); 

		initHexImageInfo();

		ret = true;

	}

	return ret;
}

bool HexImage::readHexFile()
{
	std::ifstream infile(m_filename.c_str());

	if(!infile.is_open())
	{
		ats_logf(ATSLOG(0), "Failed to open HEX file \"%s\": (%d) %s", m_filename.c_str(), errno, strerror(errno));
		exit(1);
	}

	ats::String rawLine;

	int line_index;
	uint8_t running_checksum;

	uint8_t rec_len;
	uint8_t addr_temp;
	uint32_t addr_offset;
	uint8_t rec_type;
	uint8_t data_byte;
	uint8_t line_checksum;

	bool img_parse_success = false;
	bool parse_error = false;

	while ((std::getline(infile, rawLine)) && (!rawLine.empty()) && (img_parse_success == false) && (parse_error == false))
	{
		/*
		 * Intel Hex File Line Format (Example)
		 *     : 10 2462 00 464C5549442050524F46494C4500464C 33
		 *     | |  |    |  |                                |
		 *     | |  |    |  |                                +->Checksum [2 hex digits]
		 *     | |  |    |  |
		 *     | |  |    |  +->Data [ (Record Length) * 2 hex digits]
		 *     | |  |    |
		 *     | |  |    +->Record Type [2 hex digits]
		 *     | |  |
		 *     | |  +->Address [4 hex digits]
		 *     | |
		 *     | +->Record Length, which is simply the number of bytes in the "Data" field [2 hex digits]
		 *     |
		 *     +->Colon [1 byte]
		 *
		 */
		line_index = 0;

		if (rawLine.substr(line_index++, 1) == ":")
		{
			running_checksum = 0;

			rec_len = strtol(rawLine.substr(line_index, 2).c_str(), 0, 16);
			line_index += 2;
			running_checksum += rec_len;

			addr_temp = strtol(rawLine.substr(line_index, 2).c_str(), 0, 16);
			running_checksum += addr_temp;

			addr_temp = strtol(rawLine.substr(line_index + 2, 2).c_str(), 0, 16);
			running_checksum += addr_temp;

			addr_offset = strtol(rawLine.substr(line_index, 4).c_str(), 0, 16);
			line_index += 4;

			rec_type = strtol(rawLine.substr(line_index, 2).c_str(), 0, 16);
			line_index += 2;
			running_checksum += rec_type;

			switch (rec_type)
			{
				case 0x00:  // Data Record
					//Loop through the data and store it in our data array that maps to memory
					for (uint data_index = 0; data_index < rec_len; data_index++)
					{
						data_byte = strtol(rawLine.substr(line_index, 2).c_str(), 0, 16);
						line_index += 2;

						const uint addr = addr_offset + data_index;

						if (addr >= (uint)MaxFlashSize)
						{
							ats_logf(ATSLOG(0),"Hex file read error: Addr from hex file out of bounds");
							parse_error = true;
							break;
						}
						else
						{
							if (addr < m_lowestSpecifiedAddr)
							{
								m_lowestSpecifiedAddr = addr;
							}
							if (addr > m_highestSpecifiedAddr)
							{
								m_highestSpecifiedAddr = addr;
							}

							m_hexImage[addr] = data_byte;
							m_pageSpecified512[addr >> 9] = true;
							m_pageSpecified1024[addr >> 10] = true;
							running_checksum += data_byte;
						}
					}

					if (parse_error)
					{
						break;
					}

					// Read the line checksum
					line_checksum = strtol(rawLine.substr(line_index, 2).c_str(), 0, 16);
					running_checksum += line_checksum;

					// If the final sum isn't 0, there was an error
					if (running_checksum != 0x00)
					{
						parse_error = true;
					}

					break;

				case 0x01:  // End-of-File Record
					img_parse_success = true;
					break;

				default:    // Unknown record type
					parse_error = true;
					break;
			}

		}
		else
		{
			parse_error = true;
		}

	}

	if ((img_parse_success == true) && (parse_error == false))
	{
		ats_logf(ATSLOG(0),"Hex file read successfully!");

		return true;
	}

	ats_logf(ATSLOG(0),"Hex file read error");

	return false;
}

void HexImage::initHexImageInfo()
{
	const uint32_t addr = AppFWendAddr();

	InfoBlockLength(m_hexImage[addr - (int)infoBlockLength]);
	MCUcode(m_hexImage[addr - (int)mCUcode]);
	BLtypeRaw(m_hexImage[addr - (int)BLtype]);
	FlashPageSizeCodeRaw(m_hexImage[addr - (int)flashPageSizeCode]);
	AppFWversionLow(m_hexImage[addr - (int)appFWver0]);
	AppFWversionHigh(m_hexImage[addr - (int)appFWver1]);
	CANdeviceAddr(m_hexImage[addr - (int)cANdeviceAddr]);

	//ats_logf(ATSLOG(0),"%s,%d: InfoBlockLength: %x, MCUcode: %x, BLtypeRaw: %x, FlashPageSizeCodeRaw: %x, AppFWversionLow: %x, AppFWversionHigh: %x, CANdeviceAddr: %x", 
	//		__FILE__, __LINE__, InfoBlockLength(), MCUcode(), BLtypeRaw(), FlashPageSizeCodeRaw(), AppFWversionLow(), AppFWversionHigh(), CANdeviceAddr());
}

void HexImage::getPageSpecified512(bool (&pageSpecified512)[MaxFlashSize/512])const
{
	memcpy(pageSpecified512, m_pageSpecified512, MaxFlashSize/512);
}

void HexImage::getHexImageInfo(uint8_t (&src_info)[SRC_Enum_End_Value])const
{
	src_info[(int)SRC_Info_block_Length] = InfoBlockLength();
	src_info[(int)SRC_MCU_Code] = MCUcode();
	src_info[(int)SRC_BL_Type] = BLtypeRaw();
	src_info[(int)SRC_Flash_Page_Size_Code] = FlashPageSizeCodeRaw();
	src_info[(int)SRC_App_FW_Ver_Low] = AppFWversionLow();
	src_info[(int)SRC_App_FW_Ver_High] = AppFWversionHigh();
	src_info[(int)SRC_SMBUS_Device_Addr] = CANdeviceAddr();
	src_info[(int)SRC_App_FW_Start_Addr0] = (uint8_t)(AppFWstartAddr() & 0xFF);
	src_info[(int)SRC_App_FW_Start_Addr1] = (uint8_t)((AppFWstartAddr() & 0xFF00) >> 8);
	src_info[(int)SRC_App_FW_Start_Addr2] = (uint8_t)((AppFWstartAddr() & 0xFF0000) >> 16);
	src_info[(int)SRC_App_FW_End_Addr0] = (uint8_t)(AppFWendAddr() & 0xFF);
	src_info[(int)SRC_App_FW_End_Addr1] = (uint8_t)((AppFWendAddr() & 0xFF00) >> 8);
	src_info[(int)SRC_App_FW_End_Addr2] = (uint8_t)((AppFWendAddr() & 0xFF0000) >> 16);

}

void HexImage::ExtractHexImageBytes(uint8_t* outputBuf, int outputBufOffset, uint hexImageStartAddr, int numBytes)const
{
	int numBytesLimit = numBytes;

	if ((hexImageStartAddr + numBytes - 1) > AppFWendAddr())
	{
		numBytesLimit = AppFWendAddr() - hexImageStartAddr;
	}

	for (int i = 0; i < numBytesLimit; i++)
	{
		outputBuf[outputBufOffset + i] = m_hexImage[hexImageStartAddr + i];
	}
}


