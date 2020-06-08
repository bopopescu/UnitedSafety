/*!
 * \file AFFDevice.cpp
 *
 * \brief AFF Device base class
 *
 *  common to all devices is the DevName (max 16 chars)
 *  and an ability to verify initial connections and decode the data.
 *  GPS data is added by AddGPS.
 *
 *  The AFF device is common for all subclasses and is used by each AFF
 *  to send event data and to read the raw NMEA stream if necessary.
 *  The AFF device contains the latest GPS position data and time as well.
 *
 *  Some AFFs will require a 2 plug system to work.  The Latitude system for
 *  example will require one plug for the AFF messages and another acting as
 *  an NMEA input on an RTU port.
 *
 *  RTUs can be a source of NMEA data - only the first NMEA source on the
 *  ports will be used!  The main loop will access the NMEA source RTU first
 *  and buffer the raw data and process the GPS data so that all of the other
 *  RTUs can access it.  If an external
 *  NMEA source is found the AFF data will
 *  not be used for GPS.
 *
 * \author David Huff
 * \date   24/08/2010
 *
 * Copyright &copy; Absolute Fire Systems (2010)
 */

#include "FASTLib_common.h"

#include "AFFDevice.h"
#include "tinyxml.h"


AFFDevice::AFFDevice()
{
  strcpy(m_DevName, "None");
  m_isSending = false;
  m_fdAFF = -1;
  myLenUUBuf = 0;
  m_bHasNMEA = false; // assumes that the device doesn't output NMEA to us
  m_cmdBuf.SetSize(256);
}

AFFDevice::~AFFDevice()
{
  UUFree(); // free any uuencoded buffers if they are alloced.

  // close the port
  if (m_fdAFF != -1)
    close (m_fdAFF);
}

//-----------------------------------------------------------------------------
// asks if you want to reset the log file - ret 1 if yes
//
int AFFDevice::ResetLogFile()
{
  return 0;
}

bool AFFDevice::VerifyParms(){return true;}  //return 1 if the connection seems to work.

void AFFDevice::SetPortBaud(speed_t speed)
{
  if (m_fdAFF == -1)
    return;

  fcntl(m_fdAFF, F_SETFL, 0);

  struct termios options;

  tcgetattr(m_fdAFF, &options);  // Get the current options for the port...

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
  //Hard coding for talking to SkyTrac.  Figure it all out later.
  options.c_iflag = 1;
  options.c_oflag = 0;
  options.c_lflag = 0;
  tcsetattr(m_fdAFF, TCSANOW, &options);
  tcgetattr(m_fdAFF, &options);  // Get the current options for the port...

}

////////////////////////////////////base64 encoding/////////////////////////////////
//
// Ref: http://www.koders.com/c/fidDF059C42D2FEC30FDCBF3363492CEE8F24ED5252.aspx#L66
// base64 encoding table
// static const char cb64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/-";
////////////////////////////////////////////////////////////////////////////////////

/*!
 * Return TRUE if 'c' is a valid base64 character, otherwise FALSE
 */
static int is_base64(char c)
{
  if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
     (c >= '0' && c <= '9') || (c == '+')             ||
     (c == '/')             || (c == '='))
  {
    return TRUE;
  }

  return FALSE;
}

/*!
 * Base64 encode one byte
 */
static char encode(unsigned char u)
{
  if(u < 26)  return 'A'+u;
  if(u < 52)  return 'a'+(u-26);
  if(u < 62)  return '0'+(u-52);
  if(u == 62) return '+';
  return '/';
}

/*!
 * Base64 encode and return size data in 'src'.
 *
 * \param size The size of the data in src
 * \param src The data to be base64 encode
 * \return encoded string otherwise NULL
 */
short AFFDevice::encode_base64(unsigned char *src, short len)
{
  int i;
  char *p;

  if(!src)
    return 0;

  if(!len)
    len= strlen((char *)src);

  UUAlloc(len);
  p = mypUUBuf;

  for(i=0; i<len; i+=3)
  {
    unsigned char b1=0, b2=0, b3=0, b4=0, b5=0, b6=0, b7=0;

    b1 = src[i];
    if(i+1<len)
      b2 = src[i+1];
    if(i+2<len)
      b3 = src[i+2];
    b4= b1>>2;
    b5= ((b1&0x3)<<4)|(b2>>4);
    b6= ((b2&0xf)<<2)|(b3>>6);
    b7= b3&0x3f;

    *p++= encode(b4);
    *p++= encode(b5);

    if(i+1<len)
    {
      *p++= encode(b6);
    }
    else
    {
      *p++= '=';
    }

    if(i+2<len)
    {
      *p++= encode(b7);
    }
    else
    {
      *p++= '=';
    }
  }
  return (p - mypUUBuf);
}

