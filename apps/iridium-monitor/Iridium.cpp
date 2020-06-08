#include <iomanip>
#include <cstdlib>
#include <math.h>
#include <time.h>
#include <vector>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>

#include <boost/array.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/foreach.hpp>

#include <CSelect.h>
#include <AFS_Timer.h>
#include "ats-common.h"
#include "db-monitor.h"
#include "ConfigDB.h"
#include "socket_interface.h"
#include "utility.h"
#include "Iridium.h"

#define IRIDIUM_FW_VERSION 8
#define IRIDIUM_MAX_FAIL_COUNT 5 //!\todo decide the max value


pthread_mutex_t IRIDIUM::m_cmdBufferMutex;
pthread_mutex_t IRIDIUM::m_respBufferMutex;
pthread_mutex_t IRIDIUM::m_statusMutex;
int IRIDIUM::m_cmd_pipe[2];
IRIDIUM::STATUS_CODES IRIDIUM::m_status = IRIDIUM::NOT_READY;
ats::StringList IRIDIUM::m_ResponseBuf; 
extern ATSLogger g_log;

bool bDownGrading;  // indicates that the signal has gotten worse and we are waiting on the timer to change the LEDS (JIRA- TRUL-35)
AFS_Timer downGradeTimer;
int lastLevel, curLevel; // Level is 0 - no network or rssi = 0, 1 - rssi <= 2, 2 rssi > 2

bool IRIDIUM::bTxInProgress = false; //<ISCP-238>

static bool g_bSentInetNotification = false;
static int failCount = 0;
static int e_enabled = 0;

IRIDIUM::IRIDIUM()
{
	m_bNetworkAvailable = false;
	m_rssi = 0;
	pthread_mutex_init(&m_statusMutex, 0);
}

void IRIDIUM::init()
{

	pthread_mutex_init(&m_cmdBufferMutex, 0);
	pthread_mutex_init(&m_respBufferMutex, 0);
	ats::system("stty -F /dev/ttySP4 -echo raw 19200");

	if (pipe(m_cmd_pipe) == -1)
	{
		ats_logf(ATSLOG_DEBUG, "%s:%d - Unable to create pipe. pipe returned -1, errno %d", __FILE__, __LINE__,errno);
		exit(-1);
	}

	// create the reader thread to read the port and load the buffer.
	pthread_create(&(m_reader_thread),	(pthread_attr_t*)0, reader_thread,	this);
	IRIDIUM::STATUS_CODES status;
	

		// <ISCP-163>
		db_monitor::ConfigDB db;
		e_enabled = db.GetInt("iridium-monitor", "ErrorEnabled", 0);   
		

		if(e_enabled == 1)
		{

			ats_logf(ATSLOG_DEBUG, "IridiumInit() 1 ");
		}
		else
		{
			ats_logf(ATSLOG_DEBUG, "IridiumInit() 0 ");
			db.Set("iridium-monitor", "IridiumFailCount", ats::toStr(0));
			db.Set("iridium-monitor", "ErrorNotified", ats::toStr(0));
		}
		
		db.Set("iridium-monitor", "ErrorEnabled", ats::toStr(0));
		failCount = db.GetInt("iridium-monitor", "IridiumFailCount", 0);  // 
		g_bSentInetNotification = db.GetInt("iridium-monitor", "ErrorNotified", 0);  // 
		
		ats_logf(ATSLOG_DEBUG, "IridiumInit() %d = %d - %d\r",failCount,e_enabled,g_bSentInetNotification);
		// <\ISCP-163>

	// we now go into an infinite loop until the irdium module is set up.
	while (1)
	{
		for (int i= 0 ; i< 3; i++)
		{
			status = Setup();
			LockStatus();
			m_status = status;
			UnlockStatus();

			if(m_status == PASSED)
			{
				//<ISCP-163>
				g_bSentInetNotification = false; // reset notification send and fail count if Iridium Modem is responding
				failCount = 0;
				db.Set("iridium-monitor", "ErrorEnabled", ats::toStr(0));
				db.Set("iridium-monitor", "ErrorNotified", ats::toStr(0));
				//<\ISCP-163>
				return;
			}

			SendRespond("AT*F\r");
			SetPower(false);
			sleep(2); //sleep 2 seconds as per the document "IRidium - 9603 - DeveloperGuide - V.9 - Preliminary_DEVGUIDE_May2012.pdf"
		}

		//<ISCP-163>
		if (!g_bSentInetNotification && failCount >= IRIDIUM_MAX_FAIL_COUNT)
		{
			ats_logf(ATSLOG_DEBUG, "Iridium sending error message\r");
			g_bSentInetNotification = true;
			db.Set("iridium-monitor", "ErrorNotified", ats::toStr(1));
			AFS_Timer t;
			t.SetTime();
			std::string user_data = "1020," + t.GetTimestampWithOS() + ", Iridium Module Not Responding Error";
			user_data = ats::to_hex(user_data);

			send_redstone_ud_msg("message-assembler", 0, "msg inet_error msg_priority=9 usr_msg_data=%s\r", user_data.c_str());
		}
		else if(!g_bSentInetNotification)
		{

		send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=iridium-monitor.g led=sat script=\"0,1000000\" \r");  
		send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=iridium-monitor.r led=sat.r script=\"1,1000000\" \r");  
			ats_logf(ATSLOG_ERROR, "ERROR - Iridium initialization failure.  Waiting 30 seconds to try again [%d]",failCount);
		
		}//<\ISCP-163>
		sleep(30);
	}

	LockStatus();
	m_status = FAILED;
	UnlockStatus();
}

