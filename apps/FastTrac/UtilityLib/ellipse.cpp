/*$============================================================================
  Name:        ELLIPSE

  Purpose:     handles the Ellipse class which acts as a base for the 
               ellipses class.  These functions should not be called
               by anything but the ELLIPSE class.

  History:     Dave Huff - July 9, 1997 - original code.
=============================================================================*/
//#include <stdafx.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "utility.h"
#include "ellipse.h"



/*$============================================================================
  Name:        ELLIPSE

  Purpose:     class constructor for the ELLIPSE class.

  Parameters:  none
 
  History:     Dave Huff - July 9, 1997 - original code.
=============================================================================*/

ELLIPSE::ELLIPSE
(
  double aa,
  double afl,
  char *aname
)
{
  a = aa;
  fl = afl;
  calc_b();
  calc_e2();
  strncpy(name, aname, 32);
  name[31] = '\0';
}             /*End of ELLIPSE */





/*$============================================================================
  Name:        ELLIPSE

  Purpose:     The constructor for the ELLIPSE class.

  Parameters:  none.
 
  Discussion:  

  History:     Dave Huff - July 9, 1997 - original code.
=============================================================================*/

ELLIPSE::ELLIPSE()
{
  a = 6378137.000;
  fl = 298.257223563;
  strcpy(name, "WGS84");
  calc_b();
  calc_e2();
}             /*End of ELLIPSE */



/*$============================================================================
  Name:        set_a

  Purpose:     sets the value of the semi-major axis.  This will also use
               the current value of the flattening to recompute b and e2.

       NOTE::  if you want to set a and b for the ellipse you must call
               set_b AFTER set_a otherwise b will be recomputed.


  Parameters:  A (I) - the new value
 
  Returns:     void

  History:     Dave Huff - July 9, 1997 - original code.
=============================================================================*/

void ELLIPSE::set_a
(
  double semi_major
)
{
  if (doubles_are_equal(semi_major, 0.0, 0.000001))
    return;

  a = semi_major;
  calc_b();
  calc_e2();
}             /*End of set_a */





/*$============================================================================
  Name:        set_b

  Purpose:     sets the semi-minor axis value and recomputes fl and e2

       NOTE::  if you want to set a and b for the ellipse you must call
               set_b AFTER set_a otherwise b will be recomputed.

  Parameters:  b (I) - the semi-minor axis
 
  Returns:     void

  History:     Dave Huff - July 9, 1997 - original code.
=============================================================================*/

void ELLIPSE::set_b
(
  double semi_minor
)
{
  if (doubles_are_equal(semi_minor, 0.0, 0.000001))
    return;

  b = semi_minor;
  fl = a / a - b;
  calc_e2();
}             /*End of set_b */





/*$============================================================================
  Name:        set_fl

  Purpose:     sets the flattening and recomputes b and e2

  Parameters:  f (I) - the new flattening
 
  Returns:     nothing

  History:     Dave Huff - July 9, 1997 - original code.
=============================================================================*/

void ELLIPSE::set_fl
(
  double flattening
)
{
  if (doubles_are_equal(flattening, 0.0, 0.000001))
    return;

  fl  = flattening;
  calc_b();
  calc_e2();
}             /*End of set_fl */


/*$===========================================================================
  Name:        setup_ellipse

  Purpose:     given a, fl and name it loads the values and calculates
               b and e2

  Parameters:  A (I) - the a value
               Fl (I) - the flattening
               Name (I) - the name of the ELLIPSE
 
  Returns:     void

  History:     Dave Huff - July 9, 1997 - original code.
=============================================================================*/

void ELLIPSE::setup_ellipse
(
  double semi_major,
  double flattening,
  char *aname
)
{
  if (doubles_are_equal(semi_major, 0.0, 0.000001) ||
      doubles_are_equal(flattening, 0.0, 0.000001))
    return;

  a = semi_major;
  fl = flattening;
  strncpy(name, aname, 32);
  name[31] = '\0';
  calc_b();
  calc_e2();
}             /*End of setup_ellipse */


