
#include "DeviceBase.h"
#include "WriteDebug.h"
#include "AFF_IPC.h"

// predefine the statics

DeviceBase::DeviceBase()
{
  m_nmea = new NMEA_Client();
  m_RawFile = new BufferedFile();
  m_aff = new AFF_IPC();

  mybIsTesting = false;
  m_pMyEvent = NULL;
  selftest = DRVR_UNKNOWN;
  strcpy(m_DevName, "None");
  m_RawFile->SetSize(4000);

  strcpy(m_ParmFileName, "None.prm");  //!< Parameter file stuff - Names defined in CTOR
  strcpy(m_CSVFileName, "None.csv");   //!< Log file stuff - names defined in CTOR
  strcpy(m_RawFileName, "None.raw");   //!< Log file for all incoming data (serial) or voltage changes (RTUs)
  strcpy(m_KMLFileName, "None.kml");
}

DeviceBase::~DeviceBase()
{
  delete m_aff;
  delete m_RawFile;
  delete m_nmea;
}


const char* DeviceBase::GetName() const  // all types set a DevNam in construction
{
  return m_DevName;
}




//-------------------------------------------------------------------------
void DeviceBase::WriteLogFile()
{
  if (m_pMyEvent == NULL)
    return;

  char buf[512];
  InsertLogHeader(m_pMyEvent->GetCSVHeader(buf));  // insert column titles into a new file

  FILE *fp;

  if ( (fp = fopen (m_CSVFileName, "a")) != NULL)
  {
    m_pMyEvent->WriteCSVPositionData1(fp);
    m_pMyEvent->WriteLogRecord(fp);
    m_pMyEvent->WriteCSVPositionData2(fp);
    fclose(fp);
  }
  else
    WriteDebug(__FILE__, __LINE__, "Unable to open log file", m_CSVFileName);

  sync();  // need to make sure it writes out.
}

void DeviceBase::WriteKMLFile()
{
}




void DeviceBase::UpdateFileNames()
{
}

//-------------------------------------------------------------------------------------------------
// InsertLogHeader
//  insert the column titles on a new log file
//
void DeviceBase::InsertLogHeader(const char *strHeader)
{
  int ret = access(m_CSVFileName, F_OK);

  if (ret < 0)
  {
    FILE *fp;

    fp = fopen(m_CSVFileName, "w");

    if ( fp == NULL )
      return;

    fprintf(fp, "%s", strHeader);
    fclose(fp);
  }
}
//-------------------------------------------------------------------------------------------------
// InsertKMLHeader
//  insert the column titles on a new log file
//
void DeviceBase::InsertKMLHeader()
{
  int ret = access(m_KMLFileName, F_OK);

  if (ret < 0)
  {
    FILE *fp;

    fp = fopen(m_KMLFileName, "w");

    if ( fp == NULL )
      return;

    fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n");
    fprintf(fp, "<kml xmlns=\"http://earth.google.com/kml/2.0\">\r\n");
    fprintf(fp, "<Folder>\r\n");
    fprintf(fp, "</Folder>\r\n</kml>\r\n");
    fclose(fp);
  }
}

void DeviceBase::SelfTest()
{
  if (strcmp(m_DevName, "None"))
  {
    WriteDebug(__FILE__, __LINE__, "SelfTest has not been declared for this device driver", m_DevName);
    selftest = DRVR_DIS;
  }
}


/*!
 * \brief display the results of the device self test
 */
void DeviceBase::DisplayStatus()
{
  char buf[32];
  char condition[32];
  enum STATUS{PASSED, FAILED, UNKNOWN};
  STATUS status;

  GetStatusString(condition);

  switch (selftest)
  {
    //Passing tests
  case DRVR_DIS:
  case DRVR_INIT:
  case DRVR_UNKNOWN:
    status = UNKNOWN;
    break;

  case DRVR_DATA_OK:
    status = PASSED;
    break;


    //Failing tests
  case DRVR_FAILED:
  case DRVR_NO_DATA:
  case DRVR_RXING:
  default:
    status = FAILED;
    break;
  }

  if (status == PASSED)
    sprintf(buf, "%s: passed", m_DevName);
  else if (status == FAILED)
    sprintf(buf, "%s: failed", m_DevName);
  else
    sprintf(buf, "%s: unknown", m_DevName);

}

