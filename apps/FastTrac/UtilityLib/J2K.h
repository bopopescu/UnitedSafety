/*$============================================================================
  Name:        J2K

  Purpose:     Multipurpose date/time with conversion functions between
               J2K time, dd/mm/yy and UTC, along with week, year, time


  Methods:     Time is determined relative to 2000 January 1.5d
               This is a standard epoch.
               See Hofmann-Wellenhof, Lichtenegger and Collins
               GPS Theory and Practice, Pgs 38-42.

               Remember, unlike C arrays, the first day =1, first month =1

  History:     John Schleppe - May, 1998 -  original code.
==============================================================================*/

#ifndef _J2K_H_
#define _J2K_H_

//#include "geoconst.h"
#include <math.h>

#define J2K_TRUE (1)
#define J2K_FALSE (0)

const double J1980        = 2444244.5;        // J2000 in J2K_TRUE Julian Days
const double J2000        = 2451545.0;        // J2000 in J2K_TRUE Julian Days
const double SEC_PER_DAY  = 86400.0;          // Number of seconds per day
const double SEC_PER_WEEK = 604800.0;

class J2K
{
private:
  double time;                      // seconds and dec. secs from 2000 Jan 1.5d
  short  valid;                     // set to J2K_TRUE if valid times are present

public:
  J2K();                                         // constructors
  J2K(const double t);                                 // given the J2K time
  J2K(const short y, const short m, const short d, const double day_t);  // given day, month,year,daytime
  J2K(const short y, const short m, const short d,
                       const short h, const short min, const double s);
  J2K(const short y, const short w, const double gps_t);           // given year,gpsweek,gpstime

  J2K& operator +=(const double secs);                 // increment time by secs seconds
  J2K& operator -=(const double secs);                 // decrement time by secs seconds

  double operator - (const J2K & rhs) const          // give the difference between
  {                                                // two J2K classes -in seconds
    return time - rhs.time;
  };

  J2K operator + (const double add_time)        // give the sum of a J2K and
  {  
    J2K newtime = *this;                 
    newtime += add_time;

    return newtime;
  };

  double get_gga();
 
  bool operator >  (const J2K &b) const { return (time > b.time);};
  bool operator >= (const J2K &b) const { return (time >= b.time);};
  bool operator <  (const J2K &b) const { return (time < b.time);};
  bool operator <= (const J2K &b) const { return (time <= b.time);}
  bool operator == (const J2K &b) const { return (fabs(time - b.time) < 0.001);};  // 1/1000 of a second
  bool operator != (const J2K &b) const { return (fabs(time - b.time) > 0.001);};
//  friend bool operator != (const J2K &lhs, const J2K &rhs) const 
//  {
//    return(fabs(lhs.time - rhs.time) > 0.001);
//  };

  inline void   clear(void){valid = J2K_FALSE; time = 0.0;};
  inline short  is_valid(void) const {return valid;};
  inline double get_time(void) const { return time;};

  inline double get_daytime(void)
  {
    double a;
    double jd      = time / SEC_PER_DAY + J2000;
    double partial = modf(jd + 0.5, &a);
    return partial * SEC_PER_DAY;
  };

  short   get_hms(short &h, short &m, double &s);
  short   get_hms(char *buffer) ;
  short   get_hms_ds(char *buffer);
  short   get_hms_ds_nodelim(char *buffer);

  short   get_dmy(short &dy, short &mn, short &yr)  ;
  short   get_dmy(char *buffer);
  short   get_dmy_US(char *buffer);
  short   get_gps(short &day_of_week, short &gpsweek, short &year, double &gpstime);
  short   get_gregorian(short &day_of_year, short &year, double &daytime) ;

  short  set_dmy_hms(const short d, const short m, const short y, 
                   const short h, const short min, const double s);
  short  set_dmy_hms(const char *dmy, const char* hms);
  short  set_dmy_hms(const short d, const short m, const short y, const char *hms);
  short  set_dmy_daytime(const short d, const short m, const short y, const double daytime)
         {return dmy_to_time(d, m, y, daytime);};

  short  set_year_day_time(const short y, const short d, const double day_t);
  void   set_time(const double t);
  short  set_gps(const short year, const short week, const double gps_t);

  void SetSystemTime();  // Set this J2K time to the current system time.
  char * GetTimeString(char *buf, int lenBuf);// returns HH:MM:SS DD/MM/YYYY in buf

  short GetHour(){short hour, minute; double sec; get_hms(hour, minute,sec);return hour;};
  short GetMinute(){short hour, minute; double sec; get_hms(hour, minute,sec);return minute;};
  short GetSecond(){short hour, minute; double sec; get_hms(hour, minute,sec);return (short)sec;};
  short GetDay(){short day, month, year; get_dmy(day, month, year); return day;};
  short GetMonth(){short day, month, year; get_dmy(day, month, year); return month;};
  short GetYear(){short day, month, year; get_dmy(day, month, year); return year;};

#ifdef _TEST_J2K_
  void test_gps(void);
#endif

private:
  short daytime_to_hms(const double day_t, short &h, short &m, double &sec);
  short dmy_to_time(const short d, const short m, const short y, double day_t);
  short dmy_string_to_dmy(const char *dmy, short &d, short &m, short &y);
  short gps_to_time(short w, short y, double gps_t);
  short gregorian_to_time(short day, short y, double day_t);
  short hms_to_daytime(short h, short m, double s, double &day_t);
  short hms_string_to_daytime(const char *hms, double &day_t);
  short time_to_dmy(short &d, short &m, short &y, double &daytime) const;
  short time_to_gps(short &day_of_week, short &week, short &year, double &gpstime);
  short time_to_gregorian(short &day_of_year, short &year, double &daytime);

};
#endif

