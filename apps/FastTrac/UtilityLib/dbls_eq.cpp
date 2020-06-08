#include <math.h>

#include "utility.h"



/*$----------------------------------------------------------------------------
Name:        doubles_are_equal (formerly dble_comp)

Purpose:     compares two doubles to see if they are withing a certain tolerance

Arguments:   dble1 (I) - first double
             dble2 (I) - second double
             tol   (I) - tolerance

Description:  if dble1 - dble 2 <= tolerance RETURN TRUE
              if dble1 - dble2 > tolerance   RETURN FALSE

History:     John B. Schleppe  - Nov 4, 1993  - original.
             Mohamed Abousalem - Feb 24, 1994 - Name and RETURN change
---------------------------------------------------------------------------- */
short doubles_are_equal
(
  const double dble1,
  const double dble2,
  const double tol
)
{
  short rc = 1;

  if (fabs(dble1 - dble2) > tol)
    rc = 0;

  return(rc);
}

