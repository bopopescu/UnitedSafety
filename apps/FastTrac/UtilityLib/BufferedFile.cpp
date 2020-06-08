#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "BufferedFile.h"
#include "AFS_Timer.h"

//-----------------------------------------------------------------------------
//
//
BufferedFile::BufferedFile()
{
  m_Name[0] = '\0';
  m_Buffer = NULL;  // allocate the buffer
  m_BufSize = 0;  // the size of the buffer
  m_CurBufSize = 0;
}

//-----------------------------------------------------------------------------
//
//
BufferedFile::~BufferedFile()
{
  Delete();  // delete any allocated space
}

//-----------------------------------------------------------------------------
// SetName - sets the name of the file.  This can be done independently of
//           setting the size so that file names can change
//
void BufferedFile::SetName(const char *name)
{
  if (strlen(name))
  {
    if (m_BufSize > 0)  // if there is anything in the current buffer we write it out.
      Flush();
    strcpy(m_Name, name);
  }
  else
    Delete();
}

//-----------------------------------------------------------------------------
// SetSize - sets the size to buffer to.  We allocate 10% more than this size
//           and flush the buffer whenever an Add would go beyond the size
//
void BufferedFile::SetSize(const unsigned short bufSize)
{
  if (m_BufSize > 0)
  {
    Flush();
    delete [] m_Buffer;
  }

  m_Buffer = (char *) new char [bufSize + bufSize / 10];

  if (m_Buffer == NULL)
    Delete();  //didn't work!
  else
    m_BufSize = bufSize;
}

//-----------------------------------------------------------------------------
//  Delete any allocated space and null out the name and buffer
//
void BufferedFile::Delete()
{
  if (m_BufSize > 0)
  {
    delete [] m_Buffer;
    m_BufSize = 0;
    m_Name[0] = '\0';
    m_Buffer = NULL;
    m_CurBufSize = 0;
  }
}


//-----------------------------------------------------------------------------
//  Flush the current contents - return number of bytes written.
//
unsigned short BufferedFile::Flush()
{
  if (m_BufSize == 0)  // unallocated - not set up yet!
    return 0 ;

  if (m_CurBufSize == 0)
    return 0;

  unsigned short ret = Write(m_Buffer, m_CurBufSize);
  m_CurBufSize = 0;
  return ret;
}
//-----------------------------------------------------------------------------
//  Write out the specified data
//
unsigned short BufferedFile::Write(const char *buffer, const short bufSize)
{
  FILE *fp;

  fp = fopen(m_Name, "a");

  if (fp == NULL)
    return 0;

  fwrite(buffer, 1, bufSize, fp);
  fclose(fp);

  sync();  // force everything out to disk

  return bufSize;
}

//-----------------------------------------------------------------------------
//  Add data to the buffer - write it out if the buffer is too big.
//
void BufferedFile::Add(const char *buffer)
{
  short bufSize = strlen(buffer);
  Add(buffer, bufSize);
}

//-----------------------------------------------------------------------------
//  Add data to the buffer - write it out if the buffer is too big.
//
void BufferedFile::Add(const char *buffer, const short len)
{
  if (m_BufSize == 0)  // unallocated - not set up yet!
    return;

  short bufSize = len;

  if (m_CurBufSize + bufSize > m_BufSize || m_LastWrite.DiffTime() > 10)  // writes out every ten seconds 
  {
    Flush();
    m_LastWrite.SetTime();
  }

  if (bufSize > m_BufSize)  // if current buffer is bigger than we can handle
    Write(buffer, bufSize);    // we just write it out directly
  else                         // Otherwise just buffer it.
  {
    memcpy(&m_Buffer[m_CurBufSize], buffer, bufSize);
    m_CurBufSize += bufSize;
  }
}



//-----------------------------------------------------------------------------
//  Write out the specified data
//
unsigned short KMLBufferedFile::Write(const char *buffer, const short bufSize)
{
  AFS_Timer theTime;
  theTime.SetTime();

  char cpBuf[512];
  sprintf(cpBuf, "cp %s /media/card/FASTTrack/GPStmp.kml", m_Name);
  system(cpBuf);

  FILE *fp;

  fp = fopen("/media/card/FASTTrack/GPStmp.kml", "a");

  if (fp == NULL)
  {
    fp = fopen(m_Name, "w+");

    if (fp == NULL)
    {
      return 0;
    }
  }

  fseek(fp, -19, SEEK_END);  // need to back up over </Folder> etc
  fwrite(buffer, 1, bufSize, fp);
  fprintf(fp, "</Folder>\r\n</kml>\r\n");
  fclose(fp);
  m_CurBufSize = 0;

  sync();  // force everything out to disk
  sprintf(cpBuf, "cp /media/card/FASTTrack/GPStmp.kml %s ", m_Name);
  system(cpBuf);
printf("KML write Time %.2f\n", (double)(theTime.DiffTimeMS()) / 1000.0);
  return bufSize;
}

//-----------------------------------------------------------------------------
//  Add data to the buffer - write it out if the buffer is too big.
//
void KMLBufferedFile::Add(const char *buffer)
{
  short bufSize = strlen(buffer);

  if (m_CurBufSize + bufSize > m_BufSize)
    Flush();

  if (bufSize > m_BufSize)  // if current buffer is bigger than we can handle
    Write(buffer, bufSize);    // we just write it out directly
  else                         // Otherwise just buffer it.
  {
    memcpy(&m_Buffer[m_CurBufSize], buffer, bufSize);
    m_CurBufSize += bufSize;
  }
}

//-----------------------------------------------------------------------------
//  Add data to the buffer in hex format- write it out if the buffer is too big.
//
void BufferedFile::AddHex(const char *buffer, const short len)
{
  if (m_BufSize == 0)  // unallocated - not set up yet!
    return;

  char hexBuf[16];
  
  for (short i = 0; i < len; i++)
  {
    sprintf(hexBuf, "%x ", buffer[i]);
    Add(hexBuf, strlen(hexBuf) );
  }
  Add("\r\n", 2);
}