/*$===========================================================================
Name:         mer_rad_curv().

Purpose:      Compute the meridian plane radius of curvature.

Arguments:    phi (I) - passes the geodetic latitude (radians).

Description:  returns the radius of curvature (meters).

History:      Dave Huff   C++
=========================================================================== */

double  ELLIPSE::mer_rad_curv
(
  double  phi
)
{
  return((a * (1.0 - e2)) / pow(1.0 - e2 * pow(sin(phi), 2.0), 1.5));
} /* end mer_rad_curv() */


/*$===========================================================================
Name:         mer_rad_curv().

Purpose:      Compute the meridian plane radius of curvature.

Arguments:    phi (I) - passes the geodetic latitude (radians).

Description:  returns the radius of curvature (meters).

History:      Dave Huff   C++
=========================================================================== */

double  ELLIPSE::mer_rad_curv
(
  LAT  phi
)
{
  return((a * (1.0 - e2)) / pow(1.0 - e2 * pow(sin(phi.get_rad()), 2.0), 1.5));
} /* end mer_rad_curv() */




  
/*$===========================================================================
Name:         trans_rad_curv

Purpose:      Compute the transverse radius of curvature.

Arguments:    phi (I) - passes the geodetic latitude (radians).

              rad_of_curv (O) - returns the radius of curvature (meters).

Description:

History:      Dave Huff   C++
=========================================================================== */

double  ELLIPSE::trans_rad_curv
(
  double  phi
)
{
  return(a / sqrt(1.0 - e2 * pow(sin(phi), 2.0)));
} /* end trans_rad_curv() */


  
/*$===========================================================================
Name:         trans_rad_curv

Purpose:      Compute the transverse radius of curvature.

Arguments:    phi (I) - passes the geodetic latitude (radians).

              rad_of_curv (O) - returns the radius of curvature (meters).

Description:

History:      Dave Huff   C++
=========================================================================== */

double  ELLIPSE::trans_rad_curv
(
  LAT phi
)
{
  return(a / sqrt(1.0 - e2 * pow(sin(phi.get_rad()), 2.0)));
} /* end trans_rad_curv() */





/*=============================================================================
  Name:        Ellipse::vin_inv

  Purpose:     vincenzi's inverse for large distances ( > 100 km)

  Parameters:  lat1, lon1 (I) - the position of the first point
               lat2, lon2 (I) - the position of the second point
               az12 (O) - the azimuth from point 1 to point 2
               az21 (O) - the azimuth from point 2 to point 1
               dist (O) - the distance between the two points

  Returns:     void

  History:     Dave Huff - June 1998 - original code.
=============================================================================*/

