//========================================================================================
//
//	secondary-monitor - open a client that listens for UDP packages on the broadcast ip
//											on the "PrimaryPort" port. No ack is sent back.
//
//	See https://itracker.atlassian.net/browse/TFD-791?filter=-1 for particulars.
//
#include <ctime>
#include <iostream>
#include <iomanip>
#include <string>
#include <boost/array.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>

#include "socket_interface.h"
#include "ConfigDB.h"
#include "atslogger.h"
#include "calampsmessage.h"

#define CALAMP_HEADER_MIN_SIZE 5
#define CALAMP_OPTION_HEADER_DEFINE_BIT		 0x80



void processMessage(std::string &data, std::string &strHex);
std::string getImei(const std::string & msg);
std::string convertfromBCD(const std::string data);
std::string string_to_hex(const std::string& in);
std::string	m_defaultMobileId;
ATSLogger g_log;

int main(int argc, char* argv[])
{
	g_log.open_testdata(argv[0]);
	g_log.set_global_logger(&g_log);
	ats_logf(&g_log, "%s starting", argv[0]);
	
	namespace ip = boost::asio::ip;
	boost::asio::io_service io_service;
	db_monitor::ConfigDB db;
	m_defaultMobileId = db.GetValue("RedStone", "IMEI");
	
	int BroadcastPort = db.GetInt("system", "BroadcastPort", 39003);

	// open the firewall
  char buf[256];
  sprintf(buf, "iptables -D INPUT -i eth0 -p udp --dport %d -j ACCEPT\niptables -I INPUT -i eth0 -p udp --dport %d -j ACCEPT\n", BroadcastPort, BroadcastPort);
  system(buf);

	for (;;)
	{
		try
		{
			// Client binds to any address on Primary port (the same port on which broadcast data is sent from server).
			ip::udp::socket socket(io_service, ip::udp::endpoint(ip::udp::v4(), BroadcastPort ));

			ip::udp::endpoint sender_endpoint;

			// Receive data.
			boost::array<char, 256> buffer;
			std::size_t bytes_transferred = socket.receive_from(boost::asio::buffer(buffer), sender_endpoint);
			ats_logf(&g_log, "%d bytes received on broadcast port %d", bytes_transferred, BroadcastPort);
			std::string line;
			std::copy(buffer.begin(), buffer.begin()+bytes_transferred, std::back_inserter(line));
			std::ostringstream outstr;

			for(uint i = 0; i < bytes_transferred; ++i)
			{
				outstr << std::hex <<	std::setfill('0') << std::setw(2) << std::uppercase << (buffer[i] & 0xff);
			}

			ats_logf(&g_log, "   %s",outstr.str().c_str());
			std::string strHex = outstr.str().c_str();
			processMessage(line, strHex);

		}
		catch (std::exception& e)
		{
			std::cerr << e.what() << std::endl;
		}
	}	// end of infinite loop.
	return 0;
}

//------------------------------------------------------------------------------------------------
void processMessage(std::string &data, std::string &strHex)
{
	int data_size = data.length();

	if(data_size < CALAMP_HEADER_MIN_SIZE)
	{
		return;
	}

	// send data to other devices if it is not for this TRULink
	// the data coming back via cell is different than the data from Iridium.  So we try cell first

	QByteArray byteArray(data.c_str(), data.length());
	ats::String id(CalAmpsMessage::getImei(byteArray).toUtf8().data());

	if( id != m_defaultMobileId)
	{
		//check db-config for a known attached device
		db_monitor::ConfigDB db;
		ats::String socket = db.GetValue("devices", id.c_str());

		if(!socket.empty())
		{
			std::string result = string_to_hex(data);;

			send_redstone_ud_msg(socket.c_str(), 0, "calamp data=%s sender=packetizer-cams\r", strHex.c_str());
			ats_logf(&g_log, "Sending to socket: %s,  msg:%s", socket.c_str(), strHex.c_str());
		}
		else  // this is probably from the Iridium side
		{
			uint64_t idNumber = (((byteArray[8])&0xFFULL) << 56) |  (((byteArray[7])&0xFFULL) << 48) | (((byteArray[6])&0xFFULL) << 40) | (((byteArray[5])&0xFFULL) << 32) | (((byteArray[4])&0xFF) << 24) | (((byteArray[3])&0xFF) << 16) | (((byteArray[2])&0xFF) << 8) |  (((byteArray[1])&0xFF));
			ats::String id;
			ats_sprintf(&id, "%lld", idNumber);

			//check db-config
			ats_logf(ATSLogger::get_global_logger(), "%s,%d: Mobile ID of incoming message %s", __FILE__, __LINE__, id.c_str());
			db_monitor::ConfigDB db;
			ats::String socket = db.GetValue("devices", id);

			if(!socket.empty())
			{
				ats_logf(ATSLogger::get_global_logger(), "%s,%d: Sending incoming data to socket= %s", __FILE__, __LINE__, socket.c_str());
				send_redstone_ud_msg(socket.c_str(), 0, "iridium data=%s sender=packetizer-iridium\r", ats::to_hex(ats::String(byteArray.begin(), byteArray.end())).c_str());
			}
		}
	}
}
//------------------------------------------------------------------------------------------------
// getImei - extracts and decodes the IMEI from a CALAMP packet in BCD
//
std::string getImei(const std::string & msg)
{
	uint length = msg[1];
	std::string array = convertfromBCD(msg.substr(2, length));
	return array;
}

//------------------------------------------------------------------------------------------------
// 
std::string convertfromBCD(const std::string data)
{
	if(data.length() <= 0)
	{
		return data;
	}
	std::ostringstream result;
	result << std::setw(2) << std::setfill('0') << std::hex << std::uppercase;
	std::copy(data.begin(), data.end(), std::ostream_iterator<unsigned int>(result));

	return result.str();
}
std::string string_to_hex(const std::string& in) 
{
    std::stringstream ss;

    ss << std::hex << std::setw(2) << std::setfill('0');
    for (size_t i = 0; in.length() > i; ++i) {
        ss << static_cast<unsigned int>(static_cast<unsigned char>(in[i]));
    }

    return ss.str(); 
}

