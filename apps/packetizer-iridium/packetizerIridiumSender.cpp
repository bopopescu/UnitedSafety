#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <ctime>

#include "packetizerIridiumSender.h"

PacketizerIridiumSender::PacketizerIridiumSender(MyData& p_mdata) : PacketizerSender(p_mdata)
{
	m_sbd_retries = 0;
	std::srand(std::time(0));
}


void PacketizerIridiumSender::waitForNetworkAvailable()
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

//-----------------------------------------------------------------------------
// waitForSBDReady - implement a delay between retries as per Iridium
//                   best practices.
//
void PacketizerIridiumSender::waitForSBDReady()
{
	if(m_sbd_error <= 4)
	{
		m_sbd_retries = 0;
		return;
	}

	m_sbd_retries++;
	if(m_sbd_retries == 1)
	{
		return;
	}

	if ( 3 >=  m_sbd_retries )
	{
		uint time = std::rand() % 5;
		sleep(time);
		return;
	}

	if ( 5 >= m_sbd_retries )
	{
		uint time =std::rand() % 30;
		sleep(time);
		return;
	}

	if( 11 >= m_sbd_retries )
	{
		uint time = 30 + (30*(m_sbd_retries - 4));
		sleep(time);
		return;
	}

	sleep(300);
}

bool PacketizerIridiumSender::sendMessage(std::vector< char > &p_msgBuf)
{
	std::stringstream ostream;
	ostream << "MSG["<<p_msgBuf.size()<< "]: ";
	for (uint i= 0 ; i < p_msgBuf.size(); ++i)
	{
		ostream << std::uppercase <<std::setfill('0') << std::setw(2)
				<< std::hex << (int)(p_msgBuf[i])<< " ";
	}
	ostream << "[END]";
	ats_logf(ATSLogger::get_global_logger(), "%s,%d: Message data\n%s", __FILE__, __LINE__, ostream.str().c_str());

	//PrepareToSendMessage takes ptr to buffer and length of buffer
	//The length of the buffer does not include the checksum bytes
	if(!m_pIridium.PrepareToSendMessage((char *)p_msgBuf.data(), p_msgBuf.size() - 2))
	{
		ats_logf(ATSLogger::get_global_logger(), "%s,%d: Cannot prepare to send message through Iridium.", __FILE__, __LINE__);
		return false;
	}
	waitForSBDReady();
	return true;
}

ats::String PacketizerIridiumSender::errorStr()
{
	ats::String ret;
	switch(m_sbd_error)
	{
	case -1:
		return "Timeout reading response to AT+SBDIX.";
	case 0:
		return "Message was transferred successfully";
	case 1:
		return "Message was transferred successfully. MT message queue was too big to transfer.";
	case 2:
		return "Message was transferred successfully.  Location Update was not accepted.";
	case 3:
	case 4:
		ats_sprintf(&ret, "Message was transferred successfully. Unknown reason code: %u.", m_sbd_error);
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

	}
	if(((m_sbd_error > 4)&&(m_sbd_error <=8)) || \
			((m_sbd_error > 19)&&(m_sbd_error <= 31)) || \
			((m_sbd_error > 38) && (m_sbd_error <= 63)))
	{
		ats_sprintf(&ret, "Message failed to transfer. Unkown reason code: %u.", m_sbd_error);
		return ret;
	}
	ats_sprintf(&ret, "Unknown error occured. Unknown SBD return code.Error: %u.", m_sbd_error);
	return ret;
}

bool PacketizerIridiumSender::waitforack()
{
	m_sbd_error = m_pIridium.SendMessage();
	m_pIridium.ClearMessageBuffer();
	ats_logf(ATSLogger::get_global_logger(), "%s, %d: SBD response:%d, m_sbd_retries:%d",__FILE__, __LINE__, m_sbd_error,m_sbd_retries);
	if((m_sbd_error <= 4) && (m_sbd_error >= 0))
	{
		return true;
	}
	return false;
}
