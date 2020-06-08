/*============================================================================
  Name:        GEOCONST

  Purpose:     defines a variety of geodetic constants

  Methods:     none
 
  History:     Dave Huff - April 1998 - original code.
=============================================================================*/
#pragma once

const double RAD_TO_DEG = 57.2957795130822;
const double DEG_TO_RAD = 0.0174532925199;
const double PI         = 3.1415926535898;
const double TWOPI      = (PI * 2.0);
const double PI_BY_TWO  = (PI / 2.0);

const double MS_TO_KNOTS =  1.943844493;  /* coversion m/s to knots */
const double MS_TO_KPH   = 3.6;    /* coversion m/s to km per hour*/
const double MS_TO_MPH   = 2.236936292;  /* coversion m/s to miles per hour */


const double M_TO_FT     = 3.280839895;
const double FT_TO_M     =   0.3048;

const double M_TO_USFT   = 3.280833333;
const double USFT_TO_M   = 0.30480061;

const double RAD_TO_ARCSECS = (RAD_TO_DEG * 3600.0);
const double ARCSECS_TO_RAD = (DEG_TO_RAD / 3600.0);

#ifndef TRUE
  const short TRUE = 1;
#endif

#ifndef FALSE
  const short FALSE = 0;
#endif


const double LBS_PER_USG = 8.35;

