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

#include <CSelect.h>
#include "ats-common.h"
#include "db-monitor.h"
#include "ConfigDB.h"
#include "socket_interface.h"


#include "Iridium.h"

IRIDIUM::IRIDIUM()
{
	m_bNetworkAvailable = false;
	m_bWaitingForSendToComplete = false;
	m_rssi = 0;
	m_mode = AT_MODE;

	pthread_mutex_init(&m_atPortMutex, 0);
	pthread_mutex_init(&m_respBufferMutex, 0);

	system("stty -F /dev/ttySP4 -echo raw 19200");

	if (pipe(m_cmd_pipe) == -1)
	{
		ats_logf(ATSLogger::get_global_logger(), "Unable to create pipe. pipe returned -1, errno %d", errno);
		exit(-1);
	}

	// create the reader thread to read the port and load the buffer.
	pthread_create(&(m_reader_thread),	(pthread_attr_t*)0, reader_thread,	this);

	Setup(); // set up the iridium for RedStone usage (3 wire connection on RedStone)
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
		ats::system("echo 'R' > /dev/set-gpio");
	}
	else
	{
		ats::system("echo 'r' > /dev/set-gpio");
	}
}

//----------------------------------------------------------------------------------------------------------------------
void IRIDIUM::Setup()
{
	RESPONSE_CODES ret;

	SetPower(true);

	if ((ret = SendRespond("ATE0\r")) != OK) // turn echo off
	{
		ats_logf(ATSLogger::get_global_logger(), "Unable to turn echo off (returned %d)", short(ret) );
	}
	if ((ret = SendRespond("AT&K0\r")) != OK) // turn flow control off
	{
		ats_logf(ATSLogger::get_global_logger(), "Unable to turn off flow control (returned %d)", short(ret) );
	}
	if ((ret = SendRespond("AT&D0\r")) != OK) // turn DTR off
	{
		ats_logf(ATSLogger::get_global_logger(), "Unable to turn off DTR (returned %d)", short(ret) );
	}

	if ((ret = SendRespond("AT*R1\r")) != OK) // turn Radio On
	{
		ats_logf(ATSLogger::get_global_logger(), "Unable to turn Radio On (returned %d)", short(ret) );
	}

	if ((ret = SendRespond("AT+SBDMTA=1\r")) != OK)
	{
		ats_logf(ATSLogger::get_global_logger(), "Unable to enable SBDRING (returned %d)", short(ret) );
	}

	if ((ret = SendRespond("AT+SBDAREG=1\r")) != OK)
	{
		ats_logf(ATSLogger::get_global_logger(), "Unable to set Automatic Registration (returned %d.", short (ret));
	}

	SendATCommand( "AT+CIER=1,1,1\r");	// send status of network connection whenever it changes
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
//
IRIDIUM::RESPONSE_CODES IRIDIUM::SendRespond(const ats::String buf)
{
	AFS_Timer timer;
	short errcount = 0;

	//ats_logf(ATSLogger::get_global_logger(), "SendRespond sending %s", buf );
	
	while (errcount < 10)
	{

		LockResponseBuf();
		m_ResponseBuf.clear();
		UnlockResponseBuf();
		ats_logf(ATSLogger::get_global_logger(), "SendRespond sending %s", buf.c_str() );
		SendATCommand(buf);
		timer.SetTime();
		
		while (timer.DiffTimeMS() < 1000)
		{
			ats::String line = h_rtrimATResponse(GetLineFromResponseBuf());
			if (!line.empty())
			{
				ats_logf(ATSLogger::get_global_logger(), "SendRespond Response is %s", line.c_str() );
				IRIDIUM::RESPONSE_CODES ret;
				if (strncmp(line.c_str(), "OK", 2) == 0)
				{
					ret = OK;
					ats_logf(ATSLogger::get_global_logger(), "SendRespond return val is %d", ret );
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
					ats_logf(ATSLogger::get_global_logger(), "Rx from Iridium: ##%s##", line.c_str() );
					continue;
				}
				ats_logf(ATSLogger::get_global_logger(), "SendRespond Response is %s. Returning with code %d", line.c_str(), ret );
				return ret;
			}
			usleep(10000);
		}
		errcount++;
	}
	return NONE;
}

bool IRIDIUM::WaitForResponse(const ats::String buf, const char *expected)
{
	AFS_Timer timer;

	ats_logf(ATSLogger::get_global_logger(), "WFR: Waiting for %s", expected );

	LockResponseBuf();
	m_ResponseBuf.clear();
	UnlockResponseBuf();

	SendATCommand(buf);
	timer.SetTime();

	while (timer.DiffTimeMS() < 1000)
	{
		ats::String line = GetLineFromResponseBuf();
		if (!line.empty())	// a complete line was found
		{
			ats_logf(ATSLogger::get_global_logger(), "WaitForResponse got line: ##%s", line.c_str());
			if (line.find(expected) != std::string::npos)
			{
				ats_logf(ATSLogger::get_global_logger(), "WaitForResponse got expected %s",expected);
				return true;
			}
		}
	}
	ats_logf(ATSLogger::get_global_logger(), "WaitForResponse did NOT get expected %s",expected);
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
		bool ret =	SendSBD(imsg, len + 2);
		return ret;
	}

	// we have failed if we get here.
	
	ats_logf(ATSLogger::get_global_logger(), "SBDWB did not return READY");
	return false;
}