//----------------------------------------------------------------------------------------------------------------------
IRIDIUM::~IRIDIUM()
{
	pthread_cancel(m_reader_thread);
}


void IRIDIUM::SetPower(bool state)
{
	if (state)
	{
		ats::write_file("/dev/set-gpio", "R");
		ats_logf(ATSLOG_DEBUG, "%s:%d - Turning Iridium module ON through set-gpio",__FILE__, __LINE__);
	}
	else
	{
		ats::write_file("/dev/set-gpio", "r");
		ats_logf(ATSLOG_DEBUG, "%s:%d - Turning Iridium module OFF through set-gpio", __FILE__, __LINE__);
	}
}


//----------------------------------------------------------------------------------------------------------------------
IRIDIUM::STATUS_CODES IRIDIUM::Setup()
{
	RESPONSE_CODES ret;

	SetPower(true);
	sleep(2); //wait for Iridium module to boot up

	ats_logf(ATSLOG_DEBUG, "[%d] - Iridium FW Version\r", IRIDIUM_FW_VERSION);


	if ((ret = SendRespond("ATE0\r")) != OK) // turn echo off
	{
		ats_logf(ATSLOG_DEBUG, "%s:%d - Unable to turn echo off (returned %d)", __FILE__, __LINE__,short(ret) );
		return NOT_READY;
	}

	if ((ret = SendRespond("AT&K0\r")) != OK) // turn flow control off
	{
		ats_logf(ATSLOG_DEBUG, "%s:%d - Unable to turn off flow control (returned %d)", __FILE__, __LINE__,short(ret) );
		return NOT_READY;
	}

	if ((ret = SendRespond("AT&D0\r")) != OK) // turn DTR off
	{
		ats_logf(ATSLOG_DEBUG, "%s:%d - Unable to turn off DTR (returned %d)", __FILE__, __LINE__,short(ret) );
		return NOT_READY;
	}

	if ((ret = SendRespond("AT*R1\r")) != OK) // turn Radio On
	{
		ats_logf(ATSLOG_DEBUG, "%s:%d - Unable to turn Radio On (returned %d)", __FILE__, __LINE__,short(ret) );
		return NOT_READY;
	}

	if ((ret = SendRespond("AT+SBDMTA=1\r")) != OK)
	{
		ats_logf(ATSLOG_DEBUG, "%s:%d - Unable to enable SBDRING (returned %d)", __FILE__, __LINE__, short(ret) );
		return NOT_READY;
	}

	if ((ret = SendRespond("AT+SBDAREG=1\r")) != OK)
	{
		ats_logf(ATSLOG_DEBUG, "%s:%d - Unable to set Automatic Registration (returned %d.", __FILE__, __LINE__, short (ret));
		return NOT_READY;
	}

	ats::String imei = ReadImei();
	if (imei.empty())
	{
		ats_logf(ATSLOG_DEBUG, "%s:%d - Could not read Iridium IMEI.",__FILE__, __LINE__);
		return NOT_READY;
	}

	db_monitor::ConfigDB db;
	db.Set("Iridium", "IMEI", imei);
	m_strIMEI = db.GetValue("RedStone", "IMEI");
	SendATCommand( "AT+CIER=1,1,1\r");  // send status of network connection whenever it changes
	return PASSED;
}

