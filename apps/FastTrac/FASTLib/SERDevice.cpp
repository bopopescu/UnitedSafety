/*!
 * \file RTUDevice.cpp
 *
 * \brief base class for input port drivers.
 *
 * \note
 * This is the output from the RTU. First two lines are for count,
 * last line (starting with U) where 111 indicates data from channel 1
 *
 * \code
 * 0_________1_________2_________3_________4_________5_________6__________\n
 * 01234567890123456789012345678901234567890123456789012345678901234567890\n
 * U000555000333444000888000723fff2220000000000009997771116fefff304a      \n
 * UEEEFFFHHHGGGSSSKKKTTTJJJtmpfffAAAMMMNNNUUULLLCCCBBBPPPunkfffchck - by pin
 * \endcode
 *
 *
 * \author David Huff
 * \date   24/08/2010
 * Copyright &copy; Absolute Fire Systems (2010)
 */

using namespace std;

#include <sys/ioctl.h>
#include "SERDevice.h"
#include "WriteDebug.h"



bool   SerDevice::mybIsDebugging;



SerDevice::SerDevice()
{
  strcpy(m_DevName, "None");
  m_RTUPort = -1; // indicates not set
  m_bIsNMEA = false;  // acts as a source of NMEA data
  m_fd = -1;
  selftest = DRVR_UNKNOWN;
}

SerDevice::~SerDevice()
{
  // close the port
  if (m_fd != -1)
    close (m_fd);
}

void SerDevice::SetPort
(
  short RTUPort,  // setup and open the port RTUPort will be 0, 1, or 2
  short baudIdx
)
{
  m_RTUPort = RTUPort;
  UpdateFileNames();

  char strPort[32];
  snprintf(strPort, sizeof(strPort) - 1, "/dev/ttySER%d", RTUPort + 1);
  strPort[sizeof(strPort) - 1] = '\0';
  SetPort(strPort, baudIdx);
}
void SerDevice::SetPort
(
  const char * strPort,  // setup and open the port RTUPort will be 0, 1, or 2
  short baudIdx
)
{
  VerifyParms();

  m_fd = open(strPort, O_RDWR| O_NOCTTY);// | O_NODELAY);

  if (m_fd == -1)  //error
  {
    perror(NULL);
//    WriteDebug(__FILE__, __LINE__, "RTUDevice::Unable to open port for RS232 communications ", m_DevName);
    return;
  }
  SetPortBaud(baudIdx);
}

  // accessor functions for low level values
bool SerDevice::IsNMEA()
{
  return m_bIsNMEA;
}

int SerDevice::GetFD()
{
  return m_fd;
}


void SerDevice::SetPortBaud(speed_t speed)
{
  if (m_fd == -1)
    return;

  fcntl(m_fd, F_SETFL, 0);
  struct termios options;

  tcgetattr(m_fd, &options);  // Get the current options for the port...

  // Set the baud rates...
  cfsetispeed(&options, speed);
  cfsetospeed(&options, speed);
  cfmakeraw(&options);  // this is important - make data go out without CR/LF and loopback work

  // Enable the receiver and set local mode...

  options.c_cflag |= (CLOCAL | CREAD);

  options.c_cflag &= ~PARENB;
  options.c_cflag &= ~CSTOPB;
  options.c_cflag &= ~CSIZE;
  options.c_cflag &= ~CRTSCTS; //No hard flow control
  options.c_cflag &= ~HUPCL; //Hang Up on last Close
  options.c_cflag |= CS8;

  // Set the new options for the port...
  tcsetattr(m_fd, TCSANOW, &options);

  // play with RTS & DTR
  int iFlags;

  // turn on DTR and leave it on to set the RTUs up properly
  iFlags = TIOCM_DTR;
  ioctl(m_fd, TIOCMBIS, &iFlags);

}




//-------------------------------------------------------------------------------------------------
// DumpParms - dumps the device type and parms to the debug.txt file.
//
void SerDevice::DumpParms()
{
  if (strcmp(m_DevName, "None") != 0)
  {
    WriteDebugString(m_DevName);
    WriteDebugString("  No Parameters");
  }
}


// will usually be overwritten but this does the reading from the port and the raw logging so
// it should be called in all functions.
bool SerDevice::ProcessIncomingData()
{
  m_LenInBuf = read(m_fd, m_InBuf, 1023);
  m_InBuf[m_LenInBuf] = '\0';

  LogRawData();

  return true;
}

void SerDevice::LogRawData()
{
  if(IsLoggingRawData() && m_LenInBuf > 0)
  {
    GetRawFile().Add(m_InBuf);
  }
}

//-------------------------------------------------------------------------
//  GetBaudBit
//    return the baud bit as a speed_t value for input into cfsetispeed etc
//
short SerDevice::GetBaudBit(short idx)
{
  switch (idx)
  {
    case 0: // 4800
      return B4800;
    case 1: // 9600
      return B9600;
    case 2: // 19200
      return B19200;
    case 3: // 38400
      return B38400;
    case 4: // 57600
      return B57600;
    case 5: // 115200
      return B115200;
    default:
      return B9600;
  }
}
