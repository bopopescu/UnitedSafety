#pragma once

#include <fstream>

#include "ats-common.h"
#include "atslogger.h"

class InfoBlock
{
public:
	static const ats::String BLtype_UART;
	static const ats::String BLtype_CAN;
	static const ats::String BLtype_LIN;
	static const ats::String BLtype_SMBUS;
	static const ats::String BLtype_Invalid;

	InfoBlock():
		mInfoBlockLength(0),
		mMCUcode(0),
		mBLtype(0),
		mFlashPageSizeCode(0),
		mAppFWversionLow(0),
		mAppFWversionHigh(0),
		mCANdeviceAddr(0),
		mAppFWstartAddr(0),
		mAppFWendAddr(0)
	{};

	uint8_t InfoBlockLength() const
	{
		return mInfoBlockLength;
	}

	void InfoBlockLength(uint8_t value)
	{
		mInfoBlockLength = value;
	}

	uint8_t MCUcode() const
	{
		return mMCUcode;
	}

	void MCUcode(uint8_t value)
	{
		mMCUcode = value;
	}

	uint8_t BLtypeRaw() const
	{
		return mBLtype;
	}

	void BLtypeRaw(uint8_t value)
	{
		mBLtype = value;
	}

	ats::String BLtype() const
	{
		return DecodeBLtype();
	}

	const ats::String& DecodeBLtype() const
	{

		switch (mBLtype)
		{
		case 1: return BLtype_UART;
		case 2: return BLtype_CAN;
		case 3: return BLtype_LIN;
		case 4: return BLtype_SMBUS;
		}

		return BLtype_Invalid;
	}

	uint8_t FlashPageSizeCodeRaw()const
	{
		return mFlashPageSizeCode;
	}

	uint32_t FlashPageSize()const
	{
		return DecodeFlashPageSizeCode();
	}

	uint32_t DecodeFlashPageSizeCode()const
	{
		uint32_t retval;

		switch (mFlashPageSizeCode)
		{
			case 1:
				retval = 512;
				break;
			case 2:
				retval = 1024;
				break;
			default:
				retval = 0;
				break;
		}

		return retval;
	}

	uint8_t AppFWversionLow()const
	{
		return mAppFWversionLow;
	}

	void AppFWversionLow(uint8_t value)
	{
		mAppFWversionLow = value;
	}

	void FlashPageSizeCodeRaw(uint8_t value)
	{
		mFlashPageSizeCode = value;
	}

	uint8_t AppFWversionHigh()const
	{
		return mAppFWversionHigh;
	}

	void AppFWversionHigh(uint8_t value)
	{
		mAppFWversionHigh = value;
	}

	uint8_t CANdeviceAddr()const
	{
		return mCANdeviceAddr;
	}

	void CANdeviceAddr(uint8_t value)
	{
		mCANdeviceAddr = value;
	}

	uint32_t AppFWstartAddr()const
	{
		return mAppFWstartAddr;
	}

	void AppFWstartAddr(uint32_t value)
	{
		mAppFWstartAddr = value;
	}

	uint32_t AppFWendAddr()const
	{
		return mAppFWendAddr;
	}

	void AppFWendAddr(uint32_t value)
	{
		mAppFWendAddr = value;
	}

private:
	uint8_t mInfoBlockLength;
	uint8_t mMCUcode;
	uint8_t mBLtype;
	uint8_t mFlashPageSizeCode;
	uint8_t mAppFWversionLow;
	uint8_t mAppFWversionHigh;

	uint8_t mCANdeviceAddr;

	uint32_t mAppFWstartAddr;
	uint32_t mAppFWendAddr;

};

class HexImage : public InfoBlock
{
public:

	static const int MaxFlashSize = 128*1024; // 128kB maximum flash size

	enum AppFWimageInfoBlock
	{
		sigByte0 = 0,
		sigByte1,
		sigByte2,
		sigByte3,
		infoBlockLength,
		mCUcode,
		BLtype,
		flashPageSizeCode,
		appFWver0,
		appFWver1,
		EndValue
	};

	enum Source_Info_Rsp {
		SRC_Info_block_Length = 0,
		SRC_MCU_Code,
		SRC_BL_Type,
		SRC_Flash_Page_Size_Code,
		SRC_App_FW_Ver_Low,
		SRC_App_FW_Ver_High,
		SRC_SMBUS_Device_Addr,
		SRC_App_FW_Start_Addr0,
		SRC_App_FW_Start_Addr1,
		SRC_App_FW_Start_Addr2,
		SRC_App_FW_End_Addr0,
		SRC_App_FW_End_Addr1,
		SRC_App_FW_End_Addr2,
		SRC_Enum_End_Value
	};

	enum AppFWimageInfoBlockCAN
	{
		cANdeviceAddr = EndValue
	};

	HexImage(const ats::String& name);
	~HexImage();

	void initHexImage();
	bool openHexFile();
	bool readHexFile();
	void initHexImageInfo();
	void getPageSpecified512(bool (&pageSpecified512)[MaxFlashSize/512])const;
	void getHexImageInfo(uint8_t (&src_info)[SRC_Enum_End_Value])const;
	void ExtractHexImageBytes(uint8_t* outputBuf, int outputBufOffset, uint hexImageStartAddr, int numBytes)const;

	bool hexImageValid() {return m_hexImageValid;}

private:
	ats::String m_filename;

	unsigned char* m_hexImage;
	bool* m_pageSpecified512;
	bool* m_pageSpecified1024;

	bool m_hexImageValid;
	uint32_t m_lowestSpecifiedAddr;
	uint32_t m_highestSpecifiedAddr;
};