void DeviceBase::GetStatusString(char *condition)
{
  switch (selftest)
  {
    //Passing tests
  case DRVR_DIS:
    strcpy(condition, "Unassigned");
    break;

  case DRVR_INIT:
    strcpy(condition,"Not Started");
    break;

  case DRVR_DATA_OK:
    strcpy(condition, "Good Data");
    break;

  case DRVR_UNKNOWN:
    strcpy(condition, "No Test");
    break;

    //Failing tests
  case DRVR_FAILED:
    strcpy(condition, "Not Working");
    break;

  case DRVR_NO_DATA:
    strcpy(condition, "Nothing received");
    break;

  case DRVR_RXING:
    strcpy(condition, "Unknown Data on port");
    break;

  default:
    strcpy(condition, "No Test Defined");
    break;
  }
}



bool DeviceBase::ProcessIncomingData(){return false;}  // process the RTU input, return 1 if there was an event

//-----------------------------------------------------------
// Setup - set up the device - the base class calls the
// reset log function and the self test function.
//
void  DeviceBase::Setup()
{
//  Setup_ResetLogFile();
//  Setup_SetTesting();
//  Setup_DoSelfTest();
}

//-----------------------------------------------------------------------------
// asks if you want to reset the log file - ret 1 if yes
//
void DeviceBase::Setup_ResetLogFile()
{
}

void DeviceBase::Setup_DoSelfTest()
{
}

void DeviceBase::Setup_SetTesting()
{
  mybIsTesting = false;
}

/*!
 *  ReadParms
 *  Read the number of bytes to the pointer specified
 *  return true if the file was opened and read.
 */
bool DeviceBase::ReadParms
(
  int fSize,
  void *pData
)
{
  // read file from disk
  if (strlen(m_ParmFileName) == 0)  //from cold start
  {
    UpdateFileNames();
  }

  struct stat statBuf;

  if(0 == stat(m_ParmFileName, &statBuf))
  {

    if (fSize > statBuf.st_size)
    {
      char buf[256];
      snprintf(buf, sizeof(buf) - 1, "ReadParms - ##%s## prm file is wrong size\nSize is %d (should be %d)\r\n", m_ParmFileName, (int)statBuf.st_size, fSize);
      buf[sizeof(buf) - 1] = '\0';
      WriteDebug(__FILE__, __LINE__, buf);
      return false;
    }
  }

  FILE *fp;
  fp = fopen(m_ParmFileName, "rb");

  if (fp)
  {
    if (fread(pData, fSize, 1, fp) != 1)
      WriteDebug(__FILE__, __LINE__, "ReadParms unable to read the parms file");
    fclose(fp);
    return true;
  }

  return false;
}

/*!
 * WriteParms
 * Write the number of bytes from the pointer specified
 */
void DeviceBase::WriteParms
(
  int fSize,
  void *pData
)
{
  if (strlen(m_ParmFileName) == 0)
    UpdateFileNames();

  if (strstr(m_ParmFileName, "None.prm") != NULL)
    return;

  if (strcmp(m_DevName, "None") == 0)
    return;

  FILE *fp;
  fp = fopen(m_ParmFileName, "wb");

  if (fp)
  {
    fwrite(pData, fSize, 1, fp);
    fclose(fp);
  }
}

bool DeviceBase::VerifyParms(){return true;}  //return true if the parms are valid.



//-------------------------------------------------------------------
// Send Event - encodes the event, Sends it to the AFF and
//              writes it to the KML and CSV file
void DeviceBase::SendEvent()
{
  char AFFbuf[MAX_AFF_BUF_SIZE];
  const short len = m_pMyEvent->Encode(AFFbuf);
  GetAFF().SendData(AFFbuf, len);

  #if 0
  REDSTONE FIXME:	Should logging with limited memory usage be done? Or should no
			logging be done at all?
			[Amour Hassan - September 11, 2012]
  // Now write the CSV File
  WriteLogFile();
  WriteKMLFile();
  #endif
}

//------------------------------------------------------------------
// Get Event Buffer - encodes the event, and passes the buffer 
//                    through the passed in pointer. Returns the 
//                    number of bytes written and false on failure
short DeviceBase::GetEventBuf(char *p_buf, short p_maxLen)
{
  if(p_buf == 0)
  {
    return -1;
  }
  
  if(p_maxLen < 128)
  {
    return -1;
  }

  const short len = m_pMyEvent->Encode(p_buf);

  return len;
}

bool DeviceBase::IsLoggingRawData() const {return false;}
