/*$----------------------------------------------------------------------------
  Name:        J2K

  Purpose:     Multipurpose date/time with conversion functions between
               J2K time, dd/mm/yy and UTC, along with week, year, time

               for most storage and transmission purposes the time member is
                           best used only
                           for example for the polling and time tagging use the get_time
                           function to return the time in seconds

               for display and time manipulation purposes use the entire class


  Methods:     Time is determined relative to 2000 January 1.5d
               This is a standard epoch.
               See Hofmann-Wellenhof, Lichtenegger and Collins
               GPS Theory and Practice, Pgs 38-42.

  Limitations:  This module does not presently account for the differences
                between GPS atomic time and UTC (the difference is leap seconds)
                If you initialize with GPS atomic time, the times will be
                based on this, if you initialize with UTC, the times will
                not account for upcoming leapseconds (once or twice a year).

  History:     John Schleppe - May 1998 - original code.
-----------------------------------------------------------------------------*/
#include "stdio.h"
#include "stdlib.h"
#include <iostream>
#include "string.h"
#include "J2K.h"




/*$----------------------------------------------------------------------------
  Name:        J2K

  Purpose:     class constructor for the J2K class.

  Parameters:  none

  History:     John Schleppe - May 1998 - original code.
-----------------------------------------------------------------------------*/

J2K::J2K()
{
  clear();
}             /*End of J2K */



/*$----------------------------------------------------------------------------
  Name:        J2K

  Purpose:     class constructor for the J2K class.

  Parameters:  t(I) - time since or before J2000 (seconds)

  History:     John Schleppe - May 1998 - original code.
-----------------------------------------------------------------------------*/

J2K::J2K(double t)
{
  time = t;
  valid = J2K_TRUE;
}             /*End of J2K */


/*$----------------------------------------------------------------------------
  Name:        J2K

  Purpose:     class constructor for the J2K class.

  Parameters:  y(I)    - Year AD
               m(I)    - month of year 1=January
               d(I)    - day of month (1-31)
               day_t(I)- seconds since midnight

  History:     John Schleppe - May 1998 - original code.
-----------------------------------------------------------------------------*/

J2K::J2K(short y, short m, short d, double day_t)
{
  if (!dmy_to_time(d, m, y, day_t))
  {
    // generate an exception
    valid = J2K_FALSE;
  }
  else
    valid = J2K_TRUE;
}             /*End of J2K */


/*$----------------------------------------------------------------------------
  Name:        J2K

  Purpose:     class constructor for the J2K class.

  Parameters:  y(I)    - Year AD
               m(I)    - month of year 1=January
               d(I)    - day of month (1-31)
               h(I)    - hour of day (0-23)
               min(I)  - minutes of hour
               s(I)  - seconds

  History:     John Schleppe - May 1998 - original code.
-----------------------------------------------------------------------------*/

J2K::J2K(short y, short m, short d, short h, short min, double s)
{
  double  day_t;

  if (   !hms_to_daytime(h, min, s, day_t)
      || !dmy_to_time(d, m, y, day_t))
  {
    // generate an exception
    valid = J2K_FALSE;
  }
  else
    valid = J2K_TRUE;
}             /*End of J2K */


/*$----------------------------------------------------------------------------
  Name:        J2K

  Purpose:     class constructor for the J2K class.

  Parameters:  y(I)     - year AD
               w(I)     - GPS week number since Jan6, 1980
               gps_t(I) - Seconds since Saturday Midnight

  History:     John Schleppe - May 1998 - original code.
-----------------------------------------------------------------------------*/

J2K::J2K(short y, short w, double gps_t)
{
  if (!gps_to_time(w, y, gps_t))
  {
    // generate an exception
    valid = J2K_FALSE;
  }
  else
    valid = J2K_TRUE;
}             /*End of J2K */


/*$----------------------------------------------------------------------------
  Name:        J2K::operator +=

  Purpose:     add a number of seconds to the current time

  Parameters:  secs (I) - number of seconds to add to time

  History:     John Schleppe - May 1998 - original code.
-----------------------------------------------------------------------------*/

J2K& J2K::operator +=(double secs)
{
  time += secs;

  return *this;
}


/*$----------------------------------------------------------------------------
  Name:        J2K::operator -=

  Purpose:     subtract seconds from the current time

  Parameters:  secs (I) - number of seconds to subtract from time

  History:     John Schleppe - May 1998 - original code.
-----------------------------------------------------------------------------*/

J2K& J2K::operator -=(double secs)
{
  time -= secs;

  return *this;
}