/*!
 * Decode a base64 character
 */
static unsigned char decode(char c)
{

  if(c >= 'A' && c <= 'Z') return(c - 'A');
  if(c >= 'a' && c <= 'z') return(c - 'a' + 26);
  if(c >= '0' && c <= '9') return(c - '0' + 52);
  if(c == '+')             return 62;

  return 63;
}


/*!
 * Decode the base64 encoded string 'src' into the memory pointed to by
 * 'dest'. The dest buffer is <b>not</b> NUL terminated.
 *
 * \param dest Pointer to memory for holding the decoded string.
 * Must be large enough to recieve the decoded string.
 * \param src A base64 encoded string.
 * \return TRUE (the length of the decoded string) if decode
 * succeeded otherwise FALSE.
 */
short AFFDevice::decode_base64(const char *src, const short len, unsigned char *dest)
{
  if(src && *src)
  {
    unsigned char *p= dest;
    int k, l;

    l = strlen(src)+1;
    UUAlloc(l);  //this will be longer than needed (by about 4/3)
    char *buf = mypUUBuf;

    /* Ignore non base64 chars as per the POSIX standard */
    for(k=0, l=0; src[k]; k++)
    {
      if(is_base64(src[k]))
      {
        buf[l++]= src[k];
      }
    }

    for(k=0; k<l; k+=4)
    {
      char c1='A', c2='A', c3='A', c4='A';
      unsigned char b1=0, b2=0, b3=0, b4=0;

      c1= buf[k];
      if(k+1<l)
      {
        c2= buf[k+1];
      }

      if(k+2<l)
      {
        c3= buf[k+2];
      }

      if(k+3<l)
      {
        c4= buf[k+3];
      }

      b1= decode(c1);
      b2= decode(c2);
      b3= decode(c3);
      b4= decode(c4);

      *p++=((b1<<2)|(b2>>4) );

      if(c3 != '=')
      {
        *p++=(((b2&0xf)<<4)|(b3>>2) );
      }

      if(c4 != '=')
      {
        *p++=(((b3&0x3)<<6)|b4 );
      }
    }

    return(p-dest);
  }

  return FALSE;
}

////////////////////////end of base64///////////////////////////////////


//-------------------------------------------------------------------
//  UUEncode - encode the original byte stream into 'printable ASCII'
//
//  buf (I) - the original byte stream
//  len (I) - the length of buf
//
//  Access the encoded buffer via GetUUString
//
//  return length of encodeBuf
//
//
short AFFDevice::UUEncode
(
  unsigned char *buf,
  const short len
)
{
  return encode_base64(buf, len);

  char p1, p2, p3;
  char c1, c2, c3, c4;
  short idx = 0;

  UUAlloc(len);

  for (int i = 0; i < len; i+=3)
  {
    p1 = buf[i];

    if (i + 3 < len)
    {
      p2 = buf[i + 1];
      p3 = buf[i + 2];
    }
    else  // need to pad either last char or last 2 chars
    {
      p2 = ' ';
      p3 = ' ';

      if (i + 1 < len)
        p2 = buf[i + 1];
    }

    c1 = (p1 >> 2) & 0x3F;
    c2 = ((p1 << 4) & 0x30) | ( ((p2 >> 4) & 0x0F) & 0x3F );
    c3 = ((p2 << 2) & 0x3C) | ( ((p3 >> 6) & 0x03) & 0x3F );
    c4 = p3 & 0x3F;

    mypUUBuf[idx++] = c1 + ' ';
    mypUUBuf[idx++] = c2 + ' ';
    mypUUBuf[idx++] = c3 + ' ';
    mypUUBuf[idx++] = c4 + ' ';
    mypUUBuf[idx] = '\0';
  }

  return idx;
}



void AFFDevice::UUFree()
{
  if (myLenUUBuf > 0)
  {
    delete []mypUUBuf;
    myLenUUBuf = 0;
    mypUUBuf = NULL;
  }
}

// return the pointer to the UU encoded string
//   Use this like so:
//  strcpy(buf, GetUUString());
// don't use the string directly cause it might disappear
//
const char * AFFDevice::GetUUString()
{
  return mypUUBuf;
}

