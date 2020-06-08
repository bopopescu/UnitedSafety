/*=============================================================================
  Purpose:  creates an angle class that will handle converting dd, dm, dms
            angle strings to a radian value and also output the angle
            to the various formats
=============================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "utility.h"



/*=============================================================================
  Name:        ANGLE

  Purpose:     default constructor - sets the angle value to 0.0

  Parameters:  none

  Returns:     nothing

  History:     Dave Huff - May 1998 - original code.
=============================================================================*/

ANGLE::ANGLE()
{
  val = 0;
}

/*=============================================================================
  Name:        ANGLE

  Purpose:     initializer

  Parameters:  ang

  Returns:     nothing

  History:     Dave Huff - May 1998 - original code.
=============================================================================*/

ANGLE::ANGLE(double ang)
{
  val = ang;
}

/*=============================================================================
  Name:        ~ANGLE

  Purpose:     destructor - does nothing

  Parameters:  none

  Returns:     nothing

  History:     Dave Huff - May 1998 - original code.
=============================================================================*/
ANGLE::~ANGLE(){}


/*=============================================================================
  Name:        ANGLE::set_dm

  Purpose:     sets the value based on an input string of DDDMM.MMMMMM

  Parameters:  ang (I) - the input string

  Returns:     the angle in radians

  History:     Dave Huff - May 1998 - original code.
=============================================================================*/

short ANGLE::set_dm(const char *ang)
{
  double t;
  long d;

  t = atof(ang);
  
  d = (long)(t / 100.0);

  val = (double)d + (t - d * 100) / 60;
  val *= DEG_TO_RAD;
  return TRUE;
}

/*=============================================================================
  Name:        ANGLE::set_dms

  Purpose:     converts a dms string with any kind of delimiters into a
               radian value.

  Parameters:  dms (I) - the dms string

  Returns:     1 if the angle decodes ok, 0 if the decode fails

  History:     Dave Huff - May 1998 - original code.
=============================================================================*/

short ANGLE::set_dms
(
  const char *dms
)
{
  short i = 0, j = 0;
  short sign = 1;
  char work[32];
  
  memset(work, '\0', 32);

  while(dms[i] == ' ')
    i++;

  while(dms[i])
  {
    if (dms[i] == '+')
    {
      sign = 1;
      i++;
      continue;
    }

    if (dms[i] == '-')
    {
      sign = -1;
      i++;
      continue;
    }

    if (isdigit((short)dms[i]) || dms[i] == '.')
      work[j++] = dms[i];
    else
      work[j++] = ' ';

    i++;
  }

  // now delete the trailing blanks

  j--;
  while (work[j] == ' ')
    work[j--] = 0;

  FRAGMENT f;
  double d, m, s;

  f.squeeze(work, ' '); 
  f.fragment(work, ' ');

  if (f.get_num_items() == 3)  // string is now "DD MM SS.SSS"
  {
    f.item(0, &d);
    f.item(1, &m);
    f.item(2, &s);

  }
  else if (f.get_num_items() == 1 ) // string is now "DDDMMSS.SSS"
  {
    double dval;
    f.item(0, &dval);

    s = fmod(dval, 100);
    m = (long)(dval / 100.) % 100;
    d = (long)(dval /10000.);
  }
  else
    return 0;

  if (s > 60.0 || m > 60)
    return 0;

  val = d + m / 60.0 + s / 3600.0;
  val *= (sign * DEG_TO_RAD);
  return 1;
}


/*=============================================================================
  Name:        ANGLE::get_dd

  Purpose:     returns a decimal degrees string

  Parameters:  buf (O) - the output buffer
               len (I) - the maximum length of the buffer including '\0'
               ndec (I) - the number of decimals to output the string to

  Returns:     a pointer to the character buffer

  History:     Dave Huff - May 1998 - original code.
=============================================================================*/

char * ANGLE::get_dd
(
  char *buf,
  const short len,
  const short ndec
) const
{
  char tbuf[32];

  sprintf(tbuf, "%.*f", ndec, val * RAD_TO_DEG);
  tbuf[len - 1] = '\0';
  strcpy(buf, tbuf);

  return buf;
}

/*=============================================================================
  Name:        ANGLE::get_dm

  Purpose:     returns a string containing ddmm.mmmm of ddcmm.mmmm where
               dd is the degrees and mm.mmm is the minutes and c is a
               delimiter

  Parameters:  buf (O) the character buffer to be filled
               len (I) - the maximum length of the buffer including '\0'
               ndec (I) - the number of decimals to output the string to
               delim (I) - the delimiter to be used

  Returns:     a pointer to the buffer

  History:     Dave Huff - May 1998 - original code.
=============================================================================*/

