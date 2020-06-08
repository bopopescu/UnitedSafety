// Definition of the ServerSocket class
#pragma once

//#include <boost/thread.hpp>

#include "Socket.h"
//using namespace boost;
//using namespace boost::this_thread;



class ServerSocket : private Socket
{
private:
  bool m_bTerminate;  // indicates that all threads should terminate
//  boost::thread m_Thread;

protected:
//  virtual void WorkerThread() = 0;  // pure virtual - you MUST derive from this class

public:

  ServerSocket (const char * IPAddr, int port );
  ServerSocket ()
  {
    m_bTerminate = false;
  };
  
  virtual ~ServerSocket()
  {
    m_bTerminate = true;
  };
  

  const ServerSocket& operator << ( const std::string& ) const;
  const ServerSocket& operator >> ( std::string& ) const;

  void accept ( ServerSocket& );

};