ats::String h_rtrimATResponse(const ats::String& s)
{
	if(!s.empty())
	{
		uint i = 0;
		for(i=s.length();i > 0; i--)
		{
			if(!isspace(s[i-1]))
			{
				break;
			}
		}
		return s.substr(0,i);
	}
	return ats::String();
}
//----------------------------------------------------------------------------------------------------------------------
// Sends the buffer out and expects a response of OK (or ERROR) almost immediately
// There is a  1 second timeout on the response to the command(s)
// Parameters:
//   buf - a single command or a list of commands to be sent to the Iridium AT Port
// Returns the response code of the command(s), or ERROR on timeout.
IRIDIUM::RESPONSE_CODES IRIDIUM::SendRespond(const ats::String buf)
{
	AFS_Timer timer;

	LockResponseBuf();
	m_ResponseBuf.clear();
	UnlockResponseBuf();
	LockCmdBuf();
	ats_logf(ATSLOG_DEBUG, "%s:%d - Send TO IRIDIUM: %s", __FILE__, __LINE__, buf.c_str() );
	SendATCommand(buf);
	timer.SetTime();

	while (timer.DiffTimeMS() < 1000)
	{
		ats::String line = h_rtrimATResponse(GetLineFromResponseBuf());
		if (!line.empty())
		{
//			ats_logf(ATSLOG_DEBUG, "Response from Send was: %s", line.c_str() );
			IRIDIUM::RESPONSE_CODES ret;
			if (strncmp(line.c_str(), "OK", 2) == 0)
			{
				ret = OK;
			}
			else if (strncmp(line.c_str(), "ERROR", 5) == 0)
				ret = ERROR;
			else if (strncmp(line.c_str(), "READY", 5) == 0)
				ret = READY;
			else if (strncmp(line.c_str(), "0", 1) == 0)
				ret = ZERO;
			else if (strncmp(line.c_str(), "1", 1) == 0)
				ret = ONE;
			else if (strncmp(line.c_str(), "2", 1) == 0)
				ret = TWO;
			else if (strncmp(line.c_str(), "3", 1) == 0)
				ret = THREE;
			else if (line[0] == '\r' || line[0] == '\n')
				continue;
			else
			{
				ats_logf(ATSLOG_DEBUG, "%s:%d - Rx from Iridium: ##%s##", __FILE__, __LINE__, line.c_str() );
				continue;
			}
			UnlockCmdBuf();
			ats_logf(ATSLOG_DEBUG, "%s:%d - SendRespond Response is %s. Returning with code %d", __FILE__, __LINE__, line.c_str(), ret );
			return ret;
		}
		usleep(10000);
	}
	UnlockCmdBuf();
	return NONE;
}

bool IRIDIUM::WaitForResponse(const ats::String buf, const char *expected)
{
	AFS_Timer timer;

	LockResponseBuf();
	m_ResponseBuf.clear();
	UnlockResponseBuf();

	LockCmdBuf();
	SendATCommand(buf);
	timer.SetTime();

	while (timer.DiffTimeMS() < 1000)
	{
		ats::String line = GetLineFromResponseBuf();
		if (!line.empty())  // a complete line was found
		{
			if (line.find(expected) != std::string::npos)
			{
				UnlockCmdBuf();
				return true;
			}
		}
	}
	UnlockCmdBuf();
	ats_logf(ATSLOG_DEBUG, "WaitForResponse did NOT get what was expected (%s)",expected);
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
// Prepare to send the msg out
//
bool IRIDIUM::PrepareToSendMessage(char *imsg, short len)
{
	ats::String buf;

	ats_sprintf(&buf, "AT+SBDWB=%d\r", len);

	LockResponseBuf();
	m_ResponseBuf.clear();
	UnlockResponseBuf();

	if (WaitForResponse(buf, "READY"))
	{
		bool ret =  SendSBD(imsg, len + 2);
		return ret;
	}

	// we have failed if we get here.

	ats_logf(ATSLOG_DEBUG, "SBDWB did not return READY");
	return false;
}

int IRIDIUM::ManualNetworkRegistration()
{
//	ats_logf(ATSLOG_DEBUG, "Starting manual network registration.");
	SendATCommand("AT+SBDREG\r");
	return 0;
}


//----------------------------------------------------------------------------------------------------------------------
// SendSBD - sends the data and then looks for confirmation
// 0 is good
// 2 is bad checksum
bool IRIDIUM::SendSBD(const char *buf, short len)
{
	AFS_Timer timer;

	LockResponseBuf();
	m_ResponseBuf.clear();
	UnlockResponseBuf();

//	ats_logf(ATSLOG_DEBUG, "Sending %d bytes", len);

	LockCmdBuf();
	ats::ignore_return <ssize_t>(write(m_cmd_pipe[1], buf, len));

	timer.SetTime();
	while (timer.DiffTimeMS() < 10000)
	{
		ats::String line = GetLineFromResponseBuf();
		if (!line.empty())  // a complete line was found
		{
//			ats_logf(ATSLOG_DEBUG, "SendSBD got line: ##%s", line.c_str());
			const char* resp = line.c_str();
			switch (resp[0])
			{
			case '0':
			{
				// Have the iridium LED blink 4 times to indicate a send.
				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink kick name=iridium_sending led=sat script=\"0,100000;1,100000\" priority=5 timeout=800000\r");
				UnlockCmdBuf();
				return true;
				break;
			}
			case '1':
			{
				ats_logf(ATSLOG_DEBUG, "SendSBD::Not enough bytes sent");
				break;
			}
			case '2':
			{
				ats_logf(ATSLOG_DEBUG, "SendSBD::Data rejected - bad checksum");
				return false;
				break;
			}
			case '3':
			{
				ats_logf(ATSLOG_DEBUG, "SendSBD::Invalid data size");
				break;
			}
				//         case 'O':  // assume that this is OK
				//				   if (resp[1] == 'K')
				//             return ret;
			default:
				continue;
			}
		}
		usleep(10000);
	}
	UnlockCmdBuf();
	ats_logf(ATSLOG_DEBUG, "SendSBD timed out.");
	return false;
}