char * ANGLE::get_dm
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

  if (d == 0 && val < 0.0)
  {
    if (delim)
      sprintf(tbuf, "-%d%c%02.*f", d, delim, ndec, m);
    else
      sprintf(tbuf, "-%d%0*.*f", d, ndec + 3, ndec, m);
  }
  else
  {
    if (delim)
      sprintf(tbuf, "%d%c%02.*f", d, delim, ndec, m);
    else
      sprintf(tbuf, "%d%0*.*f", d, ndec + 3, ndec, m);
  }

  tbuf[len - 1] = 0;
  strcpy(buf, tbuf);

  return buf;
}


/*=============================================================================
  Name:        ANGLE::get_dms

  Purpose:     returns a string containing ddmmss.ssss or ddcmmcss.ssss where
               dd is the degrees, mm is the minutes, ss.sss is the seconds
               and c is a delimiter

  Parameters:  buf (O) the character buffer to be filled
               len (I) - the maximum length of the buffer including '\0'
               ndec (I) - the number of decimals to output the string to
               delim (I) - the delimiter to be used, if it is 0 no spacing
                           will be done

  Returns:     a pointer to the buffer

  History:     Dave Huff - May 1998 - original code.
=============================================================================*/

char * ANGLE::get_dms
(
  char *buf,
  const short len,
  const short nd,
  const char delim
) const
{
  char tbuf[32];

  double deg, min, sec;
  int sign = 1;
  int i_deg, i_min;

  /* get the sign and make deg positive */

  deg = val * RAD_TO_DEG;

  if (deg < 0.0 )
    sign = -1;

  deg = fabs(deg);

  min   = modf ( deg , &deg ) * 60.0;
  sec   = modf ( min ,&min ) * 60.0;
  i_deg = (int)deg;
  i_min = (int)min;
  
  // have to be 0 - 4 decimals
  short ndec = nd;
  if (ndec < 0)
    ndec = 0;

  if (ndec > 4)
    ndec = 4;
  
  // check for rounding in the seconds
  double sec2 = sec;

  switch (ndec)
  {
    case 0:
      sec2 += 0.5;
      break;
    case 1:
      sec2 += 0.05;
      break;
    case 2:
      sec2 += 0.005;
      break;
    case 3:
      sec2 += 0.0005;
      break;
    case 4:
      sec2 += 0.00005;
      break;
  }

  /* deal with rounding ripple */

  if (sec2 > 60.0)
  {
    sec = 0.0;
    i_min++;

    if (i_min >= 60)
    {
      i_min -= 60;
      i_deg++;
    }
  }
 
  i_deg *= sign;

  if (i_deg == 0 && val < 0.0)
  {
    if (delim)
      sprintf ( tbuf, "-%d%c%02d%c%0*.*f", i_deg, delim, i_min, delim, ndec + 3, ndec, sec);
    else
      sprintf ( tbuf, "-%d%02d%0*.*f", i_deg, i_min, ndec + 3, ndec, sec);
  }
  else
  {
    if (delim)
      sprintf ( tbuf, "%d%c%02d%c%0*.*f", i_deg, delim, i_min, delim, ndec + 3, ndec, sec);
    else
      sprintf ( tbuf, "%d%02d%0*.*f", i_deg, i_min, ndec + 3, ndec, sec);
  }


  strncpy(buf, tbuf, len);
  return (buf);
}






/*=============================================================================
  Name:        zero_to_2_pi

  Purpose:     resets val to be 0 to 2PI

  Parameters:  none

  Returns:     the corrected angle

  History:     Dave Huff - June 1998 - original code.
=============================================================================*/

double ANGLE::zero_to_2_pi()
{
 if (val < 0.0 || val> TWOPI)
 {
   short num_twopi = (short) (val / TWOPI);
   val -= (double) num_twopi * TWOPI;

   if (val < 0.0)
     val += TWOPI;
 }
 return val;
}

/*=============================================================================
  Name:        zero_to_2_pi

  Purpose:     sets val to the input value and set it to be 0 to 2PI.

  Parameters:  radians (I) - the new value to be used

  Returns:     the corrected angle

  History:     Dave Huff - June 1998 - original code.
=============================================================================*/

double ANGLE::zero_to_2_pi
(
  const double radians
)
{
  val = radians;
  return zero_to_2_pi();
}





/*=============================================================================
  Name:        minus_pi_to_pi

  Purpose:     converts the angle to the range of -PI to PI

  Parameters:  none

  Returns:     the value in radians

  History:     Dave Huff - June 1998 - original code.
=============================================================================*/

double ANGLE::minus_pi_to_pi()
{
  zero_to_2_pi();

  if (val > PI)
    val -= TWOPI;

  return val;
}


