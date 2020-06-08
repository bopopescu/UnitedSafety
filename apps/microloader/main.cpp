#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

#include "ats-common.h"
#include "atslogger.h"
#include "db-monitor.h"
#include "ConfigDB.h"
#include "socket_interface.h"
#include "command_line_parser.h"

#include "microloader.h"

static ATSLogger g_log;
int g_dbg = 0;
static const ats::String g_app_name("microloader");

void print_usage()
{
	fprintf(stderr, "Usage: microloader HexImageFile\n");
}

void MyData::init()
{
	m_hImage = new HexImage(m_filename);
	m_hImage->openHexFile();

	getHexImageInfo();
	getPageSpecified512();

	m_targetWrite = new targetWrite();
	m_targetWrite->open_device();
}

void MyData::deinit()
{
	delete m_hImage;
}

void MyData::getHexImageInfo()
{
	//Since the version register only store one byte version number, so from file AppFWversionHigh and AppFWversionLow,
	//should grap the High number last digit, and low number first digit to create proper version number.
	//For instnace: High is 0x02 and low is 0x10, the version number string should be 0x21, and final real version is 2.1
	ats::String versionStr;
	ats_sprintf(&versionStr, "%.2x%.2x", m_hImage->AppFWversionHigh(), m_hImage->AppFWversionLow());

	m_updateFWVersion = strtol(versionStr.substr(1, 2).c_str(), 0, 16);
	ats_logf(ATSLOG(0),"Update version is %x", m_updateFWVersion);

	m_hImage->getHexImageInfo(m_imageInfo);

	m_currentAddr = m_hImage->AppFWstartAddr();

	m_currentPageIndex = 0;
	m_validPageCount = 0;

	ats::String buf;
	for( int i = 0; i < HexImage::SRC_Enum_End_Value; ++i)
	{
		ats::String s;
		ats_sprintf(&s, "%.2x-", m_imageInfo[i]);
		buf += s;
	}

	ats_logf(ATSLOG(0),"HexImageInfo sent [Hex]: %s\r\n", buf.c_str());
}

void MyData::getPageSpecified512()
{
	m_hImage->getPageSpecified512(m_pageSpecified512);
}

uint32_t MyData::CurrentPageStartAddr()const
{
	return ComputePageAddrFromPageIndex();
}

uint32_t MyData::ComputePageAddrFromPageIndex()const
{
	uint32_t pageStartAddr;

	if (m_hImage->FlashPageSize() == 512)
	{
		pageStartAddr = m_currentPageIndex << 9;
	}
	else
	{
		pageStartAddr = m_currentPageIndex << 10;
	}

	return pageStartAddr;
}

void MyData::getPageInfo()
{

	if (m_hImage->hexImageValid() == true)
	{
		do
		{
			if (m_pageSpecified512[m_currentPageIndex] == true)
			{
				break;
			}
			else
			{
				m_currentPageIndex++;
			}
		}
		while (CurrentPageStartAddr() <= m_hImage->AppFWendAddr());

		if (CurrentPageStartAddr() <= m_hImage->AppFWendAddr())
		{
			mTransmitBuffer[0] = SRC_RSP_OK;
			mTransmitBuffer[1] = (uint8_t)(CurrentPageStartAddr() & 0xFF);
			mTransmitBuffer[2] = (uint8_t)((CurrentPageStartAddr() & 0xFF00) >> 8);
			mTransmitBuffer[3] = (uint8_t)((CurrentPageStartAddr() & 0xFF0000) >> 16);

			m_validPageCount++;
		}
		else
		{
			// No more pages to send
			mTransmitBuffer[0] = SRC_RSP_DATA_END;
			mTransmitBuffer[1] = (uint8_t)((m_validPageCount-1)& 0xFF);
		}
	}
	else
	{
		mTransmitBuffer[0] = SRC_RSP_ERROR;
	}

	ats::String buf;
	for( int i = 0; i < PageInfoSize; ++i)
	{
		ats::String s;
		ats_sprintf(&s, "%.2x-", mTransmitBuffer[i]);
		buf += s;
	}

	ats_logf(ATSLOG(0),"PageInfo sent [Hex]: %s\r\n", buf.c_str());
}