void ELLIPSE::vin_inv
(
  LAT lat1,
  LON lon1,
  LAT lat2,
  LON lon2,
  ANGLE &az12,
  ANGLE &az21,
  double &dist
)
{
  double flat;
  double trlat_1, trlat_2;   /* tangent of reduced latitude           */
  double srlat_1, srlat_2;   /* sine of reduced latitude              */
  double crlat_1, crlat_2;   /* cosine of reduced latitude            */
  double t0, t1, t2, t3, t4;
  double sa, ca_2;
  double sig, dsig;
  double b1;
  double csm;
  double cs, ss;
  double dlon, dlon0, old_dlon;
  double cl;
  double srlat_sq;

  if (fabs(lat1.get_rad() - lat2.get_rad()) < 1.0e-11 &&
         fabs(lon1.zero_to_2_pi() - lon2.zero_to_2_pi()) < 1.0e-11)
  {
    dist  = 0.0;
    az12.set_rad(0.0);
    az21.set_rad(PI);
    return;
  }

  if (fabs(lat1.get_rad()) < 1.0e-11 && fabs(lat2.get_rad()) < 1.0e-11)
  {
    dlon = lon2.zero_to_2_pi() - lon1.zero_to_2_pi();
  
    if (dlon > PI)
      dlon -= TWOPI;
    else if (dlon < -PI)
      dlon += TWOPI;
  
    dist = fabs(dlon) * a;
  
    if (dlon < 0.0)
    {
      az12.set_rad(3.0 * PI_BY_TWO);
      az21.set_rad(PI_BY_TWO);
    }
    else
    {
      az12.set_rad(PI_BY_TWO);
      az21.set_rad(3.0 * PI_BY_TWO);
    }

    return;
  }


/*---------Calculate reduced latitude------------*/

  flat    = 1.0 / fl;

  trlat_1 = tan(lat1.get_rad()) * (1.0 - flat);
  t0      = atan(trlat_1);
  srlat_1 = sin(t0);
  crlat_1 = cos(t0);

  trlat_2 = tan(lat2.get_rad()) * (1.0 - flat);
  t0      = atan(trlat_2);
  srlat_2 = sin(t0);
  crlat_2 = cos(t0);


// Calculate first approximate for difference in longitude */

  dlon0  = lon2.get_rad() - lon1.get_rad();
  dlon   = dlon0;

// Iterate solution
  do
  {
    old_dlon = dlon;
    cs    = srlat_1 * srlat_2  +  crlat_1 * crlat_2 * cos(dlon);
    ss    = sqrt(pow((crlat_2 * sin(dlon)), 2.0) + pow((crlat_1 * srlat_2 - srlat_1 * crlat_2 * cos(dlon)), 2.0));

    if (fabs(cs) < 0.10)
      sig = asin(ss);
    else
      sig = atan2(ss, cs);

    sa    = crlat_1 * crlat_2 * sin(dlon) / ss;
    ca_2  = 1.0 - sa * sa;
    srlat_sq =  2.0 * srlat_1 * srlat_2;

    if (fabs(srlat_sq) < 1.0e-12 || fabs(ca_2) < 1.0e-12)
      csm = cs;
    else
      csm   = cs  -  2.0 * srlat_1 * srlat_2 / ca_2;

    cl    = flat / 16.0 * ca_2 * (4.0 + (flat * (4.0 - 3.0 * ca_2)));
    dlon  = dlon0 + (1.0 - cl) * flat * sa * (sig + cl * ss *
             (csm + cl * cs * (-1.0 + 2.0 * csm * csm)));

  } while (fabs(dlon - old_dlon) > 10.e-10);

  // Calculate the solution parameters

  t0     = ca_2 * (a * a - b * b) / (b * b);
  t1     = -768.0  +  t0 * (320.0 - 175.0*t0);
  t2     = -128.0  +  t0 * ( 74.0 -  47.0*t0);
  b1     = t0 / 1024.0 * (256.0 + t0*t2);

  // bug found in t3

  t3     = b1 * csm / 6.0 * (-3.0 + 4.0 * ss * ss) * (-3.0 + 4.0 * csm * csm);
  t4     = cs * (-1.0 + 2.0 * csm * csm);
  dsig   = b1 * ss * (csm +  b1 / 4.0 * (t4 - t3));


  // Calculate the length of geodesic

  dist  = fabs(b * (sig - dsig) * (1.0 + (t0 / 16384.0) * (4096.0 + t0*t1)));

  // Calculate the azimuth from 1 to 2

  az12.zero_to_2_pi(atan2((crlat_2 * sin(dlon)) ,
                            (crlat_1 * srlat_2 - srlat_1 * crlat_2 * cos(dlon))));

  // calculate azimuth from 2 to 1

  az21.zero_to_2_pi(atan2((-1.0*sa) , (srlat_1 * ss - crlat_1 *
                                            cs * cos(az12.get_rad()))));
}             /*End of Ellipse::vin_inv */




/*=============================================================================
  Name:        Ellipse::puis_inv

  Purpose:     Puissant's inverse for small distances (< 100 km)
               This routine computes the distance and forward & back
               azimuth given the geographic coordinates (Lat & Long) of
               two points. The distance computed is geodesic length
               (i.e. sea-level ground distance).
               Reference :
                    "A Manual for Geodetic Position Computations
                      in the Maritime Provinces"
                    by Thompson, Krakiwsky, and Adams
                    The University of Calgary

  Parameters:  lat1, lon1 (I) - the position of the first point
               lat2, lon2 (I) - the position of the second point
               az12 (O) - the azimuth from point 1 to point 2
               az21 (O) - the azimuth from point 2 to point 1
               dist (O) - the distance between the two points

  Returns:     void

  History:     Dave Huff - June 1998 - original code.
=============================================================================*/