/*$----------------------------------------------------------------------------
  Name:        J2K::daytime_to_hms

  Purpose:     convert seconds since midnight to hours, minutes, seconds

  Parameters:  daytime (I) - daytime in seconds since midnight
               hours(O)    - hours of day
               minutes(O)  - minutes of hour
               seconds(O)  - seconds of minute

  History:     John Schleppe - May 1998 - original code.
-----------------------------------------------------------------------------*/

short J2K::daytime_to_hms
(
  double daytime,
  short &hours,
  short &minutes,
  double &seconds
)
{
  short rc = J2K_TRUE;

  seconds = fmod(daytime, 60.0);
  minutes = (short) fmod( daytime / 60.0, 60.0);
  hours   = (short) fmod( daytime / 3600.0, 24.0);

  if (   hours > 24 || hours < 0
      || minutes > 60 || minutes < 0
      || seconds > 60.0 || seconds < 0.0
      || daytime > SEC_PER_DAY || daytime < 0.0)
    rc = J2K_FALSE;

  return rc;
}             /*End of J2K::daytime_to_hms */

/*$----------------------------------------------------------------------------
  Name:        J2K::dmy_string_to_dmy

  Purpose:     convert day, month, year string (dd/mm/yyyy)
               to day, month year numbers

  Parameters:
               buffer(I)  - dd/mm/yyyy
               d(O)       - day
               m(o)       - month
               y(0)       - year

  History:     John Schleppe - March 8, 2000 - original code.
-----------------------------------------------------------------------------*/

short J2K::dmy_string_to_dmy
(
  const char* buffer, 
  short &d,
  short &m,
  short &y
)
{
  char   lbuf[30];
  int    slash = 0;
  int    i;

  for (i = 0; i < (int)strlen(buffer); i++)
    if (buffer[i] == '/')
      slash++;

  if (slash != 2)
    return(J2K_FALSE);

  strcpy(lbuf, buffer);
  d = (short)atoi(lbuf);

  if (d > 31 || d < 0)
    return (J2K_FALSE);

  for (i = 0; lbuf[i] != '/'; i++);

  m = (short)atoi(&lbuf[i + 1]);

  if (m > 12 || m < 1)
    return J2K_FALSE;

  for (i++; lbuf[i] != '/'; i++);

  y = (short)atoi(&lbuf[i + 1]);

  if (y >= 3000 || y < 1900)
    return J2K_FALSE;

  return J2K_TRUE;
}


/*$----------------------------------------------------------------------------
  Name:        J2K::dmy_to_time

  Purpose:     convert Gregorian day, month, year to J2000 time

  Parameters:  day (I)     - day of month
               month (I)   - month of year
               year (I)    - year AD
               daytime (I) - seconds since midnight UTC

  History:     John Schleppe - May 1998 - original code.
-----------------------------------------------------------------------------*/

short J2K::dmy_to_time
(
  short  day,
  short  month,
  short  year,
  double daytime
)
{
  short rc = J2K_TRUE;

  time = (367.0 * double(year)
       - floor(7.0 * (double(year) + floor(double(month + 9) / 12.0))/ 4.0)
       + floor(275.0 * month / 9.0)
       + day
       - 730531.5) * SEC_PER_DAY + daytime;

  if (   year < 1900 || year > 2100
      || month < 1   || month > 12
      || day < 1     || day > 31
      || daytime < 0.0 || daytime > SEC_PER_DAY)
    rc = J2K_FALSE;

  return(rc);
}             /*End of J2K::dmy_to_time */


/*$----------------------------------------------------------------------------
  Name:        J2K::gregorian_to_time

  Purpose:     convert Gregorian day and year to J2000 time

  Parameters:  day_of_year (I) - day of year
               year (I)        - year AD
               daytime (I)     - seconds since midnight UTC

  History:     John Schleppe - May 1998 - original code.
-----------------------------------------------------------------------------*/

short J2K::gregorian_to_time
(
  short day_of_year,
  short year,
  double daytime
)
{
  short rc = J2K_TRUE;

  time = (367.0 * double(year)
       - floor(7.0 * double(year) / 4.0)
       + day_of_year
       + 30.0
       - 730531.5) * SEC_PER_DAY + daytime;

  if (   day_of_year < 1 || day_of_year > 366
      || year < 1900 || year > 2100
      || daytime < 0.0 || daytime > SEC_PER_DAY)
    rc = J2K_FALSE;

  return rc;
}             /*End of J2K::gregorian_to_time */


