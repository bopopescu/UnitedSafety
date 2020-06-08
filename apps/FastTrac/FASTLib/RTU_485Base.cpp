#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>

#include "RTU_485Base.h"
#include "CSelect.h"
//oldint arrChanIdx[] = {52, 31, 10, 13, 4, 1, 49, 19, 46}; // where each chan starts in the 'U' string
//            Pin    A   B   C  E  F   G  H   J   K   L   M   N   P   S   T   U
int arrChanIdx[] = {31, 49, 46, 1, 4, 10, 7, 22, 16, 43, 34, 37, 52, 13, 19, 40}; // where each chan starts in the 'U' string




//-------------------------------------------------------------------
RTU_485Base::RTU_485Base()
{
  strcpy(m_DevName, "485Base");
  m_fd = -1;
  nCharsRcvd = 0;

  for (short i = 0; i < NUM_PINS; i++)
  {
    myHandlers[i] = NULL;
    m_RawTimer[i].SetTime();
    m_LastV[i] = 0.0;
  }
  m_nBytesSent = 0;
  m_nBytesRcvd = 0;
  selftest = DRVR_INIT;
  m_RawRateInterval = 500;
  m_bIsLogging = false;  // not logging data.
}

//-------------------------------------------------------------------
RTU_485Base::~RTU_485Base()
{
  selftest = DRVR_DIS;
}

//-------------------------------------------------------------------
// setup and open the port
void RTU_485Base::SetPort
(
  short RTUPort
)
{
  SerDevice::SetPort(RTUPort, 5);
  m_bStartup = true;
  m_Timer.SetTime();
  selftest = DRVR_NO_DATA;
}

//-------------------------------------------------------------------
// process the RTU input, return 1 if there was an event
//
// looks for chars on the port and sends them to the AFF
//---------------------------------------------------------------------------
short RTU_485Base::ProcessIncomingData(AvgChan *avgChan)
{
  for (short i = 0; i < NUM_PINS; i++)
  {
    if (myHandlers[i] != NULL)
    {
      double voltage = avgChan[i].GetAverage();
      (this->*myHandlers[i])(i, voltage);

      if(IsLoggingRawData())
      {
        LogVoltageChange(i, voltage);
      }
  	}
  }
  return 1;
}

void  RTU_485Base::ProcessEverySecond(bool checkData)
{
  if (!checkData)
    return;

  if (m_Timer.DiffTimeMS() > 10000)
  {
    m_Timer.SetTime();

    if (nCharsRcvd == 0)
    {
      GetVFD().DrawLine(1, "NO RTU Data!");
      GetVFD().DrawLine(2, m_DevName);
    }

    nCharsRcvd = 0;
  }
}


//-------------------------------------------------------------------------
int RTU_485Base::ProcessIncomingString()
{
  char buf[256], buf2[256];
  int len;
  len = read(m_fd, buf, 256);

  m_nBytesRcvd += len;

  if (len > 0)
  {
    nCharsRcvd += len;  // used to make sure the port is talking.
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
      if (myHandlers[i] != NULL)
      {
        ADCVal = ConvertHex(&buf2[arrChanIdx[i]]) / 111.7;
        m_Chans[i].Add(ADCVal);
      }
    }
    return 1;
  }
  return 0;
}

double RTU_485Base::GetChanVoltage(int chan)
{
  if (chan < 0 || chan > 16)
    return -1;

  return m_Chans[chan].GetAverage();
}

//-------------------------------------------------------------------------------------------------
void RTU_485Base::SelfTest()
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
unsigned long  RTU_485Base::ConvertHex(char *buf)
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

//---------------------------------------------------------------------
// LogVoltageChange - log the time, channel, voltage
// to the file m_RawFileName
//
void RTU_485Base::LogVoltageChange(short chan, double voltage)
{
  if (fabs(m_LastV[chan] - voltage) > 0.2 && m_RawTimer[chan].DiffTimeMS() > m_RawRateInterval)
  {
    m_LastV[chan] = voltage;
    m_RawTimer[chan].SetTime();
    char buf[32], buf2[32];
    char pins[24] = {"ABCEFGHJKLMNPSTU"};
    GetNMEA().GetTimeStr(buf, 32);
    sprintf(buf2, "%s,%d,%c,%.1f\r\n", buf, chan, pins[chan], voltage);
    GetRawFile().Add(buf2);
  }
}