void ELLIPSE::puis_inv
(
  LAT lat1,
  LON lon1,
  LAT lat2,
  LON lon2,
  ANGLE &az12,
  ANGLE &az21,
  double &dist
)
{
  short done;

  double n1, m1, n2, w, w1, w2, w3, w4, old_az;
  double dlat, dlon;
  double t1, t2;
  double sinlat1, sinlat1sqr, coslat2, tanlat1;
  double sinazsqr, dcubed;
  double sinaz, cosaz;

  dlat = lat2.get_rad() - lat1.get_rad();
  dlon = lon2.zero_to_2_pi() - lon1.zero_to_2_pi();

  coslat2    = cos(lat2.get_rad());
  sinlat1    = sin(lat1.get_rad());
  tanlat1    = tan(lat1.get_rad());

  sinlat1sqr = sinlat1 * sinlat1;

  w  = sqrt(1.0 - e2 * sinlat1sqr);
  n1 = a / w;
  m1 = a * (1.0 - e2) / pow(w, 3.0);

  w  = sqrt(1.0 - e2 * pow(sin(lat2.get_rad()), 2.0));
  n2 = a / w;
  w2 = 6.0 * n2 * n2;
  w3 = (1.0 + 3.0 * pow(tanlat1, 2.0)) / (6.0 * n1 * n1);


  if (dlon > PI)
    dlon -= TWOPI;
  else if (dlon < -PI)
    dlon += TWOPI;

  w = 1.0 - (3.0 * e2 * sinlat1 * cos(lat1.get_rad()) * dlat) /
            (2.0 * (1.0 - e2 * sinlat1sqr));

  w4 = dlat * m1 / w;
  w1 = dlon * n2 * coslat2;

  az12.zero_to_2_pi(atan2(w1 , (dlat * m1 / w)));

  sinaz = sin(az12.get_rad());

  if (doubles_are_equal(sinaz, 0.0, 1.0e-12))
  {
    cosaz = cos(az12.get_rad());
    dist  = (dlat / cosaz) * (m1 / w);
  }
  else
    dist  = w1 / sinaz;

  if (dist > 100000.0)  /* algorithm breaks down if dist > 100 km */
    return;

  done = 0;

  do  // iterate to get the azimuth
  {
    old_az = az12.get_rad();
    sinazsqr = sinaz * sinaz;
    dcubed = pow(dist, 3.0);

    t1 = w1 + dcubed * sinaz / w2 -
              dist * pow(sinaz, 3.0) / (w2 * coslat2 * coslat2);

    t2 = w4 + pow(dist, 2.0) * tanlat1 * sinazsqr / (2.0 * n1) +
         dcubed * cos(az12.get_rad()) * sinazsqr * w3;

    az12.zero_to_2_pi(atan2(t1 , t2));

    sinaz = sin(az12.get_rad());

    if (doubles_are_equal(sinaz, 0.0, 1.0e-12))
    {
      cosaz = cos(az12.get_rad());
      dist  = t2 / cosaz;
    }
    else
      dist  = t1 / sinaz;

    if (fabs(old_az - az12.get_rad()) < 1.0e-9)
      done = 1;

  } while (!done);

  dist = fabs(dist);

  /* calculate the back azimuth */

  w = sin((lat1.get_rad() + lat2.get_rad()) / 2.0) * cos(dlat / 2.0);
  az21.zero_to_2_pi((dlon * w + (pow(dlon, 3.0) / 12.0) *
                           (w - pow(w,3.0))) + PI + az12.get_rad());
}             /*End of Ellipse::puis_inv */





#ifdef _TEST_ELLIPSE_

#include <iostream.h>
#include "ell_list.h"
#include "Datum.h"
#include "DatumList.h"

#include "coord.h"
/*=============================================================================
  Name:        main

  Purpose:     

  Parameters:  

  Returns:     

  History:     Dave Huff - June 1998 - original code.
=============================================================================*/

