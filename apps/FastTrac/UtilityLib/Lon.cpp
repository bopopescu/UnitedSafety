/*=============================================================================
  Class:       LON

  Purpose:     a longitude class - verifies that all longitudes are between
               -180 to 180 degrees and provides an ndms output where
               n is W or E.

  Methods:

  History:     Dave Huff - May 1998 - original code.
=============================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include "utility.h"


/*=============================================================================
  Name:        LON::get_ndms

  Purpose:     returns a dms string prefaced by either 'N' or 'S'

  Parameters:  len(I) - maximum declared size of buf

  History:     Dave Huff - May 1998 - original code.
=============================================================================*/

char * LON::get_ndms
(
  char *buf,
  const short len,
  short ndec,
  const char delim
) const
{
  char tbuf[32];

  get_dms(tbuf, 32, ndec, delim);

  if (tbuf[0] == '-')
  {
    tbuf[0] = 'W';
    strncpy(buf, tbuf, len);
  }
  else
  {
    buf[0] = 'E';
    strncpy(&buf[1], tbuf, (short)(len - 1));
  }

  return buf;
}             /*End of LON::get_ndms */





/*=============================================================================
  Name:        LON::get_ndm

  Purpose:     returns a string containing dddmm.mmmm,N or dddcmm.mmmmN where
               dd is the degrees and mm.mmm is the minutes and c is a
               delimiter.  Typically used for NMEA strings

  Parameters:  buf (O) the character buffer to be filled
               len (I) - the maximum length of the buffer including '\0'
               ndec (I) - the number of decimals to output the string to
               delim (I) - the delimiter to be used

  Returns:     a pointer to the buffer

  History:     Dave Huff - May 1998 - original code.
               John Schleppe - Nov 2000 - modified for ,N or ,S for NMEA
=============================================================================*/

char* LON::get_ndm
(
  char *buf,
  const short len,
  const short ndec,
  const char delim
) const
{
  short d;
  double m;
  char tbuf[32];

  d = (short)(val * RAD_TO_DEG);
  m = fabs(((val * RAD_TO_DEG) - d) * 60.0);

  d = (short)fabs((double)d);

  if (val < 0.0)
  {
    if (delim)
      sprintf(tbuf, "%03d%c%02.*f,W", d, delim, ndec, m);
    else
      sprintf(tbuf, "%03d%0*.*f,W", d, ndec + 3, ndec, m);
  }
  else
  {
    if (delim)
      sprintf(tbuf, "%03d%c%02.*f,E", d, delim, ndec, m);
    else
      sprintf(tbuf, "%03d%0*.*f,E", d, ndec + 3, ndec, m);
  }

  tbuf[len - 1] = 0;
  strcpy(buf, tbuf);

  return buf;
}


char * LON::get_nDDMMmm
(
  char *buf,
  const short len,
  const char delim
) const
{
  short d;
  short m;
  char tbuf[32];

  d = (short)(val * RAD_TO_DEG);
  m = (short)(100 * (fabs(((val * RAD_TO_DEG) - d) * 60.0)));
  d = (short)fabs((double)d);
  char ns;
  ns = 'E';

  if (val < 0.0)
    ns = 'W';

  if (delim)
    sprintf(tbuf, "%c%c%03d%c%04d", ns, delim, d, delim, m);
  else
    sprintf(tbuf, "%c %03d %04d", ns, d, m);

  tbuf[len - 1] = 0;

  if ((short)strlen(tbuf) < len)
    strcpy(buf, tbuf);

  return buf;
}             /*End of LAT::get_ndms */



/*=============================================================================
  Name:        set_ndms

  Purpose:     sets the value to an ndms string to

  Parameters:  none.

  Discussion:

  History:     Dave Huff - May 1998 - original code.
=============================================================================*/

short LON::set_ndms
(
  const char *cdms
)
{
  char dms[32];
  strcpy(dms, cdms);
  char *p;

  if ((p = strchr(dms, 'E')) != NULL)
    *p = '+';
  else if ((p = strchr(dms, 'e')) != NULL)
    *p = '+';
  else if ((p = strchr(dms, 'W')) != NULL)
    *p = '-';
  else if ((p = strchr(dms, 'w')) != NULL)
    *p = '-';

  return set_dms(dms);
}             /*End of LON::set_ndms */
// set_ndm - set the value based on input string of N DD MMMM
short LON::set_ndm
(
  const char *cdms
)
{
  char dms[32];
  strcpy(dms, cdms);
  const char *p;
  short i = 0;
  short sign = 1;
  p = &cdms[0];

  while (p && i < 32)  // strip out the spaces
  {
    if (*p == 'w' || *p == 'W' || *p == '-')
    {
      sign = -1;
      p++;
      continue;
    }
    if (*p == 'e' || *p == 'E' || *p == '+')
    {
      sign = 1;
      p++;
      continue;
    }

    if (*p != ' ')
      dms[i++] = *p;
    p++;
  }
  int deg;
  double mn;
  deg = atol(dms) / 10000;
  mn = (double( atol(dms) - (deg * 10000) ) / 100.) /  60.0;

  val = (double(sign) * ((double)deg + mn)) * DEG_TO_RAD;

  return 1;
}             /*End of LON::set_ndm */



/*=============================================================================
  Name:        LON::operator ==

  Purpose:     determines equivalency of two LON objects

  Parameters:  g (I) - the lhs of the LON == LON statement

  Returns:     TRUE if they are equal

  History:     Dave Huff - July 1998 - original code.
=============================================================================*/

short LON::operator ==
(
  const LON &rhs
) const
{
  if (doubles_are_equal(val, rhs.get_rad(), 1.e-8))
    return 1;

  return 0;
}             /*End of LON::operator == */

/*=============================================================================
  Name:        TESTING routines

  Purpose:     tests the class - define _TEST_LAT_ to compile a console based
               executable.

  History:     Dave Huff - May 1998 - original code.
=============================================================================*/
#ifdef _TEST_LON_
#include "stdio.h"
void main()
{
  LON a, b;
  char buf[32];
  double rad;
  LON c(1.234);

  for (rad = 2 * TWOPI; rad < TWOPI + 1; rad += .01)
  {
    a.set_rad(rad);
    a.get_ndms(buf, 32);
    b.set_ndms(buf);
    if (doubles_are_equal(b.get_rad(), a.get_rad(), .00001))
      continue;
    printf("Error 1: set_ndms problem at %.9f\n", rad);
  }
  if (a.set_dm("-18955.00"))
    printf("angle dm set ok\n");
  if (a.set_rad(rad))
    printf("Error: invalid set_rad return\n");

  if (a.set_rad(TWOPI + .1))
    printf("Error2: invalid set_rad return\n");

  if (a.set_dd(-191.0))
    printf("Error2: invalid set_rad return\n");

  if (a.set_dd(399))
    printf("Error2: invalid set_rad returned\n");
  printf("a should be 39 - a is %.2f\n", a.get_dd());
}

#endif
