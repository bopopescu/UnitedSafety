#pragma once

#include "state-machine-data.h"

#include "heximage.h"
#include "targetwrite.h"

class MyData : public StateMachineData
{

public:
static const uint8_t SRC_RSP_OK = 0x70;
static const uint8_t SRC_RSP_ERROR = 0x71;
static const uint8_t SRC_RSP_DATA_END = 0x72;
static const uint8_t SRC_RSP_UNKNOWN_CMD = 0x73;
static const int TransmitBufferSize = 514;
static const int PageInfoSize = 4;
static const int PageSize = 514;

	MyData(const ats::String& filename):
		m_filename(filename),
		m_currentAddr(0),
		m_updateFWVersion(0),
		m_validPageCount(0),
		m_currentPageIndex(0)
	{
		init();
	}

	~MyData()
	{
		deinit();
	}

	void init();
	void deinit();

	int process();

	void getPageSpecified512();
	void getHexImageInfo();
	void getPageInfo();
	void getPage();

	uint32_t CurrentPageStartAddr()const;
	uint32_t ComputePageAddrFromPageIndex()const;

	int readTarget(const int command)const;
	uint8_t getUpdateFWVersion()const { return m_updateFWVersion;}

private:
	HexImage* m_hImage;
	targetWrite* m_targetWrite;
	ats::String m_filename;

	uint32_t m_currentAddr;
	uint8_t m_updateFWVersion;

	int m_validPageCount;
	int m_currentPageIndex;

	uint8_t m_imageInfo[HexImage::SRC_Enum_End_Value];
	bool m_pageSpecified512[HexImage::MaxFlashSize/512];
	uint8_t mTransmitBuffer[TransmitBufferSize];
};
