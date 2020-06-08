/*$============================================================================
  Name:        ELLIPSE

  Purpose:     an ellipse class with a, b, fl and e2 all defined

  Methods:     
 
  Discussion:  ELLIPSE - The class constructor
               ~ELLIPSE - The class destructor.


  History:     Dave Huff - May 1998 - original code.
=============================================================================*/
#ifndef _ELLIPSE_H_
#define _ELLIPSE_H_

#include "string.h"
#include "angle.h"

class ELLIPSE
{
private:
  char name[32];
  double a;     // semi major axis 
  double fl;    // flattening ( 1/f)
  double b;
  double e2;

public:
  ELLIPSE();
  ELLIPSE(const ELLIPSE &rhs){DoCopy(rhs);};
  ELLIPSE(double aa, double af, char *aname);
  ELLIPSE & operator =(const ELLIPSE &rhs)
  {
    if (this == &rhs)
      return *this;

    DoCopy(rhs);
    return *this;
  };

  virtual ~ELLIPSE(){};
  void DoCopy(const ELLIPSE &rhs)
  {
    strncpy(name, rhs.name, 32);
    a  = rhs.a;
    fl = rhs.fl;
    b  = rhs.b;
    e2 = rhs.e2;
  }
     
  inline double calc_b(){b =(a - a/fl);return b;};
  
  inline double calc_e2()
  {
    e2 = (a * a - b * b) / (a * a);
    return e2;
  };

  short inverse_geodetic(LAT lat1, LON lon1, LAT lat2, LON lon2,
                            ANGLE &az12, ANGLE &az21, double &dist)
  {
    short ret = 0;

    puis_inv(lat1, lon1, lat2, lon2, az12, az21, dist);

    if (dist > 100000.0)  // use vincinzis inverse for distances over 100 km
    {
      vin_inv(lat1, lon1, lat2, lon2, az12, az21, dist);
      ret = 1;
    }

    return ret;
  };

  double mer_rad_curv(double phi);
  double mer_rad_curv(LAT phi);
  double trans_rad_curv(double phi);
  double trans_rad_curv(LAT phi);

  // accessor functions.

  inline double get_a() const {return a;};
  inline double get_b() const {return b;};
  inline double get_e2() const {return e2;};
  inline double get_fl() const {return fl;};
  inline const char * get_name() const {return name;};

  void set_a(double semi_major);
  void set_b(double semi_minor);
  void set_fl(double flattening);
  void setup_ellipse(double semi_major, double flattening, char *aname);

private:

  void  vin_inv(LAT lat1, LON lon1, LAT lat2, LON lon2,
                            ANGLE &az12, ANGLE &az21, double &dist);

  void  puis_inv(LAT lat1, LON lon1, LAT lat2, LON lon2,
                            ANGLE &az12, ANGLE &az21, double &dist);

};

#endif