/*$----------------------------------------------------------------------------
  Name:        J2K::get_dmy

  Purpose:     to send back the current day, month, year

  Parameters:
               dy(O)    - day of month (1 -31)
               mn(O)    - month of year (1 - 12)
               yr(O)    - year AD

               returns J2K_FALSE if computation was invalid

  History:     John Schleppe - May 1998 - original code.
-----------------------------------------------------------------------------*/

short J2K::get_dmy(short& dy, short& mn, short& yr)
{
  double daytime;

  if (!time_to_dmy(dy, mn, yr, daytime))
    valid = J2K_FALSE;

  return valid;
}                        /*End of J2K::get_dmy */


/*$----------------------------------------------------------------------------
  Name:        J2K::get_dmy

  Purpose:     to send back the current day, month, year

  Parameters:  buffer(O) - the dd/mm/yyyy will be written to the buffer
               ensure that you have a minimum of 11 char declared in buffer
              
               This will give the date for time rounded to the nearest second.

               returns J2K_FALSE if computation was invalid

  History:     John Schleppe - May 1998 - original code.
-----------------------------------------------------------------------------*/

short J2K::get_dmy(char *buffer)
{
  short dy, mn, yr;
  double save_time;
  double daytime;

  buffer[0] = 0;

  save_time = time;
  time = floor(time + 0.5);

  if (!time_to_dmy(dy, mn, yr, daytime))
    valid = J2K_FALSE;
  else
    sprintf(buffer, "%02hd/%02hd/%04hd", dy, mn, yr);

  time = save_time;
  return valid;
}                        /*End of J2K::get_dmy */

/*$----------------------------------------------------------------------------
  Name:        J2K::get_dmy

  Purpose:     to send back the current day, month, year

  Parameters:  buffer(O) - the mm/dd/yyyy will be written to the buffer
               ensure that you have a minimum of 11 char declared in buffer
              
               This will give the date for time rounded to the nearest second.

               returns J2K_FALSE if computation was invalid

  History:     John Schleppe - May 1998 - original code.
-----------------------------------------------------------------------------*/

short J2K::get_dmy_US(char *buffer)
{
  short dy, mn, yr;
  double save_time;
  double daytime;

  buffer[0] = 0;

  save_time = time;
  time = floor(time + 0.5);

  if (!time_to_dmy(dy, mn, yr, daytime))
    valid = J2K_FALSE;
  else
    sprintf(buffer, "%02hd/%02hd/%04hd", mn, dy, yr);

  time = save_time;
  return valid;
}                        /*End of J2K::get_dmy */


/*$----------------------------------------------------------------------------
  Name:        J2K::get_gps

  Purpose:     to send back the current day, month, year

  Parameters:
               gpstime(O)  - seconds since saturday midnight
               day_of_week - 0=sunday, 1=monday etc.
               gpsweek(O)  - weeks since Jan 6, 1980
               year(O)     - year AD

               returns J2K_FALSE if computation was invalid

  History:     John Schleppe - May 1998 - original code.
-----------------------------------------------------------------------------*/

short J2K::get_gps
(
  short  &day_of_week,
  short  &gpsweek,
  short  &year,
  double &gpstime
)
{
  if (!time_to_gps(day_of_week, gpsweek, year, gpstime))
    valid = J2K_FALSE;

  return valid;
}                        /*End of J2K::get_gps */


/*$----------------------------------------------------------------------------
  Name:        J2K::get_gps

  Purpose:     to send back the current day, month, year

  Parameters:
               gpstime(O)  - seconds since saturday midnight
               day_of_week - 0=sunday, 1=monday etc.
               gpsweek(O)  - weeks since Jan 6, 1980
               year(O)     - year AD

               returns J2K_FALSE if computation was invalid

  History:     John Schleppe - May 1998 - original code.
-----------------------------------------------------------------------------*/

short J2K::get_gregorian
(
  short  &day_of_year,
  short  &year,
  double &daytime
)
{
  if (!time_to_gregorian(day_of_year, year, daytime))
    valid = J2K_FALSE;

  return valid;
}                        /*End of J2K::get_gregorian */


/*$----------------------------------------------------------------------------
  Name:        J2K::get_hms

  Purpose:     to send back the current hour, minutes and seconds

  Parameters:
               h(O)    - hour of day (0-23)
               min(O)  - minutes of hour
               s(O)    - seconds



  History:     John Schleppe - May 1998 - original code.
-----------------------------------------------------------------------------*/

