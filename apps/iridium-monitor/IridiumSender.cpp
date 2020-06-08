#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <ctime>

#include "socket_interface.h"
#include "db-monitor.h"
#include "ConfigDB.h"

#include "Iridium.h"

#include "IridiumSender.h"

int IridiumSender::m_byteCount;
bool IridiumSender::m_dataLimitBreached;
int IridiumSender::m_elapsedTime;
int IridiumSender::m_byteLimit;
int IridiumSender::m_dataLimitTimeout;
AFS_Timer IridiumSender::m_dataLimitTimer;

IridiumSender::IridiumSender() : m_appName("Iridium")
{
	m_sbd_retries = 0;
	m_sbd_delay = 0;
	m_sbd_error = 0;
	std::srand(std::time(0));
}

void IridiumSender::start()
{
	m_pIridium.init();
	db_monitor::ConfigDB db;
	m_byteCount = db.GetInt(m_appName, "sentBytes", 0);
	m_byteLimit = db.GetInt(m_appName, "byteLimit", 20*1024); //20KB default
	m_dataLimitTimeout = db.GetInt(m_appName, "LimitTimePeriod", 86400); //24hrs default
	m_dataLimitBreached = db.GetBool(m_appName, "sentNotification", false);
	int timeStamp = db.GetInt(m_appName, "startPoint", time(NULL));
	m_elapsedTime = time(NULL) - timeStamp;
	m_dataLimitTimer.SetTime();
}

//298

void IridiumSender::IridiumGetResponse(char * p_msgBuf)
{
	 m_pIridium.GetResponse(p_msgBuf);
}
//298

bool IridiumSender::isNetworkAvailable()
{
	return (m_pIridium.IsNetworkAvailable() && isSBDReady());
}

void IridiumSender::waitForNetworkAvailable()
{
	while(1)
	{
		uint i = 0;
		while(!m_pIridium.IsNetworkAvailable())
		{
			i++;
			sleep(1);
		}
		if(m_pIridium.ClearMessageBuffer())
		{
			if(i > 4)
			{
				m_sbd_retries = 0;
			}
			return;
		}
		sleep(1);
	}
}

void IridiumSender::waitForSBDReady()
{
	while (m_sbd_timer.DiffTime() < m_sbd_delay)
	{
		sleep(1);
	}
}

bool IridiumSender::isSBDReady()
{
	return (m_sbd_timer.DiffTime() > m_sbd_delay);
}

//-----------------------------------------------------------------------------
// setSBDDelay - implement a delay between retries as per Iridium
//                   best practices.
//
void IridiumSender::setSBDDelay()
{
	if(m_sbd_error <= 4)
	{
		m_sbd_retries = 0;
		m_sbd_delay = 0;
		return;
	}

	m_sbd_retries++;
	if(m_sbd_retries == 1)
	{
		m_sbd_delay = 0;
		return;
	}

	if ( 3 >=  m_sbd_retries )
	{
		m_sbd_delay = std::rand() % 5;
		return;
	}

	if ( 5 >= m_sbd_retries )
	{
		m_sbd_delay =std::rand() % 30;
		return;
	}

	if( 11 >= m_sbd_retries )
	{
		m_sbd_delay = 30 + (30*(m_sbd_retries - 4));
		return;
	}

	m_sbd_delay = 300;
}

void IridiumSender::resetByteCount()
{
	ats_logf(ATSLOG_DEBUG, "%s,%d: Resetting the byte count.Bytes sent:%d",__FILE__,__LINE__, m_byteCount);
	m_dataLimitBreached = false;
	m_byteCount = 0;
	m_elapsedTime = 0;
	m_dataLimitTimer.SetTime();
	int timestamp = time(NULL);

	db_monitor::ConfigDB db;
	db.Set(m_appName, "startPoint", ats::toStr(timestamp));
	db.Set(m_appName, "sentBytes", ats::toStr(0));
	db.Set(m_appName,"sentNotification", ats::toStr(0));
}

bool IridiumSender::isMessageSent(int resp)
{
	if((resp <= 4) && (resp >= 0))
	{
		return true;
	}
	return false;
}

bool IridiumSender::sendMessageWithDataLimit(std::vector<char> &p_msgBuf)
{
	if((m_elapsedTime + m_dataLimitTimer.DiffTime()) > m_dataLimitTimeout)
	{
		resetByteCount();
	}

	if((m_byteCount + p_msgBuf.size()) > (uint)m_byteLimit)
	{
		m_error = IRIDIUM_OVERLIMIT_REACHED;
		if(!m_dataLimitBreached)
		{
			ats_logf(ATSLOG_DEBUG, "Iridium data has reached the limit of %d bytes. Bytes sent:%d. Message size %zd bytes", m_byteLimit, m_byteCount, p_msgBuf.size());
			m_dataLimitBreached = true;
			send_app_msg(m_appName.c_str(), "message-assembler", 0, "msg iridium_overlimit msg_priority=1\r");
			db_monitor::ConfigDB db;
			db.Set(m_appName,"sentNotification", ats::toStr(1));
		}
		return false;
	}

    m_pIridium.bTxInProgress = true;   //<ISCP-238>
	if(sendMessage(p_msgBuf))
	{
		m_byteCount += p_msgBuf.size();
		db_monitor::ConfigDB db;
		db.Set(m_appName, "sentBytes", ats::toStr(m_byteCount));
		m_pIridium.bTxInProgress = false;  //<ISCP-238>
		return true;
	}
	m_pIridium.bTxInProgress = false;  //<ISCP-238>

	return false;
}