void main()
{
  LAT lat1, lat2;
  LON lon1, lon2;

  ANGLE az12, az21;
  double dist1, dist2;
  double phi1, phi2, lambda1, lambda2;
  double az_12, az_21;
  ELLIPSE el;
  el = ellipses.get_ellipse(E_WGS84);

  EL_parms el2;

  el2.a = el.get_a();
  el2.b = el.get_b();
  el2.fl = el.get_fl();
  el2.e2 = el.get_e2();

  for (short i = 0; i < 360; i++)
  {
    for (short j = -90; j < 90; j++)
    {
      phi1 = (double)j * DEG_TO_RAD;
      phi2 = (double)(j + .01 * j) * DEG_TO_RAD;
      lambda1 = (double)i * DEG_TO_RAD;
      lambda2 = (double)(i + i * .002) * DEG_TO_RAD;

      lat1.set_rad(phi1);
      lat2.set_rad(phi2);
      lon1.set_rad(lambda1);
      lon2.set_rad(lambda2);

      el.inverse_geodetic(lat1, lon1, lat2, lon2, az12, az21, dist1);
      inverse_geodetic(&el2, phi1, lambda1, &az_12, &dist2, &az_21, phi2, lambda2);

      if (!doubles_are_equal(az12.get_rad(), az_12, .0001)&&
          !doubles_are_equal(az21.get_rad(), az_12, .0001) &&
          !doubles_are_equal(dist1, dist2, .001))
      {
        cerr << phi1 * RAD_TO_DEG << "  " << lambda1 << "  ";
        cerr << phi2 * RAD_TO_DEG << "  " << lambda2 << "\n";
        cerr << "ERROR: (" << i <<", "<< j << ")" << dist2 << "  " << az12.get_dd() << "  " << az21.get_dd() << "\n";
      }
    }
    cerr << "OK: (" << i <<", "<< j << ")" << "\n";
  }
  POSITION pos, out_pos;

  double x, y, z;
  LAT lat;
  LON lon;

  XYZ xyz;
  Datum datum;

  datum = datums.get_datum(D_NAD27_CANADA);

  // use the following to compare to the old c libraries stuff

  DATUM d; 
  d.ellip.a = datum.get_ellipse().get_a();
  d.ellip.b = datum.get_ellipse().get_b();
  d.ellip.fl = datum.get_ellipse().get_fl();
  d.ellip.e2 = datum.get_ellipse().get_e2();
  d.cart.cm_off_x = 0.0;
  d.cart.cm_off_y = 0.0;
  d.cart.cm_off_z = 0.0;
  d.cart.rot_x = datum.get_rx();
  d.cart.rot_y = datum.get_ry();
  d.cart.rot_z = datum.get_rz();
  d.cart.sf = 0.0;

  for (i = 0; i < 360; i++)
  {
    for (short j = -90; j < 90; j++)
    {
      phi1 = (double)j * DEG_TO_RAD;
      lambda1 = (double)i * DEG_TO_RAD;

      pos.set_horz_position(phi1, lambda1, 0.0, 0.0);
      pos.set_heights(0.0, 0.0, 0.0);

      datum.get_ellipse().pos_to_xyz(pos, xyz);
      datum.get_ellipse().xyz_to_pos(xyz, out_pos);

      if (!doubles_are_equal(out_pos.get_latitude().get_rad(), phi1, .0001)&&
          !doubles_are_equal(out_pos.get_longitude().get_rad(), lambda1, .0001) &&
          !doubles_are_equal(out_pos.get_h(), 0.0, .001))
      {
        cerr << phi1 * RAD_TO_DEG << "  " << lambda1 << "  ";
        cerr << "ERROR: (" << i <<", "<< j << ")" << "\n";
      }

      plh_to_xyz(&d, phi1, lambda1, 0.0, &x, &y, &z);
 
      if (!doubles_are_equal(x, xyz.x, .0001)&&
          !doubles_are_equal(y, xyz.y, .0001) &&
          !doubles_are_equal(z, xyz.z, .001))
      {
        cerr << "ERROR: (" << i <<", "<< j << ") x " <<  x << " y" << y << " z: " << z <<"\n";
      }
    }
    cerr << "OK: (" << i <<", "<< j << ")" << "\n";
  }
}             /*End of main */

#endif