short AFFDevice::UUAlloc(const short lenData)
{
  UUFree(); // get rid of whatever is there

#if UUENCODE
  if (lenData % 3 == 0)
    myLenUUBuf = lenData / 3 * 4;
  else
    myLenUUBuf = (lenData / 3 + 1 ) * 4;
#else
  myLenUUBuf = lenData * 4/3 + 4;  //for base64 encoding
#endif

  mypUUBuf = new char [myLenUUBuf];

  if (mypUUBuf == NULL)
    WriteDebug(__FILE__, __LINE__, "FAILED to allocate data for uuencode");

  return myLenUUBuf;
}



// Send bytes out as a message
//
bool AFFDevice::SendData(const char *data, short lenData)
{

  AFFMessage msg;
  BuildMessageFrame(data, lenData, msg);

  WriteDebug(__FILE__, __LINE__, "Adding message to queue - SeqNum is ", (short)(unsigned char)data[2]);
  WriteDebug(__FILE__, __LINE__, "       messages in queue ", (unsigned long)m_OutgoingMessages.GetNumElements());
  return m_OutgoingMessages.Add(msg);
}

/*!
 *  ReadParms
 *  Read the number of bytes to the pointer specified
 *  return true if the file was opened and read.
 */
bool AFFDevice::ReadParms
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
  stat(m_ParmFileName, &statBuf);

  if (fSize > statBuf.st_size)
  {
    printf("ReadParms - ##%s## prm file is wrong size\nSize is %d (should be %d)\r\n", m_ParmFileName, (int)statBuf.st_size, fSize);
    return false;
  }

  FILE *fp;
  fp = fopen(m_ParmFileName, "rb");

  if (fp)
  {
    if (fread(pData, fSize, 1, fp) != 1)
      WriteDebug(__FILE__, __LINE__, "ReadParms:Unable to read parm file ", m_ParmFileName );
    fclose(fp);
    return true;
  }

  return false;
}

/*!
 * WriteParms
 * Write the number of bytes from the pointer specified
 */
void AFFDevice::WriteParms
(
  int fSize,
  void *pData
)
{
  if (strlen(m_ParmFileName) == 0)
    UpdateFileNames();

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

/*!
 * \brief Write parms as XML
 */

void AFFDevice::WriteParms(const char* commentField)
{
  //Assemble common header for XML file
  TiXmlDocument doc;
  TiXmlComment * comment;
  string s;
  TiXmlDeclaration* decl = new TiXmlDeclaration( "1.0", "", "" );
  doc.LinkEndChild( decl );

  TiXmlElement * root = new TiXmlElement("FASTTrack");
  doc.LinkEndChild( root );

  comment = new TiXmlComment();
  comment->SetValue(commentField);
  root->LinkEndChild( comment );

  //Get driver to format it's own data
  xmlEncode(root);

  //Write to disk
  if (strlen(m_ParmFileName) == 0)
    UpdateFileNames();  //TODO this will need changing for xml extension eventually
  doc.SaveFile(m_ParmFileName);
}


/*!
 * \brief Read parms from XML file
 * \note Look for prm file if no XML exists
 * \todo delete prm file
 */

bool AFFDevice::ReadParms()
{
  // read file from disk
  if (strlen(m_ParmFileName) == 0)  //from cold start
  {
    UpdateFileNames();
  }

  TiXmlDocument doc(m_ParmFileName);
  if (!doc.LoadFile())
  {
    WriteDebug(__FILE__, __LINE__,"Failed to open %s\n", m_ParmFileName);
    return 0;
  }

  //Begin parsing the common part of the document
  TiXmlHandle hDoc(&doc);
  TiXmlElement* pElem;
  TiXmlHandle hRoot(0);

  pElem=hDoc.FirstChildElement().Element();
  // should always have a valid root but handle gracefully if it does
  if (!pElem)
  {
    WriteDebug(__FILE__, __LINE__,"Malformed Parameter file\n");
    return 0;
  }

  // get driver to parse its own data
  hRoot=TiXmlHandle(pElem);
  xmlDecode(&hRoot);

  return 1;
}
/*!
 * \brief display the results of the device self test
 */
void AFFDevice::DisplayStatus()
{
  char buf[32];
  char condition[32];
  GetStatusString(condition);

  snprintf(buf, sizeof(buf) - 1, "%s: unknown", m_DevName);
  buf[sizeof(buf) - 1] = '\0';
}

void AFFDevice::GetStatusString(char *condition)
{
  strcpy(condition, "No Test Defined");
}

