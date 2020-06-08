#include <math.h>
#include <stdio.h>
#include <string.h>

#include "checksum.h"

/*$-----------------------------------------------------------------------------
Name:        checksum


Purpose:     to compute NMEA checksum and compare against that in a string

Arguments:   buffer[] (I) - message that checksum is to be computed for

Description: computes NMEA checksum for given string and compares against
             the checksum found in the nmea message
             see NMEA 0183 standards for details on check sum computations.

Returns:     YES if string passes checksum test.
             NO  if no checksum was found.
             BADCHKSUM if a checksum is found, and it's bad.

History:     John B. Schleppe - March 11, 1991 - original.
             Paul B. Rown     - July, 1991 - modified to return BADCHKSUM.
             John B. Schleppe - June 1, 1994 - modifed to break on CR/LF
             Amour Hassan (ahassan@gps1.com) - June 17, 2014 - No longer modifies "buffer"
----------------------------------------------------------------------------- */
CHECKSUM::CS_STATES CHECKSUM::is_valid
(
  const char *buffer
)
{
  unsigned char check_sum = 0;
  const char *asteric;

  if ((asteric = strrchr(buffer, '*')) == NULL)      /* no checksum to check */
    return NO_CHECKSUM;

  const int upper = atohex(*(asteric+1));
  const int lower = atohex(*(asteric+2));

  if (upper == -1 || lower == -1)
    return BAD_CHECKSUM;

  check_sum ^= lower & 0xff;
  check_sum ^= (upper<<4) & 0xff;

 int len = asteric - buffer;

  while(len--)
  {
    check_sum ^= *(buffer++);
  }

  return (check_sum == '\0') ? VALID_CHECKSUM : BAD_CHECKSUM;
}



/*$----------------------------------------------------------------------------
  Name:        atohex

  Purpose:     converts a character to a hex digit

  Parameters:  c (I) - the input character

  Discussion:

  History:     Dave Huff - Jan. 30, 1995 - original code.
-----------------------------------------------------------------------------*/

char CHECKSUM::atohex
(
  char c
)
{
  if (c >= '0' && c <= '9')
    return c - '0';

  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;

  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;

  return -1;
}             /*End of atohex */





/*=============================================================================
  Name:        CHECKSUM::add_checksum

  Purpose:     

  Parameters:  

  Returns:     

  History:     Dave Huff - April 1999 - original code.
=============================================================================*/

bool CHECKSUM::add_checksum
(
  char *buf,
  short len
)
{
  short i, blen;
  char check_sum = 0;

  blen = strlen(buf);

  if (blen > len - 4)
    return false;

  for (i = 0; i < blen; i++)
  {
    check_sum ^= buf[i];
  }

  sprintf(&buf[blen], "*%1X%1X\r\n",
                (check_sum >> 4) & 0x0fu, check_sum & 0x0fu);

  return true;
}             /*End of CHECKSUM::add_checksum */





/*=============================================================================
  Name:        CHECKSUM::remove_checksum

  Purpose:     

  Parameters:  

  Returns:     

  History:     Dave Huff - April 1999 - original code.
=============================================================================*/

void CHECKSUM::remove_checksum
(
  char *buf
)
{
  char *asteric;

  if ((asteric = strrchr(buf, '*')) != NULL)
    *asteric = '\0';
}             /*End of CHECKSUM::remove_checksum */

/*=============================================================================
  Name:        CHECKSUM::add_checksum

  Purpose:     add the checksum to a string

  Parameters:  

  Returns:     

  History:     Dave Huff - Dec 2013 - original code.
=============================================================================*/
void CHECKSUM::add_checksum(ats::String &buf)
{
  short i, blen;
  char check_sum = 0;

  blen = buf.length();

  for (i = 0; i < blen; i++)
  {
    if (i ==0 && buf[i] == '$') // ignore $ if first char
      continue;
  
    check_sum ^= buf[i];
  }
  char tmp[8];
  sprintf(tmp, "*%1X%1X\n\0", (check_sum >> 4) & 0x0fu, check_sum & 0x0fu);

  buf += tmp;

}
