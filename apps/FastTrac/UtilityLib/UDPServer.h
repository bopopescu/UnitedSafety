#include <iostream>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <utility.h>
#include <iostream>
#include <string>
#include <boost/array.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>

using namespace std;
using boost::asio::ip::udp;



class UDPServer
{
  boost::asio::io_service m_io_service;
  udp::socket * m_pSocket  ;
  
  // Open a UDP connection at addr:port
  UDPServer(const char * addr, const long port)
  {
		udp::resolver resolver(m_io_service);
		char strPort[32];
		sprintf(strPort, "%ld", port);
		udp::resolver::query query(udp::v4(), addr, strPort);
		udp::endpoint receiver_endpoint = *resolver.resolve(query);

		m_pSocket = new udp::socket(m_io_service);
		m_pSocket->open(udp::v4());
  }
  
  ~UDPServer()
  {
    delete m_pSocket;
  }
  
  
  // Send the msg to the socket via UDP.  Expect socket to return checksum as a string.
  // Return: true if checksum is received
  //         false if checksum not received or is wrong.
  // NOTE:  It is up to the calling application to decide on repeated tries, etc when 
  //        the message does not get through.
  //
  bool SendMessage(const char *msg)
  {
  	short cs = Checksum(msg);
  	
		m_pSocket->send_to(boost::asio::buffer(send_buf), receiver_endpoint);
		boost::array<char, 128> recv_buf;
		udp::endpoint sender_endpoint;
		size_t len = socket.receive_from( boost::asio::buffer(recv_buf), sender_endpoint);
		short cs2 = atoi(recv_buf.data());
		if (cs2 == cs)
		  return true;
		  
		return false;
  }

  short Checksum(const char *msg)
  {
	  short i, blen;
  	char check_sum = 0;

  	blen = strlen(buf);

  	if (blen > len - 4)
			return false;

  	for (i = 0; i < blen; i++)
			check_sum ^= buf[i];
  }
  
};