ats::String IRIDIUM::GetLineFromResponseBuf()
{
	ats::String line;
	LockResponseBuf();
	if(!m_ResponseBuf.empty())
	{
		line = m_ResponseBuf.front();
		m_ResponseBuf.erase(m_ResponseBuf.begin());
	}
	UnlockResponseBuf();
	return line;
}


#define IRIDIUM_MAX_CHAR_READ 256
//----------------------------------------------------------------------------------------------------------------------
void* IRIDIUM::reader_thread(void* p_iridium)
{
	CharBuf InputBuf;
	InputBuf.SetSize(1024);
	IRIDIUM& iridium = *((IRIDIUM*)p_iridium);
	CSelect irdSelect;
	AFS_Timer noCharsTimer;
	db_monitor::ConfigDB db;

	int fdr = open(IRIDIUM_PORT, O_RDONLY| O_NOCTTY);
	if (fdr < 0)
	{
		ats_logf(ATSLOG_DEBUG, "Unable to open Iridium port: open returned (%d), errno %d", fdr, errno);
	}
	int fdw = open(IRIDIUM_PORT, O_WRONLY | O_NOCTTY);
	if (fdw < 0)
	{
		ats_logf(ATSLOG_DEBUG, "Unable to open Iridium port: open returned (%d), errno %d", fdw, errno);
	}

	irdSelect.SetDelay(0, 500000);
	irdSelect.Add(fdr);
	irdSelect.Add(iridium.m_cmd_pipe[0]);

	while (1)
	{
		irdSelect.Select();
		
		if(irdSelect.HasData(fdr))
		{
			char readBuf[IRIDIUM_MAX_CHAR_READ];
			memset(readBuf, 0, sizeof(char));
			int len = read(fdr, readBuf, IRIDIUM_MAX_CHAR_READ);
			if(len > 0)
			{
				InputBuf.Add(readBuf, len);
				IRIDIUM::ReadATResponses(&iridium, InputBuf, fdr, fdw);
				noCharsTimer.SetTime();  // we are talking to the Iridium unit
			}
			else
			{
				usleep(1000);
			}
		}

		if(irdSelect.HasData(iridium.m_cmd_pipe[0]))
		{
			char writeBuf[IRIDIUM_MAX_CHAR_READ];
			memset(writeBuf, 0, sizeof(char));
			int len = read(iridium.m_cmd_pipe[0],writeBuf,IRIDIUM_MAX_CHAR_READ);
			if(len > 0)
			{
				ats::ignore_return <ssize_t>(write(fdw, writeBuf, len));
			}
		}
		
		// downgraded signals must stay downgraded for 10 seconds before changing the LED.
		if (bDownGrading && downGradeTimer.DiffTimeMS() > 10000)  
		{
			bDownGrading = false;
			lastLevel = curLevel;
			if (curLevel == 0)
			{
				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=iridium-monitor.g led=sat script=\"0,1000000\"\r"); 
				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=iridium-monitor.r led=sat.r script=\"0,250000;1,750000\"\r"); 
			}
			else if (curLevel == 1)
			{
				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=iridium-monitor.g led=sat script=\"1,750000;0,250000\" \r");  
				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=iridium-monitor.r led=sat.r script=\"0,1000000\" \r");  
			}
		}
		
		// now test that we have at least something every 10 second.  If not send an AT command.  If we don't get a response we may have lost the port
		if (noCharsTimer.DiffTime() == 15)
			SendATCommand( "AT\r"); 	 // send status of network connection whenever it changes

		if (noCharsTimer.DiffTime() > 29)
		{
			ats_logf(ATSLOG_DEBUG, "%s:%d - No chars for 30 seconds.  Looking for AT", __FILE__, __LINE__);
			RESPONSE_CODES ret;
			sleep(5);
			if ((ret = SendRespond("AT\r")) == NONE) // if we don't get an OK back we need to restart irdium-monitor
			{
				sleep(5);  // wait then try again.
				if ((ret = SendRespond("AT\r")) == NONE) // if we don't get an OK back we need to restart irdium-monitor
				{
					//<ISCP-163>
					if(failCount < IRIDIUM_MAX_FAIL_COUNT)
					{
						failCount++;	
						db.Set("iridium-monitor", "IridiumFailCount", ats::toStr(failCount));
					}
					if(e_enabled == 0)
					{
						db.Set("iridium-monitor", "ErrorEnabled", ats::toStr(1));	
					}
					//<\ISCP-163>

					ats_logf(ATSLOG_DEBUG, "%s:%d:%d - No chars on port - exiting", __FILE__, __LINE__,failCount);
					send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=iridium-monitor.g led=sat script=\"0,1000000\" \r");  
	 				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=iridium-monitor.r led=sat.r script=\"1,1000000\" \r");  
	 				system("killall iridium-monitor&");  // only way to kill it!
	 				return 0;
				}
				else
					noCharsTimer.SetTime();  // we are talking to the Iridium unit
			}
			else
				noCharsTimer.SetTime();  // we are talking to the Iridium unit

		}
	}
	return 0;
}


