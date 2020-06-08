/*=============================================================================
  Class:       LAT

  Purpose:     a latitude class - verifies that all lats are between
               -90 to 90 degrees and provides an ndms output where
               n is N or S.

  Methods:

  Discussion:  LAT  - The class constructor
               ~LAT - The class destructor.

  History:     Dave Huff - May 1998 - original code.
=============================================================================*/

#include "stdio.h"
#include "stdlib.h"
#include "utility.h"


/*=============================================================================
  Name:        LAT::get_ndms

  Purpose:     returns a dms string prefaced by either 'N' or 'S'

  Parameters:  len(I) - the total declared size of buf

  History:     Dave Huff - May 1998 - original code.
=============================================================================*/

char * LAT::get_ndms
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
    tbuf[0] = 'S';
    strncpy(buf, tbuf, len);
  }
  else
  {
    buf[0] = 'N';
    strncpy(&buf[1], tbuf, short(len - 1));
  }
  return buf;
}             /*End of LAT::get_ndms */


char * LAT::get_nDDMMmm
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
  ns = 'N';

  if (val < 0.0)
    ns = 'S';

  if (delim)
    sprintf(tbuf, "%c%c%02d%c%04d", ns, delim, d, delim, m);
  else
    sprintf(tbuf, "%c %02d %04d", ns, d, m);

  tbuf[len - 1] = 0;

  if ((short)strlen(tbuf) < len)
    strcpy(buf, tbuf);

  return buf;
}             /*End of LAT::get_ndms */





/*=============================================================================
  Name:        LAT::get_ndm

  Purpose:     returns a string containing ddmm.mmmm,N or ddcmm.mmmmN where
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

char * LAT::get_ndm
(
  char *buf,
  short len,
  short ndec,
  char delim
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
      sprintf(tbuf, "%02d%c%02.*f,S", d, delim, ndec, m);
    else
      sprintf(tbuf, "%02d%0*.*f,S", d, ndec + 3, ndec, m);
  }
  else
  {
    if (delim)
      sprintf(tbuf, "%02d%c%02.*f,N", d, delim, ndec, m);
    else
      sprintf(tbuf, "%02d%0*.*f,N", d, ndec + 3, ndec, m);
  }

  tbuf[len - 1] = 0;
  strcpy(buf, tbuf);

  return buf;
}




/*=============================================================================
  Name:        set_ndms

  Purpose:     sets the value to an ndms string to

  Parameters:  none.

  Discussion:

  History:     Dave Huff - May 1998 - original code.
=============================================================================*/

short LAT::set_ndms
(
  const char *cdms
)
{
  char dms[32];
  strcpy(dms, cdms);
  char *p;

  if ((p = strchr(dms, 'N')) != NULL)
    *p = '+';
  else if ((p = strchr(dms, 'n')) != NULL)
    *p = '+';
  else if ((p = strchr(dms, 'S')) != NULL)
    *p = '-';
  else if ((p = strchr(dms, 's')) != NULL)
    *p = '-';

  return set_dms(dms);
}             /*End of LAT::set_ndms */

// set_ndm - set the value based on input string of N DD MMMM
short LAT::set_ndm
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
    if (*p == 's' || *p == 'S' || *p == '-')
    {
      sign = -1;
      p++;
      continue;
    }
    if (*p == 'n' || *p == 'N' || *p == '+')
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
}             /*End of LAT::set_ndms */






/*=============================================================================
  Name:        set_rad

  Purpose:     sets the radians value for a latitude.  If the range is
               outside of -90 to 90 degrees the following occurs
               1 - the range is set to -PI to PI then if it is still
                   not within the -90 to 90 range 90 degrees is subtracted
                   from the absolute value of thangle.
               This creates the following results:
                 Input     Output
                 45         45
                 90         90
                 91         89
                 181        -89 (181 -PI_to_PI gives -179 then -90 = -89)

  Parameters:  radians (I) - the input value in radians

  Returns:     the final radians value

  History:     Dave Huff - May 1998 - original code.
=============================================================================*/

short  LAT::set_rad
(
  const double radians
)
{
  val = minus_pi_to_pi(radians);

  if (val < -PI_BY_TWO)
    val = -PI - val;
  else if (val > PI_BY_TWO)
    val = PI - val ;

  if (radians < -PI_BY_TWO || radians > PI_BY_TWO)
    return FALSE;

  return TRUE;
}             /*End of set_rad */

short LAT::set_dm(const char * ang)
{
  ANGLE::set_dm(ang);
  return LAT::set_rad(val);
}  // returns the angle



/*=============================================================================
  Name:        LAT::operator ==

  Purpose:     determines equivalency of two LAT objects

  Parameters:  rhs (I) - the rhs of the LAT1 == LAT2 statement

  Returns:     TRUE if they are equal

  History:     Dave Huff - July 1998 - original code.
=============================================================================*/

short LAT::operator ==
(
  const LAT &rhs
) const
{
  if (doubles_are_equal(val, rhs.get_rad(), 1.e-8))
    return 1;

  return 0;
}             /*End of LAT::operator == */



/*=============================================================================
  Name:        TESTING routines

  Purpose:     tests the class - define _TEST_LAT_ to compile a console based
               executable.

  History:     Dave Huff - May 1998 - original code.
=============================================================================*/
#ifdef _TEST_LAT_
#include "stdio.h"
#include "conio.h"
void main()
{
  LAT a, b;
  char buf[32];
  LAT c(1.234);

  a.set_rad(0.01);
  a.get_ndms(buf, 32, 3, ' ');
  fprintf(stderr, buf);
  getch();
  a.set_rad(-0.91);
  a.get_ndms(buf, 32, 3, ' ');
  fprintf(stderr, buf);
  getch();


  for (double rad = -TWOPI; rad < TWOPI + 1; rad += .01)
  {
    if (a.set_rad(rad))
    {
      a.get_ndms(buf, 32);
      b.set_ndms(buf);

      if (doubles_are_equal(b.get_rad(), a.get_rad(), .00001))
        continue;
      printf("Error 1: set_ndms problem at %.9f\n", rad);
    }
  }
  rad = - TWOPI;
  if (a.set_dm("8955.00"))
    printf("angle dm set ok\n");
  if (a.set_rad(rad))
    printf("Error: invalid set_rad return\n");

  if (a.set_rad(PI_BY_TWO + .1))
    printf("Error2: invalid set_rad return\n");

  if (a.set_dd(-91.0))
    printf("Error2: invalid set_rad return\n");

  if (a.set_dd(99))
    printf("Error2: invalid set_rad returned\n");
  printf("a should be 81 - a is %.2f\n", a.get_dd());
}

#endif