short J2K::get_hms(short& h, short& min, double& s)
{
  short d, m, y;
  double daytime;

  if (   !time_to_dmy(d, m, y, daytime)
      || !daytime_to_hms(daytime, h, min, s))
    valid = J2K_FALSE;

  return valid;
}                      /*End of J2K::get_hms */


/*$----------------------------------------------------------------------------
  Name:        J2K::get_hms

  Purpose:     to send back the current hour, minutes and seconds

  Parameters:
               buffer (O) - contains hh:mm:ss - make sure you have a buffer
                            declared to handle 11 chars

               This will round the time to the nearest second.

             History:     John Schleppe - May 1998 - original code.
-----------------------------------------------------------------------------*/

short J2K::get_hms(char *buffer)
{
  short d, m, y;
  short h, min;
  double s;
  double daytime;
  double save_time;

  save_time = time;
  time = floor(time + 0.5) + 0.0001;

  buffer[0] = 0;

  if (   !time_to_dmy(d, m, y, daytime)
      || !daytime_to_hms(daytime, h, min, s))
    valid = J2K_FALSE;
  else
    sprintf(buffer, "%02hd:%02hd:%02.0f", h, min, s);

  time = save_time;

  return valid;
}                      /*End of J2K::get_hms */

/*$----------------------------------------------------------------------------
  Name:        J2K::get_hms

  Purpose:     to send back the current hour, minutes and seconds

  Parameters:
               buffer (O) - contains hh:mm:ss - make sure you have a buffer
                            declared to handle 11 chars

               This will round the time to the nearest second.

             History:     John Schleppe - May 1998 - original code.
-----------------------------------------------------------------------------*/

short J2K::get_hms_ds(char *buffer)
{
  short d, m, y;
  short h, min;
  double s;
  double daytime;
  
  buffer[0] = 0;

  if (   !time_to_dmy(d, m, y, daytime)
      || !daytime_to_hms(daytime, h, min, s))
    valid = J2K_FALSE;
  else
    sprintf(buffer, "%02hd:%02hd:%06.3f", h, min, s);

  return valid;
}                      /*End of J2K::get_hms */


/*$----------------------------------------------------------------------------
  Name:        J2K::get_hms

  Purpose:     to send back the current hour, minutes and seconds with no
               colon deliminator - used for NMEA messages

  Parameters:
               buffer (O) - contains hh:mm:ss - make sure you have a buffer
                            declared to handle 11 chars

               This will round the time to the nearest second.

             History:     John Schleppe - May 1998 - original code.
-----------------------------------------------------------------------------*/

short J2K::get_hms_ds_nodelim(char *buffer)
{
  short d, m, y;
  short h, min;
  double s;
  double daytime;
  
  buffer[0] = 0;

  if (   !time_to_dmy(d, m, y, daytime)
      || !daytime_to_hms(daytime, h, min, s))
    valid = J2K_FALSE;
  else
    sprintf(buffer, "%02hd%02hd%05.2f", h, min, s);

  return valid;
}                      /*End of J2K::get_hms */


/*$----------------------------------------------------------------------------
  Name:        J2K::gps_to_time

  Purpose:     convert gps time and week to J2000 time
               assumes the week goes to zero after 10bit rollover problem in
               Aug 21st 1999.
               requires that the year has also been set

  Parameters:  gpsweek (I) - number of weeks since Jan 6, 1980
               year    (I) - year AD
               gpstime (I) - time in seconds since Saturday Midnight (UTC)

  History:     John Schleppe - May 1998 - original code.
-----------------------------------------------------------------------------*/

short J2K::gps_to_time
(
  short gpsweek,
  short year,
  double gpstime
)
{
  short  rc = J2K_TRUE;
  double jd, jd_year;
  double num_weeks;
  double num_rolls = 0.0;

  // Sort out year

  if (year < 1980)
    year = 1999;

  // Account for GPS week crossover by computing the actual number
  // of weeks between J1980 and the 1st day of the current year
  // and then compare that to the expected number of weeks for this year
  // The number of 1024 week intervals is then computed if the gpsweeks
  // is less than the computed number of weeks

  jd_year = (367.0 * double(year)
          - floor(7.0 * double(year) / 4.0)
          + 31.0
          - 730531.5) + J2000;

  num_weeks = floor((jd_year - J1980) / 7.0);

  if (gpsweek < num_weeks)
    num_rolls = floor((num_weeks - gpsweek) / 1024.0 + 0.5);

  jd   = double(gpsweek) * 7.0 + J1980 + num_rolls * 7168.0 + gpstime / SEC_PER_DAY;

  time = (jd - J2000) * SEC_PER_DAY;

  if (gpstime < 0.0 || gpstime > SEC_PER_WEEK || gpsweek < 0 || year < 1900 || year > 2100)
    rc = J2K_FALSE;

  return rc;
}             /*End of J2K::gps_to_time */