bool IridiumSender::sendMessage(std::vector< char > &p_msgBuf)
{
	std::stringstream ostream;
	ostream << "MSG["<<p_msgBuf.size()<< "]: ";
	for (uint i= 0 ; i < p_msgBuf.size(); ++i)
	{
		ostream << std::uppercase <<std::setfill('0') << std::setw(2)
				<< std::hex << (int)(p_msgBuf[i])<< " ";
	}
	ostream << "[END]";
	ats_logf(ATSLOG_DEBUG, "%s,%d: Message data\n%s", __FILE__, __LINE__, ostream.str().c_str());

		uint i = 0;  //<ISCP-238>
		while(!m_pIridium.IsNetworkAvailable() && i<10)
		{
			i++;
			sleep(1);
		}//<ISCP-238>

	//PrepareToSendMessage takes ptr to buffer and length of buffer
	//The length of the buffer does not include the checksum bytes
	if(!m_pIridium.PrepareToSendMessage((char *)p_msgBuf.data(), p_msgBuf.size() - 2))
	{
		m_pIridium.ClearMessageBuffer();//<ISCP-238>
		ats_logf(ATSLOG_DEBUG, "%s,%d: >> sendMessage 0 <<\r", __FILE__, __LINE__);//<ISCP-238>
		if(!m_pIridium.PrepareToSendMessage((char *)p_msgBuf.data(), p_msgBuf.size() - 2))//<ISCP-238>
		{
			
			ats_logf(ATSLOG_DEBUG, "%s,%d: >> Cannot prepare to send message through Iridium.", __FILE__, __LINE__);//<ISCP-238>
			for(uint i=0 ; i< 4; i++)	//<ISCP-238>
			{

				if(m_pIridium.ClearMessageBuffer()) // //<ISCP-238>
				{
		m_sbd_error = MESSAGE_BUFFER_ERROR;
		return false;
	}
				else
				{
					sleep(1); //<ISCP-238>
				}
			}
		}
	}

	ats_logf(ATSLOG_DEBUG, "%s,%d: >> sendMessage 1 <<\r", __FILE__, __LINE__);
	m_sbd_error = m_pIridium.SendMessage();
	ats_logf(ATSLOG_DEBUG, "%s,%d: >> sendMessage 2 <<\r", __FILE__, __LINE__);
	setSBDDelay();
	m_sbd_timer.SetTime();
	m_pIridium.ClearMessageBuffer();
	ats_logf(ATSLOG_DEBUG, "%s, %d: SBD response:%d, m_sbd_retries:%d",__FILE__, __LINE__, m_sbd_error,m_sbd_retries);
	return isMessageSent(m_sbd_error);
}

ats::String IridiumSender::errorStr(int err)
{
	ats::String ret;
	switch(err)
	{
	case MESSAGE_BUFFER_ERROR:
		return "Message could not be loaded into Iridium buffer.";
	case IRIDIUM_OVERLIMIT_REACHED:
		ats_sprintf(&ret,"Data limit breached. Please wait %d seconds and try again.", m_dataLimitTimeout);
		return ret;
	case SBDIX_CMD_TIMEOUT:
		return "Timeout reading response to AT+SBDIX.";
	case 0:
		return "Message was transferred successfully";
	case 1:
		return "Message was transferred successfully. MT message queue was too big to transfer.";
	case 2:
		return "Message was transferred successfully.  Location Update was not accepted.";
	case 3:
	case 4:
		ats_sprintf(&ret, "Message was transferred successfully. Unknown reason code: %u.", err);
		return ret;
	case 10:
		return "Message failed to transfer. GSS reported that the call did not complete in the allowed time.";
	case 11:
		return "Message failed to transfer. MO message queue at the GSS is full.";
	case 12:
		return "Message failed to transfer. MO message has too many segments.";
	case 13:
		return "Message failed to transfer. GSS reported that the session did not complete.";
	case 14:
		return "Message failed to transfer. Invalid segment size.";
	case 15:
		return "Message failed to transfer. Access is denied.";
	case 16:
		return "Message failed to transfer. ISU has been locked and may not make SBD calls (see +CULK command).";
	case 17:
		return "Message failed to transfer. Gateway not responding (local session timeout).";
	case 18:
		return "Message failed to transfer. Connection lost (RF drop).";
	case 19:
		return "Message failed to transfer. Link failure (A protocol error caused termination of the call).";
	case 32:
		return "Message failed to transfer. No network service, unable to initiate call.";
	case 33:
		return "Message failed to transfer. Antenna fault, unable to initiate call.";
	case 34:
		return "Message failed to transfer. Radio is disabled, unable to initiate call (see *Rn command).";
	case 35:
		return "Message failed to transfer. ISU is busy, unable to initiate call.";
	case 36:
		return "Message failed to transfer. Try later, must wait 3 minutes since last registration.";
	case 37:
		return "Message failed to transfer. SBD service is temporarily disabled.";
	case 38:
		return "Message failed to transfer. Try later, traffic management period (see +SBDLOE command).";
	case 64:
		return "Message failed to transfer. Band violation (attempt to transmit outside permitted frequency band).";
	case 65:
		return "Message failed to transfer. PLL lock failure; hardware error during attempted transmit.";
	default:
		if((err > 0) && (err <= 65))
		{
			ats_sprintf(&ret, "Message failed to transfer. Unkown reason code: %u.", err);
		}
		else
		{
			ats_sprintf(&ret, "Unknown error occured. Unknown SBD return code.Error: %u.", err);
		}
	}

	return ret;
}
