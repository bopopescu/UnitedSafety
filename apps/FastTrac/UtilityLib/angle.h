/*============================================================================
  Name:        ANGLE

  Purpose:    defines the ANGLE, LAT and LON classes 

  Methods:     

  History:     Dave Huff - May 1998 - original code.
=============================================================================*/
#ifndef _ANGLE_H_
#define _ANGLE_H_
#include <math.h>
#include "geoconst.h"

class ANGLE
{
protected:
  double val;

public:

  ANGLE();
  ANGLE(double rad_val);
  ANGLE(const ANGLE &rhs){val = rhs.val;};
  virtual ~ANGLE();

  virtual inline short set_rad(const double ang){val = ang; return TRUE;};
  virtual inline short set_dd(const double ang){val = ang * DEG_TO_RAD;return TRUE;};
  virtual short set_dm(const char *ang);  
  virtual short set_dms(const char *dms);  // returns 1 if ok

  inline double get_rad() const {return val;};
  inline double get_dd() const {return val * RAD_TO_DEG;};

  char * get_dd(char *buf, const short len, const short ndec = 10) const;
  char * get_dm(char *buf, short len, short ndec = 7, char delim = 0) const;
  char * get_dms(char *buf, const short len, 
                   const short ndec = 4, const char delim = 0) const;

  double minus_pi_to_pi();
  double zero_to_2_pi();
  double minus_pi_to_pi(const double radians);
  double zero_to_2_pi(const double radians);

  short operator == (const ANGLE &a) const;

  ANGLE &operator=(const ANGLE &rhs)
  {
    if (this == &rhs)
      return *this;
    val = rhs.val;
    return *this;
  }
};



class LAT : public ANGLE // implemented in LAT.CPP
{
public:
  LAT(){LAT::set_rad(0.0);};
  LAT(const double ang) : ANGLE(ang){};
  char * get_ndms(char *buf, const short len, short ndec = 4, const char delim = 0) const;
  char * get_ndm(char *buf, const short len, short ndec = 7, const char delim = 0) const;
  char * get_nDDMMmm(char *buf, const short len, const char delim = 0) const;  // N 50 1234 output
  short  set_ndms(const char *dms); // returns 1 if dms string decodes
  short  set_rad(const double ang);
  short set_ndm(const char *cdms);
  
  inline short  set_dd(const double ang)
  {
    return LAT::set_rad(ang * DEG_TO_RAD);
  };
  
  short  set_dm(const char *ang);
  
  inline short  set_dms(const char *dms)
  {
    if (ANGLE::set_dms(dms))
      return LAT::set_rad(val);
                  
    return 0;
  };  // returns 1 if ok
  short operator == (const LAT &rhs) const;
  short operator != (const LAT &rhs){return (!(*this == rhs));}
  LAT& operator=(const LAT &rhs){val = rhs.val;return *this;};
};

class LON : public ANGLE // implemented in LON.CPP
{
public:
  LON(){LON::set_rad(0.0);};
  LON(const double ang) : ANGLE(ang){};
  char * get_ndms(char *buf, const short len, short ndec = 4, const char delim = 0) const;
  char * get_ndm(char *buf, const short len, short ndec = 7, const char delim = 0) const;
  char * get_nDDMMmm(char *buf, const short len,const char delim = 0) const;  // N 50 1234 output

  short  set_ndms(const char *dms); // returns 1 if dms string decodes
  short set_ndm(const char *cdms);

  inline short set_rad(const double ang)
  {
    val = minus_pi_to_pi(ang);
                                   
    if (ang > TWOPI || ang < -PI)
      return FALSE;
    else
      return TRUE;
  };

  inline short set_dd(const double ang){return(set_rad(ang * DEG_TO_RAD));};

  inline short set_dm(const char *ang){ANGLE::set_dm(ang);return set_rad(val);};  // returns the angle

  inline short  set_dms(const char *dms)
  {
    if (ANGLE::set_dms(dms))
    {
      set_rad(val);
      return 1;
    }
    return 0;
  };  // returns 1 if ok
  short operator == (const LON &rhs) const;
  short operator != (const LON &rhs){return (!(*this == rhs));};
  LON& operator=(const LON &rhs){val = rhs.val;return *this;};
};

#endif