/*$----------------------------------------------------------------------------
  Name:        J2K::hms_to_daytime

  Purpose:     convert hours, minutes, seconds to seconds since midnight

  Parameters:
               hours(I)   - hour of day (0-23)
               minutes(I) - minutes of hour
               seconds(I) - seconds
               daytime(O) - seconds since midnight

  History:     John Schleppe - May 1998 - original code.
-----------------------------------------------------------------------------*/

short J2K::hms_to_daytime
(
  short  hours,
  short  minutes,
  double seconds,
  double &daytime
)
{
  short rc = J2K_TRUE;

  daytime = 3600.0 * double(hours) + 60.0 * double(minutes) + seconds;

  if (   hours > 24 || hours < 0
      || minutes > 60 || minutes < 0
      || seconds > 60.0 || seconds < 0.0
      || daytime > SEC_PER_DAY || daytime < 0.0)
    rc = J2K_FALSE;

  return rc;
}             /*End of J2K::hms_to_daytime */

/*$----------------------------------------------------------------------------
  Name:        J2K::hms_string_to_daytime

  Purpose:     convert hours, minutes, seconds string (hh:mm:ss.sss)
               to seconds since midnight

  Parameters:
               buffer(I)  - hh:mm:ss.sss
               daytime(O) - seconds since midnight

  History:     John Schleppe - March 8, 2000 - original code.
-----------------------------------------------------------------------------*/

short J2K::hms_string_to_daytime
(
  const char* buffer, 
  double& daytime
)
{
  char   lbuf[32];
  int    colon = 0;
  int    h;
  int    i;
  int    m;
  int    s;

  for (i = 0; i < (int)strlen(buffer); i++)
    if (buffer[i] == ':')
      colon++;

  if (colon != 2)
    return(J2K_FALSE);

  strcpy(&lbuf[0], buffer);
  h = atoi(lbuf);

  if (h > 24 || h < 0)
    return (J2K_FALSE);

  for (i = 0; lbuf[i] != ':'; i++);

  m = atoi(&lbuf[i + 1]);

  if (m > 59 || m < 0)
    return J2K_FALSE;

  for (i++; lbuf[i] != ':'; i++);

  s = atoi(&lbuf[i + 1]);

  if (s >= 60.0 || s < 0.0)
    return J2K_FALSE;

  daytime = (h * 3600.0) + (m * 60.0) + s;

  daytime = fmod(daytime, SEC_PER_DAY);

  return J2K_TRUE;
}


/*$----------------------------------------------------------------------------
  Name:        set_dmy_hms

  Purpose:     set the time with the input day, month, year, hours, minutes,
               and seconds

  Parameters:  y(I)    - Year AD
               m(I)    - month of year 1=January
               d(I)    - day of month (1-31)
               h(I)    - hour of day (0-23)
               min(I)  - minutes of hour
               s(I)    - seconds

  History:     John Schleppe - May 1998 - original code.
-----------------------------------------------------------------------------*/

short J2K::set_dmy_hms
(
  short d,
  short m,
  short y,
  short h,
  short min,
  double s
)
{
  double daytime;

  if (   !hms_to_daytime(h, min, s, daytime)
      || !dmy_to_time(d, m, y, daytime))
    valid = J2K_FALSE;
  else
    valid = J2K_TRUE;

  return valid;
}             /*End of set_dmy_hms*/

/*$----------------------------------------------------------------------------
  Name:        set_dmy_hms

  Purpose:     set the time with the input day, month, year, hours, minutes,
               and seconds

  Parameters:  dmy(I) - day/month/year - character buffer
               hms(I) - hours:minutes:seconds - character buffer

  History:     John Schleppe - March 8, 2000 - original code.
-----------------------------------------------------------------------------*/

short J2K::set_dmy_hms
(
  const char *dmy,
  const char *hms
)
{
  double daytime;
  short d, m, y;

  if (   !hms_string_to_daytime(hms, daytime)
      || !dmy_string_to_dmy(dmy, d, m, y)
      || !dmy_to_time(d, m, y, daytime))
    valid = J2K_FALSE;
  else
    valid = J2K_TRUE;

  return valid;
}             /*End of set_dmy_hms*/