void MyData::getPage()
{

	if (m_hImage->hexImageValid() == true)
	{
		mTransmitBuffer[0] = SRC_RSP_OK;
		m_hImage->ExtractHexImageBytes(mTransmitBuffer, 1, (uint)CurrentPageStartAddr(), (int)m_hImage->FlashPageSize());
		mTransmitBuffer[m_hImage->FlashPageSize() + 1] = SRC_RSP_OK;

		m_currentPageIndex++;
	}
	else
	{
		mTransmitBuffer[0] = SRC_RSP_ERROR;
	}

	ats::String buf;
	for( int i = 0; i < PageSize; ++i)
	{
		ats::String s;
		ats_sprintf(&s, "%.2x-", mTransmitBuffer[i]);
		buf += s;
	}

	ats_logf(ATSLOG(0),"Page sent [Hex]: %s\r\n", buf.c_str());
}

int MyData::process()
{

	int retry = 0;

	for(;;)
	{
		m_targetWrite->TGT_Enter_BL_Mode();

		// AWARE360 FIXME: Why sleep 2 seconds?
		sleep(2);

		const uint8_t TGT_Response = m_targetWrite->TGT_Enter_BL_Mode();

		if(TGT_Response != TGT_RSP_BL_MODE)
		{
			m_targetWrite->TGT_SW_Reset();

			// AWARE360 FIXME: Really need retry 10 times?
			if(retry++ > 10)
			{
				ats_logf(ATSLOG(0),"Fail to enter BL mode: response: %d!", TGT_Response);
				return ERR_TGT_BL_MODE;
			}

		}
		else
		{
			ats_logf(ATSLOG(0), "Enter BL mode response: %x!", TGT_Response);
			break;
		}

	}

	// Now that the last app page has been erased, begin the page-by-page bootload process
	int ret = 0;

	for(;;)
	{
		// Request start data from source
		getPageInfo();
		uint8_t SRC_Response = mTransmitBuffer[0];

		if (SRC_Response != SRC_RSP_OK)
		{
			if(SRC_Response == SRC_RSP_DATA_END)
			{
				ats_logf(ATSLOG(0),"DATA end");
			}
			else
			{
				ret = ERR_SRC_UNEXPECTED_RSP;
				ats_logf(ATSLOG(0),"DATA error");
			}

			break;
		}

		const uint32_t Page_Addr = ((mTransmitBuffer[3] << 16) | (mTransmitBuffer[2] << 8) | (mTransmitBuffer[1]));

		// Request data from source
		getPage();
		SRC_Response = mTransmitBuffer[0];

		if (SRC_Response != SRC_RSP_OK || mTransmitBuffer[PageSize - 1] != SRC_RSP_OK)
		{
			ret = ERR_SRC_UNEXPECTED_RSP;
			break;
		}

		uint8_t Page_Buf[PageSize - 2];

		for(int i = 0; i < PageSize - 2; ++i)
		{
			Page_Buf[i] = mTransmitBuffer[i + 1];
		}

		// Set target page
		// Erase the target application page

		ats_logf(ATSLOG(0),"Erase application page, Address begin: 0x%x", Page_Addr);
		const uint8_t TGT_Response = m_targetWrite->TGT_Erase_Page(Page_Addr);

		if (TGT_Response != TGT_RSP_OK)
		{
			ret = ERR_TGT_UNEXPECTED_RSP;
			ats_logf(ATSLOG(0),"TGT_Erase_Page fail, response: %x", TGT_Response);
			break;
		}

		//New code for writing.
		ats_logf(ATSLOG(0),"Write application page, Address begin: 0x%x", Page_Addr);
		for (int index = 0; index < PAGE_SIZE; index += MAX_BUF_BYTES)
		{
			const uint8_t TGT_Response = m_targetWrite->TGT_Write_Flash(Page_Buf + index,
					Page_Addr + index, MAX_BUF_BYTES);
			if (TGT_Response != TGT_RSP_OK)
			{
				ret = ERR_TGT_UNEXPECTED_RSP;
				ats_logf(ATSLOG(0),"TGT_Write_Flash fail, response: %x", TGT_Response);
				break;
			}

		}

	}

	// Reset target
	m_targetWrite->TGT_SW_Reset();

	// AWARE360 FIXME: Why sleep 1 second?
	sleep(1);

	return ret;
}