bool IRIDIUM::ProcessSBDIXResponse(const ats::String& response, SBD_Status &status)
{
	uint pos=0;
	uint end_pos=0;
	ats::String trimmed_resp = h_rtrimATResponse(response);
	if((pos = trimmed_resp.find("+SBDIX")) == ats::String::npos)
	{
		return false;
	}
	if((pos = trimmed_resp.find(":")) == ats::String::npos)
	{
		return false;
	}
//	ats_logf(ATSLOG_DEBUG, "%s,%d:Processing SBDIX response. %s", __FILE__, __LINE__, trimmed_resp.c_str());
	pos += 1;
	//Read MO status
	if((end_pos = trimmed_resp.find(',', pos)) == ats::String::npos)
	{
		return false;
	}
	if(pos == end_pos)
	{
		status.mo_status = 0;
	}
	else
	{
		status.mo_status = (unsigned char)strtol(trimmed_resp.substr(pos,end_pos-1).c_str(),0, 10);
	}
	pos = end_pos + 1;
	//Read MOMSN
	if((end_pos = trimmed_resp.find(',', pos)) == ats::String::npos)
	{
		return false;
	}
	if(pos == end_pos)
	{
		status.momsn = 0;
	}
	else
	{
		status.momsn = (ushort)strtol(trimmed_resp.substr(pos,end_pos - 1).c_str(), 0 , 10);
	}
	pos = end_pos + 1;
	//Read MT status
	if((end_pos = trimmed_resp.find(',', pos)) == ats::String::npos)
	{
		return false;
	}
	if(pos == end_pos)
	{
			status.mt_status = 0;
	}
	else
	{
		status.mt_status = (ushort)strtol(trimmed_resp.substr(pos,end_pos - 1).c_str(), 0, 10);
	}
	pos = end_pos + 1;
	//Read MTMSN
	if((end_pos = trimmed_resp.find(',', pos)) == ats::String::npos)
	{
		return false;
	}
	if(pos == end_pos)
	{
		status.mtmsn = 0;
	}
	else
	{
		status.mtmsn = (ushort)strtol(trimmed_resp.substr(pos,end_pos - 1).c_str(), 0, 10);
	}
	pos = end_pos + 1;
	//Read MT Length
	if((end_pos = trimmed_resp.find(',', pos)) == ats::String::npos)
	{
		return false;
	}
	if(pos == end_pos)
	{
		status.mt_length = 0;
	}
	else
	{
		status.mt_length = (uint)strtol(trimmed_resp.substr(pos,end_pos - 1).c_str(), 0, 10);
	}
	pos = end_pos + 1;
	//Read MT queued
	if(pos > (trimmed_resp.length() - 1))
	{
		status.mt_queued = 0;
		return true;
	}
	status.mt_queued = strtol(trimmed_resp.substr(pos).c_str(), 0, 10);
	ats_logf(ATSLOG_DEBUG, "%s,%d: MO Status:%d MOMSN:%d MT Status:%d MTMSN:%d MT Length:%d MT Queued:%d",
			 __FILE__, __LINE__, status.mo_status, status.momsn, status.mt_status, status.mtmsn, status.mt_length, status.mt_queued);
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
// GetResponse - get response
// Author : HAMZA MEHBOOB
void IRIDIUM::GetResponse(char * p_msgBuf)	
{
	for(int i =0; i <40; i++)
	{
		p_msgBuf[i] = m_MessageRecieved[i];
	}	
	for(int i =0; i <40; i++)
	{
		m_MessageRecieved[i] = 0;
	}	 // ISCP-339, old response from translator needs to be cleared to update cloud icon/iNet status
}

//----------------------------------------------------------------------------------------------------------------------
// ProcessIridiumResponse - copy response buffer 
// Author : Hamza Mehboob
//void IRIDIUM::ProcessIridiumResponse(std::vector<char>&  iridiumResponse)
void IRIDIUM::ProcessIridiumResponse(std::string iridiumResponse)
{
	if((iridiumResponse.find("02")!= std::string::npos) || (iridiumResponse.find("05")!= std::string::npos))
	{
		iridiumResponse.copy(m_MessageRecieved,40,0);
	}

}
//298


bool IRIDIUM::ProcessMTMessage(std::vector<char>& buf)
{

		std::stringstream ostream;
		//ostream << "RECV["<<buf.size()<< "]: ";
		for (uint i= 0 ; i < buf.size(); ++i)
		{
			ostream << std::uppercase <<std::setfill('0') << std::setw(2)
					<< std::hex << (int)(buf[i])<< " ";
		}
		ats_logf(ATSLOG_DEBUG, "%s:%d - Incoming MTMessage: %s", __FILE__, __LINE__, ostream.str().c_str());
		// ISCP-298
		ProcessIridiumResponse(ostream.str());

		return true; 
}

//wrapper to write command, saves calculating length of string
ssize_t h_writeString(int p_fdw, const ats::String& p_cmd)
{
	return write(p_fdw, p_cmd.c_str(), p_cmd.length());
}


//============================================================================================================
int IRIDIUM::ReadATResponses(IRIDIUM* p_iridium, CharBuf& buf, int p_fdr, int p_fdw)
{
	IRIDIUM& iridium = *p_iridium;
	IRIDIUM::SBD_Status status;
	char strResponse[IRIDIUM_MAX_CHAR_READ];
	

	while (1)
	{
		memset(strResponse, 0, IRIDIUM_MAX_CHAR_READ);
		int lineLen = buf.GetLine(strResponse, IRIDIUM_MAX_CHAR_READ, '\n');
		if(lineLen < -1)
		{
			ats_logf(ATSLOG_DEBUG, "CharBuf GetLine, error finding EOL, error code:%d", lineLen);
			buf.Reset();
			return -1;
		}
		else if (lineLen <= 0 )
		{
			break; //No EOL detected
		}
//		ats_logf(ATSLOG_DEBUG, "@@%s@@)", strResponse);
		ats::String line = strResponse;
		// now look for asynchronous strings in the data
		// CIEV is in 2 forms:
		//  +CIEV,1,x - x=0:network unavailable or 1:network available  
		//  _CIEV,0,x - x= signal strength
		uint i;
		if ((i = line.find("+CIEV:")) != std::string::npos)
		{
			uint sig_offset = i + 6;
			uint value_offset = i + 8;

			if (strResponse[sig_offset] == '1')
			{
				if (strResponse[value_offset] == '1')  // network connection established
				{
					if (iridium.m_bNetworkAvailable == false)
					{
						iridium.m_bNetworkAvailable = true;
						ats_logf(ATSLOG_DEBUG, "Network available - %d bars  (Actual string ##%s##)", iridium.m_rssi, StripCRLF(strResponse));

						// now set the LED - flash green if rssi <= 2 Solid green if > 2
						if (iridium.m_rssi <= 2)
						{
							if (lastLevel < 2) // are we upgrading the signal - then we change blink immediately
							{
								send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=iridium-monitor.g led=sat script=\"1,750000;0,250000\" \r");  
			 					send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=iridium-monitor.r led=sat.r script=\"0,1000000\" \r");  
								bDownGrading = false;
								lastLevel = curLevel = 1;
			 				}
			 				else if (!bDownGrading) // downgrading - start timer
			 				{
				 				downGradeTimer.SetTime();
				 				bDownGrading = true;
				 				curLevel = 1;
			 				}
		 				}
		 				else // good signal - change blink immediately
		 				{
							send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=iridium-monitor.g led=sat script=\"1,1000000\" \r");  
		 					send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=iridium-monitor.r led=sat.r script=\"0,1000000\" \r");  
							bDownGrading = false;
							lastLevel = curLevel = 2;
		 				}
					}
				}
				else  // network connection lost  - wait 15 seconds before changing LEDs
				{
					if (iridium.m_bNetworkAvailable == true)
					{
						if (!bDownGrading)  // don't reset the downgrade timer if we are already downgrading.
						{
			 				downGradeTimer.SetTime();
			 				bDownGrading = true;
			 			}
		 				curLevel = 0;
						iridium.m_bNetworkAvailable = false;
						ats_logf(ATSLOG_DEBUG, "Network unavailable (Actual string ##%s##)", StripCRLF(strResponse));
					}
				}
			}

			if (strResponse[sig_offset] == '0')  // signal strength indicator
			{
				iridium.m_rssi =  strResponse[value_offset] - '0';
				ats_logf(ATSLOG_DEBUG, "Network signal strength %d (Actual string ##%s##)", iridium.m_rssi, StripCRLF(strResponse));
				// now set the LED - flash green if rssi <= 2 Solid green if > 2
				if (iridium.m_rssi == 0)
				{
					if (lastLevel > 0 && !bDownGrading)
					{
		 				downGradeTimer.SetTime();
		 				bDownGrading = true;
		 			}
	 				curLevel = 0;
				}
				else if (iridium.m_rssi <= 2)
				{
	 				curLevel = 1;

 					if (lastLevel > 1 && !bDownGrading)
					{
		 				downGradeTimer.SetTime();
		 				bDownGrading = true;
		 			}
		 			else
					{
						send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=iridium-monitor.g led=sat script=\"1,750000;0,250000\" \r");  
 						send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=iridium-monitor.r led=sat.r script=\"0,1000000\" \r");  
 					}
 				}
 				else // good signal - LED to solid green
 				{
					send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=iridium-monitor.g led=sat script=\"1,1000000\" \r");  
 					send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=iridium-monitor.r led=sat.r script=\"0,1000000\" \r");  
 				}
			}
		}
		else if ((i = line.find("SBDRING")) != std::string::npos && (bTxInProgress == false)) //<ISCP-238>
		{
			ats_logf(ATSLOG_DEBUG, "Incoming message detected: Somebody wants to talk to us!\r\n");
			send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=iridium-monitor led=sat script=\"0:100000;1:100000;0:100000;1:100000;0:100000;1:100000;0:100000;1,1000000\"\r");  // somebody is sending us s
			h_writeString(p_fdw, "\rAT+SBDIXA\r");
		}
		else if (iridium.ProcessSBDIXResponse(line, status))
		{
			if((status.mt_status == 1) && (status.mt_length > 0))
			{
				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=iridium-monitor led=sat script=\"1,1000000\"\r");
				ats_logf(ATSLOG_DEBUG, "Incoming Message has been received");

				std::vector<char> MTMessageBuf;
				h_writeString(p_fdw, "\rAT+SBDRB\r");
				for(uint bytes_read = 0;bytes_read < (status.mt_length + 4);)
				{
					unsigned char c;
					int ret = read(p_fdr, &c, 1);
					if(ret > 0)
					{
						MTMessageBuf.push_back(c);
						bytes_read++;
					}
				}
				//process MT Message
				iridium.ProcessMTMessage(MTMessageBuf);
				if(status.mt_queued > 0)
				{
					h_writeString(p_fdw,"\rAT +SBDD2 +SBDIX\r");
				}
			}

			iridium.LockResponseBuf();
			iridium.m_ResponseBuf.push_back(line);
			iridium.UnlockResponseBuf();

		}
		else if ((i = line.find("HARDWARE FAILURE:")) != std::string::npos)
		{
			ats_logf(ATSLOG_DEBUG, "Critical ERROR - HARDWARE FAILURE detected :%s\r\n",line.c_str());
		}
		else
		{
			iridium.LockResponseBuf();
			iridium.m_ResponseBuf.push_back(line);
			iridium.UnlockResponseBuf();
		}
	}
	return 0;
}