/*$----------------------------------------------------------------------------
  Name:        set_dmy_hms

  Purpose:     set the time with the input day, month, year, hours, minutes,
               and seconds

  Parameters:  y(I)    - Year AD
               m(I)    - month of year 1=January
               d(I)    - day of month (1-31)
               hms(I) - hours:minutes:seconds - character buffer

  History:     John Schleppe - March 8, 2000 - original code.
-----------------------------------------------------------------------------*/

short J2K::set_dmy_hms
(
  const short d,
  const short m,
  const short y,
  const char *hms
)
{
  double daytime;

  if (   !hms_string_to_daytime(hms, daytime)
      || !dmy_to_time(d, m, y, daytime))
    valid = J2K_FALSE;
  else
    valid = J2K_TRUE;

  return valid;
}             /*End of set_dmy_hms*/


/*$----------------------------------------------------------------------------
  Name:        set_gps

  Purpose:     set the time with the input gpstime, week and year

  Parameters:  y(I)    - Year AD
               m(I)    - month of year 1=January
               d(I)    - day of month (1-31)
               h(I)    - hour of day (0-23)
               min(I)  - minutes of hour
               s(I)    - seconds

  History:     John Schleppe - May 1998 - original code.
-----------------------------------------------------------------------------*/

short J2K::set_gps
(
  short year,
  short week,
  double gpstime
)
{

  if (!gps_to_time(week, year, gpstime))
    valid = J2K_FALSE;
  else
    valid = J2K_TRUE;

  return valid;
}             /*End of set_gps*/


/*$----------------------------------------------------------------------------
  Name:        set_year_day_time

  Purpose:     set the time with the input gregorian day, year and daytime

  Parameters:  y(I)     - Year AD
               d(I)     - day of year (1-366)
               day_t(I) - seconds

  History:     John Schleppe - May 1998 - original code.
-----------------------------------------------------------------------------*/

short J2K::set_year_day_time(short y, short d, double day_t)
{
  if (!gregorian_to_time(d, y, day_t))
    valid = J2K_FALSE;
  else
    valid = J2K_TRUE;

  return valid;
}             /*End of set_dmy_hms*/


/*$----------------------------------------------------------------------------
  Name:        J2K::set_time

  Purpose:     class constructor for the J2K class.

  Parameters:  t(I) - time since or before J2000 (seconds)

  History:     John Schleppe - May 1998 - original code.
-----------------------------------------------------------------------------*/

void J2K::set_time(double t)
{
  time = t;
  valid = J2K_TRUE;
}             /*End of J2K:set_time */


/*$----------------------------------------------------------------------------
  Name:        J2K::time_to_dmy

  Purpose:     convert J2K time (seconds) into day, month, year and daytime

  Parameters:  day (I)  - day of month
               month(I) - month of year
               year(I)  - year AD
               daytime (I) - seconds since midnight

  History:     John Schleppe - May 1998 - original code.
-----------------------------------------------------------------------------*/

short J2K::time_to_dmy
(
  short  &day,
  short  &month,
  short  &year,
  double &daytime
) const 
{
  short rc = J2K_TRUE;
  double a, b, c, d, e, partial, jd;

  jd = time / SEC_PER_DAY + J2000;
  partial = modf(jd + 0.5, &a);
  b = a + 1537.0;
  c = floor((b - 122.1) / 365.25);
  d = floor(365.25 * c);
  e = floor((b - d) / 30.6001);

  day     = short(b - d - floor(30.6001 * e) + partial);
  month   = short(e - 1.0 - 12.0 * floor(e / 14.0));
  year    = short(c - 4715.0 - floor((7.0 + month) / 10.0));
  daytime = partial * SEC_PER_DAY;

  if (   day < 1 || day > 31
      || month < 1 || month > 12
      || year < 1900 || year > 2100
      || daytime < 0.0 || daytime > SEC_PER_DAY)
    rc = J2K_FALSE;

  return rc;
}             /*End of J2K::time_to_dmy */


/*$----------------------------------------------------------------------------
  Name:        J2K::time_to_gps

  Purpose:     convert J2K time (seconds) into gpsweek, gpstime, gpsday of week

  Parameters:  gpstime (O)     - seconds since midnight saturday
               gpsweek (O)     - weeks since Jan 6, 1980, modulus 1024
               year (O)        - year AD

  History:     John Schleppe - May 1998 - original code.
-----------------------------------------------------------------------------*/