int MyData::readTarget(const int command)const
{
	return m_targetWrite->i2c_read(command);
}

// Description: Watchdog to detect firmware programming deadlock, or long delay.
//
//	XXX: The parent process must kill this watchdog before the timeout expires
//	     (killing of the watchdog is how the parent proves that it finished
//	     the programming task in time). It is not sufficient for the parent
//	     process to finish programming micro firmware and not kill the watchdog,
//	     it must kill this watchdog.
//
//	As long as the watchdog is running, if the parent programming task times out,
//	then the parent will be sent SIGUSR1, then SIGTERM after max_signal_response_seconds,
//	and finally SIGKILL after a final max_signal_response_seconds.
static void watchdog(int p_parent_pid, int p_fd)
{
	const int max_firmware_program_seconds = 10;
	const int sigbuf[] = {SIGUSR1, SIGTERM, SIGKILL};
	const int* sig = sigbuf;

	struct timeval tv;
	tv.tv_sec = max_firmware_program_seconds;
	tv.tv_usec = 0;

	for(;;)
	{
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(p_fd, &rfds);

		const int retval = select(p_fd + 1, &rfds, NULL, NULL, &tv);

		if(-1 == retval)
		{
			exit(2); // Select failed
		}
		else if(retval)
		{
			exit(0); // Parent process has exited
		}

		kill(p_parent_pid, *sig);

		if(SIGKILL == (*sig))
		{
			break;
		}

		++sig;
		const int max_signal_response_seconds = 2;
		tv.tv_sec = max_signal_response_seconds;
		tv.tv_usec = 0;
	}

	exit(1);
}

static void watchdog_timeout_signal(int p_sig)
{
	ats_logf(ATSLOG(0), "Firmware programming timeout");
	exit(1);
}

int main(int argc, char* argv[])
{

	if(argc < 2)
	{
		print_usage();
		return 1;
	}

	signal(SIGUSR1, watchdog_timeout_signal);
	int parent_link[2];
	pipe(parent_link);
	const int parent_pid = getpid();
	const int pid = fork();

	if(!pid)
	{
		close(parent_link[1]);
		watchdog(parent_pid, parent_link[0]);
	}

	close(parent_link[0]);

	g_log.set_global_logger(&g_log);
	g_log.set_level(g_dbg);
	g_log.open_testdata(g_app_name);

	const ats::String filename(argv[1]);
	MyData md(filename);

	const uint8_t verfromfile = md.getUpdateFWVersion();
	int verfromreg = md.readTarget(0x11);

	if( (uint8_t)verfromreg >= verfromfile )
	{
		ats_logf(&g_log, "No need to Update, Current version: 0x%.2x is equal or greater than update version: 0x%.2x\n", verfromreg, verfromfile);
		return 0;
	}

	const int ret = md.process();

	if(ret)
	{
		printf("Update fail, error code %d\n", ret);
		return 1;
	}

	verfromreg = md.readTarget(0x11);

	if(verfromreg < 0 || (uint8_t)verfromreg != verfromfile )
	{
		printf("Update fail, fail to get version register or not match with version from file\n");
		return 1;
	}

	kill(pid, SIGKILL);
	waitpid(pid, 0, 0);

	ats::String version;
	ats_sprintf(&version, "%x", verfromreg);
	printf("Update success with version %s.%s\n", version.substr(0, (version.size() - 1)).c_str(), version.substr(version.size() - 1).c_str());
	return 0;
}
