#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>

#include "RTUDevice.h"
//#include "FTData_Error.h"
#include "CSelect.h"
//            Pin    A   B   C  E  F   G  H   J   K   L   M   N   P   S   T   U
//int arrChanIdx[] = {31, 49, 46, 1, 4, 10, 7, 22, 16, 43, 34, 37, 52, 13, 19, 40}; // where each chan starts in the 'U' string




//-------------------------------------------------------------------
RTUDevice::RTUDevice()
{
  strcpy(m_DevName, "RTUDevice");
  m_fd = -1;
  m_nCharsRcvd = 0;
  m_nDevices = 0;
  m_Timer.SetTime();

  m_nInterrupts = 0;
  m_bValidDataRcvd = false;
  m_bInterruptSent = false;

  short i;

  for (i = 0; i < NUM_PINS; i++)
  {
    myHandlers[i] = NULL;
  }

  for (i = 0; i < 4; i++)
    m_RTUDevices[i] = NULL;

  m_nBytesSent = 0;
  m_nBytesRcvd = 0;
  selftest = DRVR_INIT;

}

//-------------------------------------------------------------------
RTUDevice::~RTUDevice()
{
  selftest = DRVR_DIS;

  if (m_fd != -1)
  {
    close(m_fd);
    m_fd = -1;
  }
}


//-------------------------------------------------------------------
// process the RTU input, return 1 if there was an event
//
// looks for chars on the port and sends them to the AFF
bool RTUDevice::ProcessIncomingData()
{
 if (m_bStartup)  // just eat and delete the first second of data
  {
    char buf[256], buf2[256];
    int len;
    len = read(m_fd, buf, 256);

    if (m_Timer.DiffTimeMS() > 1000)
      m_bStartup = false;
    else
      return false;
  }

  if (ProcessIncomingString())
  {
    selftest = DRVR_DATA_OK;
    m_bValidDataRcvd = true;
    m_TimeSinceValidData = 0;
    m_bInterruptSent = false;

    short i;

    for (i = 0; i < m_nDevices; i++)
    {
      if (m_RTUDevices[i] != NULL)
        m_RTUDevices[i]->ProcessIncomingData(m_Chans);
    }
  }
  else
  {
    selftest = DRVR_RXING;
  }

  return false;
}

//-------------------------------------------------------------------------
void  RTUDevice::ProcessEveryTime()
{
  for (short i = 0; i < m_nDevices; i++)
  {
    if (m_RTUDevices[i] != NULL)
      m_RTUDevices[i]->ProcessEveryTime();
  }
}

//-------------------------------------------------------------------------
void  RTUDevice::ProcessEverySecond()
{
  if (m_Timer.DiffTimeMS() > 20000)
  {
    m_Timer.SetTime();

    if (m_nCharsRcvd == 0)
    {
      GetVFD().DrawLine(1, "NO RTU Data!");
      GetVFD().DrawLine(2, m_DevName);
    }

    m_nCharsRcvd = 0;
  }

  if (m_bValidDataRcvd)
  {
    m_TimeSinceValidData++;

    if (m_TimeSinceValidData > 5 && !m_bInterruptSent && m_nInterrupts < 5)
    {
      printf("Valid Data interrupted on device %s  Port %d\r\n", m_DevName, m_RTUPort);
//      FTData_Error ftd_error;
//      ftd_error.SetData(nmea.GetData(), AFS_RTU_FEED_FAILED, m_RTUPort);
//      ftd_error.SendError();

      m_bInterruptSent = true;
      m_nInterrupts++;
    }
  }
  for (short i = 0; i < m_nDevices; i++)
  {
    if (m_RTUDevices[i] != NULL)
      m_RTUDevices[i]->ProcessEverySecond(false);  // false indicates not to check for port activity since is was already done
  }
}