short J2K::time_to_gps
(
  short  &day_of_week,
  short  &gpsweek,
  short  &year,
  double &gpstime
)
{
  short  rc = J2K_TRUE;
  short  day, month;
  double jd, daytime;

  if (  !time_to_dmy(day, month, year, daytime))
        rc = J2K_FALSE;

  jd          = time / SEC_PER_DAY + J2000;
  day_of_week = short(fmod(floor(jd + 1.5), 7.0));
  gpstime     = (double(day_of_week) * SEC_PER_DAY + daytime);
  gpsweek     = (short)fmod(floor((jd - J1980) / 7.0), 1024.0);

  if (   year < 1900 || year > 2100
      || day_of_week < 0 || day_of_week > 6
      || gpstime < 0.0 || gpstime > SEC_PER_WEEK
      || gpsweek < 0)
  {
    rc = J2K_FALSE;
  }

  return rc;
}             /*End of J2K::time_to_gps */


/*$----------------------------------------------------------------------------
  Name:        J2K::time_to_gregorian

  Purpose:     convert J2K time (seconds) into day, year and daytime

  Parameters:  day_of_year (I)  - day of year
               year(I)          - year AD
               daytime (I)      - seconds since midnight

  History:     John Schleppe - May 1998 - original code.
-----------------------------------------------------------------------------*/

short J2K::time_to_gregorian
(
  short  &day_of_year,
  short  &year,
  double &daytime
)
{
  short  rc = J2K_TRUE;
  short  day, month;

  time_to_dmy(day, month, year, daytime);

  double jd = time / SEC_PER_DAY + J2000;

  day_of_year = short(jd
              - (  floor(365.25 * (year - 1.0))
                 + floor(30.6001 *  13.0)
                 + 31.0
                 + 1720981.5
                ));

  if (   day < 1 || day > 31
      || month < 1 || month > 12
      || year < 1900 || year > 2100
      || daytime < 0.0 || daytime > SEC_PER_DAY
      || day_of_year < 1 || day_of_year > 366)
    rc = J2K_FALSE;

  return rc;
}             /*End of J2K::time_to_gregorian */


//--------------------------------------------------
// GetSystemTime
//
// load the current system time into a J2K time
//
void J2K::SetSystemTime()
{
  time_t     now;
  struct tm  *ts;

  now = ::time(NULL);
  ts = localtime(&now);
  set_dmy_hms(ts->tm_mday, ts->tm_mon + 1, ts->tm_year + 1900,
               ts->tm_hour, ts->tm_min, ts->tm_sec);
}

// GetTimeString
// returns HH:MM:SS DD/MM/YYYY in buf
//
char * J2K::GetTimeString(char *buf, int lenBuf)
{

  short d, m, y;
  short h, min;
  double s;
  double daytime;
  
  if (   !time_to_dmy(d, m, y, daytime)
      || !daytime_to_hms(daytime, h, min, s)
      || lenBuf < 19)
  {
    buf[0] = 'X';
    return buf;
  }

  sprintf(buf, "%02d:%02d:%02d %02d/%02d/%d", h, min, (short)s, d, m, y);
  return buf;
}



#ifdef _TEST_J2K_
/*$----------------------------------------------------------------------------
  Name:        J2K::test_gps

  Purpose:     function to test the conversion from time to gpstime and
               from gpstime back to time

  Parameters:  none

  History:     John Schleppe - May 1998 - original code.
-----------------------------------------------------------------------------*/

