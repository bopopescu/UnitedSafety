#pragma once
#include "AFS_Timer.h"

class BufferedFile
{
protected:
  char m_Name[256];
  char *m_Buffer;  // allocate the buffer
  unsigned short m_BufSize;  // the size of the buffer
  unsigned short m_CurBufSize;  // the amount of chars in the pending buffer
  AFS_Timer m_LastWrite;  // last time the disk was flushed
  
public:
  BufferedFile();
  virtual ~BufferedFile();
  void SetName(const char *name);
  void SetSize(const unsigned short bufSize);

  virtual void Add(const char *buffer);
  virtual void Add(const char *buffer, const short len);
  virtual void AddHex(const char *buffer, const short len);  // writes out buffer as hex chars.
  unsigned short Flush();  // write out the contents of the buffer now.

protected:
  void Delete();
  virtual unsigned short Write(const char *buffer, const short bufSize);

};

class KMLBufferedFile : public BufferedFile
{
public:
  void Add(const char *buffer);
  unsigned short Write(const char *buffer, const short bufSize);
};