int IRIDIUM::ManualNetworkRegistration()
{
	ats_logf(ATSLogger::get_global_logger(), "Starting manual network registration.");
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

	ats_logf(ATSLogger::get_global_logger(), "Sending %d bytes", len);
	write(m_cmd_pipe[1], buf, len);

	timer.SetTime();
	while (timer.DiffTimeMS() < 10000)
	{
		ats::String line = GetLineFromResponseBuf();
		if (!line.empty())	// a complete line was found
		{
			ats_logf(ATSLogger::get_global_logger(), "SendSBD got line: ##%s", line.c_str());
			const char* resp = line.c_str();
			switch (resp[0])
			{
				case '0':
					{
						ats_logf(ATSLogger::get_global_logger(), "SendSBD::Data accepted");
						return true;
					}
				case '1':
					{
						ats_logf(ATSLogger::get_global_logger(), "SendSBD::Not enough bytes sent");
						break;
					}
				case '2':
					{
						ats_logf(ATSLogger::get_global_logger(), "SendSBD::Data rejected - bad checksum");
						return false;
						break;
					}
				case '3':
					{
						ats_logf(ATSLogger::get_global_logger(), "SendSBD::Invalid data size");
						break;
					}
					//				 case 'O':	// assume that this is OK
					//					 if (resp[1] == 'K')
					//						 return ret;
				default:
					continue;
			}
		}
		usleep(10000);
	}
	ats_logf(ATSLogger::get_global_logger(), "SendSBD timed out.");
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

	int fdr = open(IRIDIUM_PORT, O_RDONLY| O_NOCTTY);
	if (fdr < 0)
	{
		ats_logf(ATSLogger::get_global_logger(), "Unable to open Iridium port: open returned (%d), errno %d", fdr, errno);
	}
	int fdw = open(IRIDIUM_PORT, O_WRONLY | O_NOCTTY);
	if (fdw < 0)
	{
		ats_logf(ATSLogger::get_global_logger(), "Unable to open Iridium port: open returned (%d), errno %d", fdw, errno);
	}
	ats_logf(ATSLogger::get_global_logger(), "fdr=%d, m_cmd_pipe[0]= %d",fdr, iridium.m_cmd_pipe[0]);

	irdSelect.EnableDelay(false);
	irdSelect.Add(fdr);
	irdSelect.Add(iridium.m_cmd_pipe[0]);

	while (1)
	{
		if(!irdSelect.Select())
		{
			ats_logf(ATSLogger::get_global_logger(), "Select returned and error errno=%d", errno);
			continue;
		}

		if(irdSelect.HasData(fdr))
		{
				char readBuf[IRIDIUM_MAX_CHAR_READ];
				memset(readBuf, 0, sizeof(char));
				int len = read(fdr, readBuf, IRIDIUM_MAX_CHAR_READ);
				if(len > 0)
				{
					ats_logf(ATSLOG(0),"Read %d bytes.", len);
					std::stringstream ostream;
					ostream << "Bytes["<<len<< "]: ";
					for (int i= 0 ; i < len; ++i)
					{
						ostream << std::uppercase <<std::setfill('0') << std::setw(2)
										<< std::hex << (int)(readBuf[i])<< " ";
					}
					ats_logf(ATSLOG(0), "%s", ostream.str().c_str());
					InputBuf.Add(readBuf, len);
					IRIDIUM::ReadATResponses(&iridium, InputBuf, fdr, fdw);
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
				ats_logf(ATSLOG(0),"Read %d bytes from pipe.", len);
				std::stringstream ostream;
				ostream << "Bytes["<<len<< "]: ";
				for (int i= 0 ; i < len; ++i)
				{
					ostream << std::uppercase <<std::setfill('0') << std::setw(2)
									<< std::hex << (int)(writeBuf[i])<< " ";
				}
				ats_logf(ATSLogger::get_global_logger(), "%s", ostream.str().c_str());
				write(fdw,writeBuf, len);
			}
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
	ats_logf(ATSLogger::get_global_logger(), "%s,%d:Processing SBDIX response. %s", __FILE__, __LINE__, trimmed_resp.c_str());
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
	ats_logf(ATSLogger::get_global_logger(), "%s,%d: SBDIX Response MO Status:%d MOMSN:%d MT Status:%d MTMSN:%d MT Length:%d MT Queued:%d",
					 __FILE__, __LINE__, status.mo_status, status.momsn, status.mt_status, status.mtmsn, status.mt_length, status.mt_queued);
	return true;
}


bool IRIDIUM::ProcessMTMessage(std::vector<char>& buf)
{
	ats_logf(ATSLOG(0), "Raw data read in buffer.");
	{
		std::stringstream ostream;
		ostream << "RECV["<<buf.size()<< "]: ";
		for (uint i= 0 ; i < buf.size(); ++i)
		{
						ostream << std::uppercase <<std::setfill('0') << std::setw(2)
														<< std::hex << (int)(buf[i])<< " ";
		}
		ats_logf(ATSLOG(0), "%s", ostream.str().c_str());
	}

	if (buf.size() < 5)
	{
		ats_logf(ATSLogger::get_global_logger(), "MT Message length is too small.");
		return false;
	}

	uint msg_size = buf[1] + buf[0] * 256;
	if(buf.size() < msg_size + 4)
	{
		ats_logf(ATSLogger::get_global_logger(), "MT Message length is too small.Read size: %d, Buff size: %d", msg_size, buf.size());
		return false;
	}

	ushort checksum = buf[msg_size + 3] + buf[msg_size + 2] * 256;
	std::vector<char> msg(buf.begin() + 2, buf.end() - 2);
	ushort calc_checksum = ComputeCheckSum(msg);

	if(checksum != calc_checksum)
	{
		ats_logf(ATSLogger::get_global_logger(), "MT Message checksum incorrect.calc_checksum=%d checksum=%d", calc_checksum, checksum);
		return false;
	}

	//do something with message now
	{
		std::stringstream ostream;
		ostream << "RECV["<<msg_size<< "]: ";
		for (uint i= 0 ; i < msg_size; ++i)
		{
			ostream << std::uppercase <<std::setfill('0') << std::setw(2)
							<< std::hex << (int)(msg[i])<< " ";
		}
		ats_logf(ATSLOG(5), "%s", ostream.str().c_str());

		uint id_size = (uint)msg[0];
		if(id_size >= msg.size())
		{
			ats_logf(ATSLogger::get_global_logger(), "%s,%d: Message recieved in unknown format. Id size(%d bytes) exceeds message size(%d bytes).",
							 __FILE__, __LINE__, id_size, msg.size());
			return false;
		}
		ats::String id(msg.begin() + 1, msg.begin() + id_size + 1);

		//check db-config
		ats_logf(ATSLogger::get_global_logger(), "%s,%d: Mobile ID of message %s", __FILE__, __LINE__, id.c_str());
		db_monitor::ConfigDB db;
		ats::String socket = db.GetValue("devices", id);
		if(!socket.empty())
		{
			ats_logf(ATSLogger::get_global_logger(), "%s,%d: socket= %s", __FILE__, __LINE__, socket.c_str());
			send_redstone_ud_msg(socket.c_str(), 0, "iridium data=%s sender=packetizer-iridium\r", ats::to_hex(ats::String(msg.begin(), msg.end())).c_str());
		}
		else
		{
			ats_logf(ATSLogger::get_global_logger(), "%s,%d: No registered process for ID:%s", __FILE__, __LINE__, id.c_str());
		}

	}

	return true;
}

//wrapper to write command, saves calculating length of string
ssize_t h_writeString(int p_fdw, const ats::String& p_cmd)
{
	return write(p_fdw, p_cmd.c_str(), p_cmd.length());
}

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
			ats_logf(ATSLogger::get_global_logger(), "CharBuf GetLine, error finding EOL, error code:%d", lineLen);
			buf.Reset();
			return -1;
		}
		else if (lineLen <= 0 )
		{
			break; //No EOL detected
		}
		ats::String line = strResponse;
		// now look for asynchronous strings in the data
		uint i;
		if ((i = line.find("+CIEV:")) != std::string::npos)
		{
			uint sig_offset = i + 6;
			uint value_offset = i + 8;

			if (strResponse[sig_offset] == '1')
			{
				if (strResponse[value_offset] == '1')
				{
					if (iridium.m_bNetworkAvailable == false)
					{
						iridium.m_bNetworkAvailable = true;
						ats_logf(ATSLogger::get_global_logger(), "Network available - %d bars	(Actual string ##%s##)", iridium.m_rssi, strResponse);
					}
				}
				else
				{
					if (iridium.m_bNetworkAvailable == true)
					{
						iridium.m_bNetworkAvailable = false;
						ats_logf(ATSLogger::get_global_logger(), "Network unavailable (Actual string ##%s##)", strResponse);
					}
				}
			}

			if (strResponse[sig_offset] == '0')	// signal strength indicator
			{
				iridium.m_rssi =	strResponse[value_offset] - '0';
			}
		}
		else if ((i = line.find("SBDRING")) != std::string::npos)
		{
			ats_logf(ATSLogger::get_global_logger(), "Somebody wants to talk to us!\r\n");
			h_writeString(p_fdw, "\rAT+SBDIXA\r");
		}
		else if (iridium.ProcessSBDIXResponse(line, status))
		{
			if((status.mt_status == 1) && (status.mt_length > 0))
			{
				ats_logf(ATSLogger::get_global_logger(), "Message has been received");

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
		else if ((i = line.find("HARDWARE FAILURE ")) != std::string::npos)
		{
				ats_logf(ATSLogger::get_global_logger(), "Critical ERROR - HARDWARE FAILURE detected\r\n");
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
	ats_logf(ATSLogger::get_global_logger(),"Writing command to pipe: %s", buf.c_str());
	write(m_cmd_pipe[1], buf.c_str(), buf.length());
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

	SendATCommand("AT+SBDIX\r");
	AFS_Timer timer;
	timer.SetTime();
	while (timer.DiffTime() < 60)	// 60 second timeout - should never hit this timeout
																			// but it exists to avoid deadlock in the
																			// event of a communication issue with the Iridium module.
	{
		ats::String line = GetLineFromResponseBuf();
		uint pos;
		if((pos = line.find("+SBDIX:")) != std::string::npos)
		{
			ats_logf(ATSLogger::get_global_logger(), "%s, %d: Response:\r\n%s\n--End of String--",__FILE__, __LINE__, line.c_str());
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

			return atoi(resp.c_str());
		}
		sleep(1);
	}
	return -1;
}

