using namespace std;
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include "J2K.h"

#define AFS_DEBUG_FILE ("/media/card/FASTTrac/debug.txt")
#define AFS_DEBUG_HEX_FILE ("/media/card/FASTTrac/debug.hex")

void WriteDebug(const char *src, const int line)
{
  FILE *fp;

  if ( (fp = fopen (AFS_DEBUG_FILE, "a+")) != NULL)
  {
    fprintf(fp, "%s:%d\r\n", src, line);
    fclose(fp);
  }
  syslog(LOG_ERR, "%s:%d\r\n", src, line);
}

void WriteDebug(const char *src, const int line, const char *str)
{
  FILE *fp;

  if ( (fp = fopen (AFS_DEBUG_FILE, "a+")) != NULL)
  {
    J2K theTime;
    char strTime[32];
    theTime.SetSystemTime(); // sets J2K to the system time
    fprintf(fp, "%s:: ", theTime.GetTimeString(strTime, 32));
    fprintf(fp, "%s:%d  %s\n", src, line, str);
    syslog(LOG_ERR, "%s:%d  %s\r\n", src, line, str);
    fclose(fp);
  }
  else
  {
    syslog(LOG_ERR, "%s %d Unable to open %s! Errno = %d\n", src, line, AFS_DEBUG_FILE, errno);
  }
}

void WriteDebug(const char *src, const int line, const char *str, int ival)
{
  FILE *fp;

  if ( (fp = fopen (AFS_DEBUG_FILE, "a+")) != NULL)
  {
    J2K theTime;
    char strTime[32];
    theTime.SetSystemTime(); // sets J2K to the system time
    fprintf(fp, "%s:: ", theTime.GetTimeString(strTime, 32));
    fprintf(fp, "%s:%d  %s - %d\n", src, line, str, ival);
    syslog(LOG_ERR, "%s:%d  %s - %d\r\n", src, line, str, ival);
    fclose(fp);
  }
  else
    syslog(LOG_ERR, "%s %d Ival %d Unable to open debug.txt!\n", src, line, ival);
}

void WriteDebug(const char *src, const int line, const char *str, unsigned long ival)
{
  FILE *fp;

  if ( (fp = fopen (AFS_DEBUG_FILE, "a+")) != NULL)
  {
    J2K theTime;
    char strTime[32];
    theTime.SetSystemTime(); // sets J2K to the system time
    fprintf(fp, "%s:: ", theTime.GetTimeString(strTime, 32));
    fprintf(fp, "%s:%d  %s - %ld\n", src, line, str, ival);
    syslog(LOG_ERR, "%s:%d  %s - %ld\r\n", src, line, str, ival);
    fclose(fp);
  }
  else
    syslog(LOG_ERR, "%s %d Ival %ld Unable to open debug.txt!\n", src, line, ival);
}

void WriteDebug(const char *src, const int line, const char *str, const char *strval)
{
  FILE *fp;

  if ( (fp = fopen (AFS_DEBUG_FILE, "a+")) != NULL)
  {
    J2K theTime;
    char strTime[32];
    theTime.SetSystemTime(); // sets J2K to the system time
    fprintf(fp, "%s:: ", theTime.GetTimeString(strTime, 32));
    if (strlen(strval) > 0)
    {
      fprintf(fp, "%s:%d  %s - %s\n", src, line, str, strval);
      syslog(LOG_ERR, "%s:%d  %s - %s\r\n", src, line, str, strval);
    }
    else
    {
      fprintf(fp, "%s:%d  %s - ##Empty String!##\n", src, line, str);
      syslog(LOG_ERR, ":%s:%d  %s - ##Empty String!##\r\n", src, line, str);
    }

    fclose(fp);
  }
  else
    syslog(LOG_ERR, "%s %d %s %s Unable to open debug.txt!\n", src, line, str, strval);
}

void WriteDebug(const char *src, const int line, const char *str, double dval)
{
  FILE *fp;

  if ( (fp = fopen (AFS_DEBUG_FILE, "a+")) != NULL)
  {
    J2K theTime;
    char strTime[32];
    theTime.SetSystemTime(); // sets J2K to the system time
    fprintf(fp, "%s:: ", theTime.GetTimeString(strTime, 32));
    fprintf(fp, "%s:%d  %s - %.3f\n", src, line, str, dval);
    syslog(LOG_ERR, "%s:%d  %s - %.3f\r\n", src, line, str, dval);
    fclose(fp);
  }
  else
    syslog(LOG_ERR, "%s %d Dval %.3f Unable to open debug.txt!\n", src, line, dval);
}