/*=============================================================================
  Name:        minus_pi_to_pi

  Purpose:     sets val to the input angle and converts the angle
               to the range of -PI to PI.

  Parameters:  radians

  Returns:     the value in radians

  History:     Dave Huff - June 1998 - original code.
=============================================================================*/

double ANGLE::minus_pi_to_pi
(
  const double radians
)
{
  val = radians;
  return minus_pi_to_pi();
}             /*End of minus_pi_to_pi */


/*=============================================================================
  Name:        ANGLE::operator ==

  Purpose:     determines equivalency of two angles to  milliseconds

  Parameters:  p (I) - the rhs of the ANGLE == ANGLE statement

  Returns:     TRUE if they are equal

  History:     Dave Huff - July 1998 - original code.
=============================================================================*/

short ANGLE::operator ==
(
  const ANGLE &rhs
) const
{
  return doubles_are_equal(rhs.get_rad(), val, 0.000000004);
}


#ifdef _TEST_ANGLE_
#include <conio.h>
#include <iostream.h>

/*=============================================================================
  Name:        testing routines

  Purpose:     tests the ANGLE class functions

  Parameters:  none

  Returns:     nothing

  History:     Dave Huff - June 1998 - original code.
=============================================================================*/

void test2() // a further cyclic test - should not print anything
{
  //  this test will change radians in .0001 degree increments from -3PI to 3PI and
  //  set the value then get the value and make sure that it is valid.
  double r;
  double dd;
  char dds[32];
  char dm[32];
  char dms[32];
  ANGLE a, b;
  long i = 0;

  for (r = -6 * PI; r < 6 * PI; r +=.0001, i++)
  {
    if ((i % 10000) == 0)
      printf(" (%ld %.6f)\n", i, r);

    a.set_rad(r);

    if (!doubles_are_equal(r, a.get_rad(), .0001))
    {
      printf("FAILURE 1  %f: set_rad != get_rad\n", r);
      break;
    }

    if (!doubles_are_equal(r * RAD_TO_DEG, a.get_dd(), .0001))
    {
      printf("FAILURE 2  %f: set_rad != get_dd\n", r);
      break;
    }

    a.get_dd(dds, 32);
    dd = atof(dds);

    if (!doubles_are_equal(r * RAD_TO_DEG, dd, .000001))
    {
      printf("FAILURE 3  %f: set_rad != get_dd string\n", r);
      break;
    }

    a.get_dm(dm, 32);
    b.set_dm(dm);
    if (!doubles_are_equal(a.get_rad(), b.get_rad(), .000001))
    {
      printf("FAILURE 4  %f: a != b with set_dm\n", r);
      break;
    }

    a.get_dms(dms, 32);
    b.set_dms(dms);
    if (!doubles_are_equal(a.get_rad(), b.get_rad(), .000001))
    {
      printf("FAILURE 5  %f: a != b with set_dms\n", r);
      break;
    }

  }
}
void main()
{
  ANGLE a;
  char buf[32];

  // set_rad / get_rad (+/-) test
  for (short i = 0; i < 10; i++)
  {
    sprintf(buf, "0 59 59.999%d1\0", i);
    a.set_dms(buf);
    cerr << a.get_dms(buf, 32, 4, ' ') << "  ";
    cerr << a.get_dms(buf, 32, 3, ' ') << " | ";
    sprintf(buf, "0 00 59.999%d1\0", i);
    a.set_dms(buf);
    cerr << a.get_dms(buf, 32, 4, ' ') << "  ";
    cerr << a.get_dms(buf, 32, 3, ' ') << " | ";
    sprintf(buf, "0 00 00.000%d1\0", i);
    a.set_dms(buf);
    cerr << a.get_dms(buf, 32, 4, ' ') << "  ";
    cerr << a.get_dms(buf, 32, 3, ' ') << "\n";
  }
  _getch();
  cerr << "\n\n";

  for (i = 0; i < 10; i++)
  {
    sprintf(buf, "0 59 59.99%d1\0", i);
    a.set_dms(buf);
    cerr << a.get_dms(buf, 32, 2, ' ') << "  ";
    cerr << a.get_dms(buf, 32, 2, ' ') << " | ";
    sprintf(buf, "0 00 59.99%d1\0", i);
    a.set_dms(buf);
    cerr << a.get_dms(buf, 32, 3, ' ') << "  ";
    cerr << a.get_dms(buf, 32, 2, ' ') << " | ";
    sprintf(buf, "0 00 00.00%d1\0", i);
    a.set_dms(buf);
    cerr << a.get_dms(buf, 32, 4, ' ') << "  ";
    cerr << a.get_dms(buf, 32, 3, ' ') << "\n";
  }
  _getch();
  cerr << "\n\n";
  
  for (i = 0; i < 10; i++)
  {
    sprintf(buf, "0 59 59.9%d1\0", i);
    a.set_dms(buf);
    cerr << a.get_dms(buf, 32, 2, ' ') << " -> ";
    cerr << a.get_dms(buf, 32, 1, ' ') << "    ";
    sprintf(buf, "0 00 59.9%d1\0", i);
    a.set_dms(buf);
    cerr << a.get_dms(buf, 32, 2, ' ') << " ->";
    cerr << a.get_dms(buf, 32, 1, ' ') << "   ";
    sprintf(buf, "0 00 00.0%d1\0", i);
    a.set_dms(buf);
    cerr << a.get_dms(buf, 32, 2, ' ') << " -> ";
    cerr << a.get_dms(buf, 32, 1, ' ') << "\n";
  }
  cerr << "\n\n";
  _getch();

  a.set_rad(3 * PI + .1);
  printf("0->2PI = %.3f\n", a.zero_to_2_pi());
  printf("0->2PI = %.3f\n", a.zero_to_2_pi(3 * PI + .1));
  printf("%.3f -PI -> PI = %.3f\n", a.get_rad(), a.minus_pi_to_pi());
  printf("%.3f -PI -> PI = %.3f\n", a.get_rad(), a.minus_pi_to_pi(3 * PI + .1));

  a.set_rad(PI);
  printf("angle %.9f should be %.9f\n", a.get_rad(), PI);
  printf("angle of PI is %.6f degrees\n", a.get_dd());

  a.set_rad(-PI);
  printf("angle %.9f should be %.9f\n", a.get_rad(), -PI);
  printf("angle of PI is %.6f degrees\n", a.get_dd());

  // set_dd / get_dd (+/-) test

  a.set_dd(33.0);
  printf("angle %.9f should be %.9f\n", a.get_rad(), 33.0 * DEG_TO_RAD);
  printf("angle of 33.0 is %.6f degrees\n", a.get_dd());

  a.set_dd(-33.0);
  printf("angle %.9f should be %.9f\n", a.get_rad(), -33.0 * DEG_TO_RAD);
  printf("angle of -33.0 is %.6f degrees\n", a.get_dd());

  // set_dm / get_dm test

  a.set_dm("13030.50");
  printf("DM angle of 13030.50 is %.6f radians\n", a.get_rad());
  printf("DM angle of 13030.50 is %s as a DM string\n", a.get_dm(buf, 32));
  printf("DM angle of 13030.50 is %s as a DM string with 3 decimals\n", a.get_dm(buf, 32, 3));
  printf("DM angle of 13030.50 is %s as a DM string with 3 decimals\n", a.get_dm(buf, 32, 3, ' '));
  // set_dm / get_dm test  just below equator

  printf("DM angle of -30.50 is %.6f radians\n", a.set_dm("-30.50"));
  printf("DM angle of -30.50 is %s as a DM string\n", a.get_dm(buf, 32));
  printf("DM angle of -30.50 is %s as a DM string with 3 decimals\n", a.get_dm(buf, 32, 3));
  printf("DM angle of -30.50 is %s as a DM string with 3 decimals\n", a.get_dm(buf, 32, 3, ' '));

  // set_dm / get_dm test

  printf("DM angle of -13030.50 is %.6f radians\n", a.set_dm("-030.50"));
  printf("DM angle of -13030.50 is %s as a DM string\n", a.get_dm(buf, 32));
  printf("DM angle of -13030.50 is %s as a DM string with 3 decimals\n", a.get_dm(buf, 32, 3));
  printf("DM angle of -13030.50 is %s as a DM string with 3 decimals\n", a.get_dm(buf, 32, 3, ' '));  

  // set_dms / get_dms test
  if (a.set_dms("-05947.49241"))
  {
    printf("DMS angle of -1303029.50 is %.6f radians\n", a.get_rad());
    printf("DMS angle of -1303029.50 is %s as a DMS string\n", a.get_dms(buf, 32));
    printf("DMS angle of -1303029.50 is %s as a DMS string with 3 decimals\n", a.get_dms(buf, 32, 3));
    printf("DMS angle of -1303029.50 is %s as a DMS string with 3 decimals\n", a.get_dms(buf, 32, 3, ' '));  
  }
  else
    printf("FAILURE:: dms failed to set\n");

  if (a.set_dms("-3029.50"))
  {
    printf("DMS angle of -030.50 is %.6f radians\n", a.get_rad());
    printf("DMS angle of -030.50 is %s as a DMS string\n", a.get_dms(buf, 32));
    printf("DMS angle of -030.50 is %s as a DMS string with 3 decimals\n", a.get_dms(buf, 32, 3));
    printf("DMS angle of -030.50 is %s as a DMS string with 3 decimals\n", a.get_dms(buf, 32, 3, ' '));  
  }
  else
    printf("DMS angle failed to set\n");

  test2();
}

#endif
    
    
    