void J2K::test_gps(void)
{
  double start = time;
  short  day_of_week, gpsweek, year;
  double gpstime;

  if (!time_to_gps(day_of_week, gpsweek, year, gpstime))
  {
    cerr << "Failure in functions:  time_to_gps\n";
    cerr << "Week: " << gpsweek << " GPSTIME: " << gpstime << " WEEKDAY: " << day_of_week << "\n";
    cerr << "GPStime: " << gpstime << "\n";
  }

  double s_gt  = gpstime;
  short  s_wd  = day_of_week;
  short  s_gw  = gpsweek;
  short  day, month;
  double daytime;

  if (!time_to_dmy(day, month, year, daytime))
  {
    cerr << "Failure in functions:  time_to_dmy\n";
    cerr << "Day: " << day << " Month: " << month << " Year: " << year << "\n";
    cerr << "Daytime: " << daytime << "\n";
  }

  double s_dt  = daytime;

  if (!gps_to_time(gpsweek, year, gpstime))
  {
    cerr << "Failure in functions:  gps_to_time\n";
  }

  if (fabs(start - time) > 0.001)
  {
    cerr.precision(8);
    cerr.setf(ios::fixed, ios::floatfield);
    cerr << "Time - GPSTIME Conversion Failure at: " << start << " " << time << " Delta: " << start - time << "\n";
    cerr << "GPStime: " << s_gt << " " << gpstime << "\n";
    cerr << "Day of Week: " << s_wd << " " << day_of_week << "\n";
    cerr << "GPS Week: " << s_gw << " " << gpsweek << "\n";
    _getch();
  }

  start = time;

  if (!time_to_dmy(day, month, year, daytime))
  {
    cerr << "Failure in functions:  time_to_dmy\n";
    cerr << "YR: " << year << " MN: " << month << " DAY: " << day << " DAYTIME: " << daytime << "\n";
  }

  if (fabs(s_dt - daytime) > 0.01)
  {
    cerr.precision(8);
    cerr.setf(ios::fixed, ios::floatfield);
    cerr << "Time - DMY Conversion Failure at: " << time << " Delta: " << start - time << "\n";
    cerr << "Daytime: " << daytime << "\n";
  }

  if (!dmy_to_time(day, month, year, daytime))
  {
    cerr << "Failure in functions:  dmy_to_time\n";
  }


  if (fabs(start - time) > 0.001)
  {
    cerr.precision(8);
    cerr.setf(ios::fixed, ios::floatfield);
    cerr << "Time - DMY Conversion Failure at: " << time << " Delta: " << start - time << "\n";
    cerr << "Daytime: " << daytime << "\n";
  }

}

/*$-----------------------------------------------------------------------------
Name:        gps_to_dmy

Purpose:     to convert gps time and week number
             day, month, year

Arguments:   gpstime(I) - input gps time in seconds
             gpsweek(I) - input gps time
             ref_year(I) - needed for EOW rollover computations
                           you will need an external reference for your year
             day    (O) - day of month
             month  (O) - month of year
             year   (I/O) - year AD

WARNING:   the year should be set before calling this routine to ensure
           that the END of WEEK rollover is handled.  IF it is not set
           we assume the year is 1999 (so this is only valid for 19.6 years)

Description: limitation is that this is valid for March 1990 to Feb 2100
             equations from Pages 34, 35 of GPS THEORY AND PRACTICE,
             Hofmann-Wellenhof, Lichtenegger, Collins

History:     John B. Schleppe - February 28, 1994 UTC - original code
----------------------------------------------------------------------------- */
void gps_to_dmy
(
  double gpstime,
  double gpsweek,
  short  *day,
  short  *month,
  short  *year
)
{
  double julian_daz;   /* DAYS SINCE 4713 B.C., January 1.5 */
  double a, b, c, d, e;
  double temp;
  double jd_year;
  double num_weeks = 0.0;
  double num_rolls = 0.0;

  // Sort what year it is
  if (*year < 1980)
    *year = 1999;

  // Account for GPS week crossover by computing the actual number
  // of weeks between J1980 and the 1st day of the current year
  // and then compare that to the expected number of weeks for this year
  // The number of 1024 week intervals is then computed if the gpsweeks
  // is less than the computed number of weeks

  jd_year = (367.0 * double(*year)
          - floor(7.0 * double(*year) / 4.0)
          + 31.0
          - 730531.5) + 2451545.0;

  num_weeks = floor((jd_year - 2444244.5) / 7.0);

  if (gpsweek < num_weeks)
    num_rolls = floor((num_weeks - gpsweek) / 1024.0 + 0.5);

  /* WORK OUT JULIAN DAY FROM WEEK NUMBER AND GPS TIME */

  julian_daz = gpsweek * 7.0 + num_rolls * 7168.0 + 2444244.5 + floor(gpstime / 86400.0);

  /* COMPUTE DAY, MONTH, YEAR */

  a = floor(julian_daz + 0.5);
  b = a + 1537.0;
  c = floor((b - 122.1) / 365.25);
  d = floor(365.25 * c);
  e = floor((b - d) / 30.6001);

  *day   = (short)(b - d - floor(30.6001 * e) + modf(julian_daz + 0.5, &temp));
  *month = (short)(e - 1.0 - 12.0 * floor(e / 14.0));
  *year  = (short)(c - 4715 - floor((7 + *month) / 10.0));
}


#endif

double J2K::get_gga()
{
  short hour, mins;
  double secs;

  get_hms(hour, mins, secs);

  double ret = hour * 10000 + mins * 100 + secs;
  return ret;
}
