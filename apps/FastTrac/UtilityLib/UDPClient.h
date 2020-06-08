#include <iostream>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <utility.h>

using boost::asio::ip::udp;

class UDPClient
{
  boost::asio::io_service m_io_service;
  udp::socket * m_pSocket  ;
  udp::endpoint m_ReceiverEndpoint;
  bool m_EndPointResolved;

public:  
  // Open a UDP connection at addr:port
  UDPClient(const char * addr, const long port)
  {
		m_EndPointResolved = true;
		
  	try
  	{
			udp::resolver resolver(m_io_service);
			char strPort[32];
			sprintf(strPort, "%ld", port);
			udp::resolver::query query(udp::v4(), addr, strPort);
			m_ReceiverEndpoint = *resolver.resolve(query);

			m_pSocket = new udp::socket(m_io_service);
			m_pSocket->open(udp::v4());
		}
		catch (std::exception& e)
		{
			m_EndPointResolved = false;
		}
  }
  
  ~UDPClient()
  {
    delete m_pSocket;
  }
  
  
  // Send the msg to the socket via UDP.  Expect socket to return checksum as a string.
  // Return: true if checksum is received
  //         false if checksum not received or is wrong.
  // NOTE:  It is up to the calling application to decide on repeated tries, etc when 
  //        the message does not get through.
  //
  bool SendMessage(std::string &msg)
  {
  	if (!m_EndPointResolved)
		{
		  std::cout <<"End point is not resolved - is the other end running?" << endl;
      return false;
    }  	  
  	short cs = Checksum(msg.c_str());
			cout << "\nMSG:" << msg << "  \nCS:" << cs << "\n" << endl;

		m_pSocket->send_to(boost::asio::buffer(msg	), m_ReceiverEndpoint);
		boost::array<char, 128> recv_buf;
		udp::endpoint sender_endpoint;
		size_t len = m_pSocket->receive_from( boost::asio::buffer(recv_buf), sender_endpoint);

		if (len)
		{
			short cs2 = atoi(recv_buf.data());
			cout << "MSG:" << msg << "  \nCS:" << cs << " CSreturned:" << cs2 << "\n" << endl;
			
			if (cs2 == cs)
			  return true;
		}	  
		return false;
  }
 
  short Checksum(const char *msg)
  {
	  short i, blen;
  	char check_sum = 0;

  	blen = strlen(msg);

  	for (i = 0; i < blen; i++)
  	{
			check_sum ^= msg[i];
		}
		return check_sum;
  }
};

