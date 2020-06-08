// Implementation of the ServerSocket class
#include <stdio.h>
#include "ServerSocket.h"
#include "SocketException.h"

//using namespace boost;
//using namespace boost::this_thread;

ServerSocket::ServerSocket ( const char *IPAddr, int port )
{
  if ( ! Socket::create() )
  {
    throw SocketException ( "Could not create server socket." );
  }

  if ( ! Socket::bind (IPAddr, port ) )
  {
    throw SocketException ( "Could not bind to port." );
  }

  if ( ! Socket::listen() )
  {
    throw SocketException ( "Could not listen to socket." );
  }
}



const ServerSocket& ServerSocket::operator << ( const std::string& s ) const
{
  if ( ! Socket::send ( s ) )
  {
    throw SocketException ( "Could not write to socket." );
  }

  return *this;
}


const ServerSocket& ServerSocket::operator >> ( std::string& s ) const
{
  if ( ! Socket::recv ( s ) )
  {
    throw SocketException ( "Could not read from socket." );
  }

  return *this;
}

void ServerSocket::accept ( ServerSocket& sock )
{
  if ( ! Socket::accept ( sock ) )
  {
    throw SocketException ( "Could not accept socket." );
  }

  // we have accepted a new connection - fire off a thread
//	pthread_t thread_id;
  printf("---------------------\nReceived connection from %s\n",inet_ntoa(m_addr.sin_addr));
//  pthread_create(&thread_id, 0, &SocketHandler, (void*)sock.m_sock );
//  pthread_detach(thread_id);
}