//-------------------------------------------------------------------------
int RTUDevice::ProcessIncomingString()
{
  char buf[256], buf2[256];
  int len;
  len = read(m_fd, buf, 256);
  int arrChanIdx[] = {31, 49, 46, 1, 4, 10, 7, 22, 16, 43, 34, 37, 52, 13, 19, 40}; // where each chan starts in the 'U' string

  m_nBytesRcvd += len;

  if (len > 0)
  {
    m_nCharsRcvd += len;  // used to make sure the port is talking.
  }

  double ADCVal;

  m_Data.Add(buf, len);

  if ((len = m_Data.Find('U')) != -1)
  {
    if (len > 0)  // get rid of crap at the start.
      m_Data.Toss(len);

    if ((len = m_Data.GetLine(buf2, 127)) == -1)  // leave if there is no end-of-line
    {
      if (m_Data.GetNumElements() > 127) //if no EOL 127 after 'U' get rid of 'U'
        m_Data.Toss(1);

      return 0;
    }
    else if (buf2[2] == 'I') // 'Invalid command'
    {
      return 0;
    }
    else
    {
      buf2[len] = '\0';
    }

    // Ensure a complete record is available for decoding
    if (len != 66)
    {
      //printf("malformed[%d]%s\n", len, buf2);
      printf(".");
      return 0;
    }

    for (short i = 0; i < 16; i++)  // check each analog signal
    {
      ADCVal = ConvertHex(&buf2[arrChanIdx[i]]) / 111.7;
      m_Chans[i].Add(ADCVal);
    }

    return 1;
  }
  return 0;
}

double RTUDevice::GetChanVoltage(int chan)
{
  if (chan < 0 || chan > 16)
    return -1;

  return m_Chans[chan].GetAverage();
}

//-------------------------------------------------------------------------------------------------
void RTUDevice::SelfTest()
{
  char buf[24];
  sprintf(buf, "Testing %s", m_DevName);
  usleep(250000);

  GetVFD().CenterScreen(buf, "Please Wait");

  CSelect sel;
  AFS_Timer t1;
  sel.Add(m_fd);
  t1.SetTime();

  while (t1.DiffTimeMS() < 2000)
  {
    if (sel.Select())
    {
      if (sel.HasData(m_fd) )
      {
        if (ProcessIncomingString() == 1)
        {
          selftest = DRVR_DATA_OK;
          return;
        }
        usleep(10000);
      }
    }
  }

  selftest = DRVR_NO_DATA;
  return;
}
//  this converts the hex signal from the RTU to an unsigned long number.
//
unsigned long  RTUDevice::ConvertHex(char *buf)
{
  char c;
  unsigned long ret = 0;
  for (short i = 0; i < 3; i++)
  {
    if (buf[i] >= '0' && buf[i] <= '9')
      c = buf[i] - '0';
    else if (buf[i] >= 'A' && buf[i] <= 'F')
      c = buf[i] - 'A' + 10;
    else if (buf[i] >= 'a' && buf[i] <= 'f')
      c = buf[i] - 'a' + 10;
    else
      c = 0;

    ret = ret * 16 + c;
  }

  return ret;
}

//-------------------------------------------------------------------------
//
//  Setup
//
void RTUDevice::Setup()
{
  for (short i = 0; i < m_nDevices; i++)
  {
    if (m_RTUDevices[i] != NULL)
    {
      GetVFD().DisplayInfo(m_RTUDevices[i]->m_DevName);  // displays device name and OK
      m_RTUDevices[i]->Setup();
    }
	else
	  printf("Skipping device %d\r\n", i);
  }
  printf("Leaving Setup from base class\r\n");
}


void RTUDevice::UpdateFileNames()
{
  for (short i = 0; i < m_nDevices; i++)
  {
    if (m_RTUDevices[i] != NULL)
      m_RTUDevices[i]->UpdateFileNames();
  }
}


bool RTUDevice::VerifyParms()  //return true if the parms are valid.
{
  bool ret = true;

  for (short i = 0; i < m_nDevices; i++)
  {
    if (m_RTUDevices[i] != NULL)
      ret &= m_RTUDevices[i]->VerifyParms();
  }

  return ret;
}

void RTUDevice::DumpParms()  //return true if the parms are valid.
{
  for (short i = 0; i < m_nDevices; i++)
  {
    if (m_RTUDevices[i] != NULL)
      m_RTUDevices[i]->DumpParms();
  }
}