void WriteDebugHex(const char *src, const int line, const char *data, short len)
{
  FILE *fp;

  if ( (fp = fopen (AFS_DEBUG_FILE, "a+")) != NULL)
  {
    J2K theTime;
    char strTime[32];
    theTime.SetSystemTime(); // sets J2K to the system time
    fprintf(fp, "%s:: ", theTime.GetTimeString(strTime, 32));
    fprintf(fp, "%s:%d (%d bytes) --> ", src, line, len);

    for (short i = 0; i < len; i++)
    {
      fprintf(fp, "%x ", data[i]);
    }
    fprintf(fp, "\n");

    fclose(fp);
  }
  else
    syslog(LOG_ERR, "%s %d Unable to open debug.txt!\n", src, line);
}
//---------------------------------------------------------------------------------
// writes to debug.hex file - just the chars that are being logged.
//
void WriteDebugHex2(const char *data, short len)
{
  FILE *fp;
  static unsigned short count = 0;
  static bool first = true;


  if ( (fp = fopen (AFS_DEBUG_HEX_FILE, "a+")) != NULL)
  {
    if (first)
    {
      J2K theTime;
      char strTime[32];
      theTime.SetSystemTime(); // sets J2K to the system time
      fprintf(fp, "\r\nStartup %s:: \r\n", theTime.GetTimeString(strTime, 32));
      first = false;
    }
    for (short i = 0; i < len; i++)
    {
      fprintf(fp, "%02x ", data[i]);
      if (++count == 16)
      {
        fprintf(fp, "\n");
        count = 0;
      }
    }

    fclose(fp);
  }
  else
    syslog(LOG_ERR, "Unable to open debug.hex!\n");
}


void EraseDebug()
{
  FILE *fp;

  if ( (fp = fopen (AFS_DEBUG_FILE, "w")) != NULL)
  {
    syslog(LOG_ERR, "Debug File erased\n");
    fclose(fp);
  }
  else
    syslog(LOG_ERR, "Unable to erase debug.txt!\n");
}

// CheckDebugFileSize - check to see if the file has gotten too big.  If it has
//              remove it and start a new one.
void CheckDebugFileSize()
{
  struct stat fileStat;

  stat(AFS_DEBUG_FILE, &fileStat);

  if (fileStat.st_size > 500000)
    remove(AFS_DEBUG_FILE);

  stat(AFS_DEBUG_HEX_FILE, &fileStat);

  if (fileStat.st_size > 500000)
    remove(AFS_DEBUG_HEX_FILE);
}

void WriteDebugString(const char *str)
{
  FILE *fp;

  if ( (fp = fopen (AFS_DEBUG_FILE, "a+")) != NULL)
  {
    fprintf(fp, "%s\n", str);
    syslog(LOG_ERR, "%s\n", str);
    fclose(fp);
  }
}

void WriteDebugString(const char *str, int ival)
{
  FILE *fp;

  if ( (fp = fopen (AFS_DEBUG_FILE, "a+")) != NULL)
  {
    fprintf(fp, "%s - %d\n", str, ival);
    syslog(LOG_ERR, "%s - %d\n", str, ival);
    fclose(fp);
  }
}

void WriteDebugString(const char *str1, const char *str2)
{
  FILE *fp;

  if ( (fp = fopen (AFS_DEBUG_FILE, "a+")) != NULL)
  {
    fprintf(fp, "%s - %s\n", str1, str2);
    syslog(LOG_ERR, "%s - %s\r\n", str1, str2);
    fclose(fp);
  }
}
void WriteDebugString(const char *str1, const char *str2, const char *str3 )
{
  FILE *fp;

  if ( (fp = fopen (AFS_DEBUG_FILE, "a+")) != NULL)
  {
    fprintf(fp, "%s - %s - %s\n", str1, str2, str3);
    syslog(LOG_ERR, "%s - %s - %s\r\n", str1, str2, str3);
    fclose(fp);
  }
}