void IRIDIUM::SendATCommand(const ats::String buf)
{
//	ats_logf(ATSLOG_DEBUG,"Writing command to pipe: %s", buf.c_str());
	ats::ignore_return <ssize_t>(write(m_cmd_pipe[1], buf.c_str(), buf.length()));
}

//----------------------------------------------------------------------------------------------------------------------
// ComputeCheckSum: sum up the bytes as a short and adds it to the end of msg
//
unsigned short IRIDIUM::ComputeCheckSum(char *msg, short len)
{
	unsigned short val = 0;

	for (short i = 0; i < len; i++)
		val += short(msg[i]);

	msg[len] = (char)((val >> 8) & 0xFF);
	msg[len + 1] = (char)(val & 0xFF);
	msg[len + 2] = '\0';
	return val;
}

unsigned short IRIDIUM::ComputeCheckSum(std::vector<char> &msg)
{
	unsigned short val = 0;

	for (size_t i = 0; i < msg.size(); i++)
		val += short(msg[i]);

	msg.push_back((char)((val >> 8) & 0xFF));
	msg.push_back((char)(val & 0xFF));

	return val;
}

bool IRIDIUM::ClearMessageBuffer()
{
	return WaitForResponse("AT+SBDD0\r", "0");
}

int IRIDIUM::SendMessage()
{
	LockCmdBuf();
	SendATCommand("AT+SBDIX\r");
	AFS_Timer timer;
	timer.SetTime();
	while (timer.DiffTime() < 60)  // 60 second timeout - should never hit this timeout
		// but it exists to avoid deadlock in the
		// event of a communication issue with the Iridium module.
	{
		ats::String line = GetLineFromResponseBuf();
		uint pos;
		if((pos = line.find("+SBDIX:")) != std::string::npos)
		{
//			ats_logf(ATSLOG_DEBUG, "%s, %d: Response:\r\n%s\n--End of String--",__FILE__, __LINE__, line.c_str());
			ats::String resp;
			const char* line_ptr = line.c_str();
			pos += 7;
			for (int j=0; (line_ptr[pos] != ',') && (j < 3) && (pos < line.length());++pos)
			{
				if((0x30 <= line_ptr[pos]) && (line_ptr[pos] <= 0x39))
				{
					resp += line_ptr[pos];
				}
				j++;
			}
			UnlockCmdBuf();
			return atoi(resp.c_str());
		}
		sleep(1);
	}
	UnlockCmdBuf();
	return -1;
}
ats::String IRIDIUM::ReadImei()
{
	LockCmdBuf();
	AFS_Timer timer;
	timer.SetTime();
	SendATCommand("AT+CGSN\r");
	std::vector<ats::String> respList;
	while (timer.DiffTime() < 5) // 5 second timeout - should never hit this timeout
	{
		ats::String line = h_rtrimATResponse(GetLineFromResponseBuf());
		if(line.find("OK") != std::string::npos)
		{
			break;
		}
		if(!line.empty())
		{
			respList.push_back(line);
		}
	}
	UnlockCmdBuf();
	if(respList.empty())
	{
		return "";
	}
	return respList.at(respList.size() - 1);
}

bool IRIDIUM::DecodeCommandMessage(std::vector<char>msg)
{
	if (!(msg[0] == 0xaa && msg[1] == 0xcc))
		return false;
	
	switch (msg[3])
	{
		case 0x00:  // reboot;
			ats::system("reboot");
			break;
		case 0x01: // check-update
			break;
		case 0xFF:  // text command, next byte is length
		{
			int len = (int)msg[4];
			char buf[255];
			memcpy(&msg[5], buf, len);
			buf[len] = '\0';
			ats::system(buf);
		  break;
		}
	}
	return true;
}


