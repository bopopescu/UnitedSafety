/*-----------------------------------------------------------------------------
  Program:      FRAGMENT

  Purpose:      a fragment class for the fragmenting of a string into
                substrings based on a delimiter

  Methods:      Fragment - a constructor
                ~Fragment - the desctructor
                fragment - fragments the string
                item - overloaded function the converts one of the items
                num_items - returns the number of items.
       

  History:      David Huff - March 15, 1995 - Original code.
-----------------------------------------------------------------------------*/
#include <string.h>
#include <stdlib.h>


#include "fragment.h"


/*-----------------------------------------------------------------------------
  Program:      item

  Purpose:      return a short value

  Arguments:    index (I) - the index into the string items
                val (O) - the returned short

  Description:  overloaded for all datatypes
                 RETURNS: 0 if unable to get item
                          1 if item returned.

  History:      David Huff - March 15, 1995 - Original code.
-----------------------------------------------------------------------------*/
short FRAGMENT::item
(
  const short index,
  unsigned short *val
)
{
  if (index < 0 || index >= num_items)
    return 0;

  *val = (unsigned short)atoi(ptrs[index]);
  return 1;
}             /*End of item */

short FRAGMENT::item
(
  const short index,
  unsigned short &val
)
{
  if (index < 0 || index >= num_items)
    return 0;

  val = (unsigned short)atoi(ptrs[index]);
  return 1;
}             /*End of item */




/*-----------------------------------------------------------------------------
  Program:      item

  Purpose:      return a short value

  Arguments:    index (I) - the index into the string items
                val (O) - the returned short

  Description:  overloaded for all datatypes
                 RETURNS: 0 if unable to get item
                          1 if item returned.

  History:      David Huff - March 15, 1995 - Original code.
-----------------------------------------------------------------------------*/
short FRAGMENT::item
(
  const short index,
  short *val
)
{
  if (index < 0 || index >= num_items)
    return 0;

  *val = (short)atoi(ptrs[index]);
  return 1;
}             /*End of item */

short FRAGMENT::item
(
  const short index,
  short &val
)
{
  if (index < 0 || index >= num_items)
    return 0;

  val = (short)atoi(ptrs[index]);
  return 1;
}             /*End of item */




/*-----------------------------------------------------------------------------
  Program:      item

  Purpose:      return a short value

  Arguments:    index (I) - the index into the string items
                val (O) - the returned short

  Description:  overloaded for all datatypes
                 RETURNS: 0 if unable to get item
                          1 if item returned.

  History:      David Huff - March 15, 1995 - Original code.
-----------------------------------------------------------------------------*/
short FRAGMENT::item
(
  const short index,
  int *val
)
{
  if (index < 0 || index >= num_items)
    return 0;

  *val = (int)atoi(ptrs[index]);
  return 1;
}             /*End of item */


short FRAGMENT::item
(
  const short index,
  int &val
)
{
  if (index < 0 || index >= num_items)
    return 0;

  val = (int)atoi(ptrs[index]);
  return 1;
}             /*End of item */


/*-----------------------------------------------------------------------------
  Program:      item

  Purpose:      return a long value

  Arguments:    index (I) - the index into the string items
                val (O) - the returned long

  Description:  overloaded for all datatypes
                 RETURNS: 0 if unable to get item
                          1 if item returned.

  History:      David Huff - March 15, 1995 - Original code.
-----------------------------------------------------------------------------*/
short FRAGMENT::item
(
  const short index,
  long *val
)
{
  if (index < 0 || index >= num_items)
    return 0;

  *val = atol(ptrs[index]);
  return 1;
}             /*End of item */

short FRAGMENT::item
(
  const short index,
  long &val
)
{
  if (index < 0 || index >= num_items)
    return 0;

  val = atol(ptrs[index]);
  return 1;
}             /*End of item */




/*-----------------------------------------------------------------------------
  Program:      item

  Purpose:      return a float value

  Arguments:    index (I) - the index into the string items
                val (O) - the returned float

  Description:  overloaded for all datatypes
                 RETURNS: 0 if unable to get item
                          1 if item returned.

  History:      David Huff - March 15, 1995 - Original code.
-----------------------------------------------------------------------------*/
short FRAGMENT::item
(
  const short index,
  float *val
)
{
  if (index < 0 || index >= num_items)
    return 0;

  *val = (float)atof(ptrs[index]);
  return 1;
}             /*End of item */

short FRAGMENT::item
(
  const short index,
  float &val
)
{
  if (index < 0 || index >= num_items)
    return 0;

  val = (float)atof(ptrs[index]);
  return 1;
}             /*End of item */




/*-----------------------------------------------------------------------------
  Program:      item

  Purpose:      return a double value

  Arguments:    index (I) - the index into the string items
                val (O) - the returned double

  Description:  overloaded for all datatypes
                 RETURNS: 0 if unable to get item
                          1 if item returned.

  History:      David Huff - March 15, 1995 - Original code.
-----------------------------------------------------------------------------*/
short FRAGMENT::item
(
  const short index,
  double *val
)
{
  if (index < 0 || index >= num_items)
    return 0;

  *val = atof(ptrs[index]);
  return 1;
}             /*End of item */

short FRAGMENT::item
(
  const short index,
  double &val
)
{
  if (index < 0 || index >= num_items)
    return 0;

  val = atof(ptrs[index]);
  return 1;
}             /*End of item */




/*-----------------------------------------------------------------------------
  Program:      item

  Purpose:      return a double value

  Arguments:    index (I) - the index into the string items
                val (O) - the returned double

  Description:  overloaded for all datatypes
                 RETURNS: 0 if unable to get item
                          1 if item returned.

  History:      David Huff - March 15, 1995 - Original code.
-----------------------------------------------------------------------------*/
short FRAGMENT::item
(
  const short index,
  bool *val
)
{
  if (index < 0 || index >= num_items)
    return 0;

  short v;
  v = (short)atoi(ptrs[index]);

  if (v)
    *val = true;
  else
    *val = false;
  return 1;
}             /*End of item */

short FRAGMENT::item
(
  const short index,
  bool &val
)
{
  if (index < 0 || index >= num_items)
    return 0;

  short v;
  v = (short)atoi(ptrs[index]);
  
  if (v)
    val = true;
  else
    val = false;
  return 1;
}             /*End of item */




/*-----------------------------------------------------------------------------
  Program:      item

  Purpose:      return a char value

  Arguments:    index (I) - the index into the string items
                val (O) - the returned char

  Description:  overloaded for all datatypes
                 RETURNS: 0 if unable to get item
                          1 if item returned.

  History:      David Huff - March 15, 1995 - Original code.
-----------------------------------------------------------------------------*/
short FRAGMENT::item
(
  const short index,
  char *val
)
{
  if (index < 0 || index >= num_items)
    return 0;

  strcpy(val, ptrs[index]);
  return 1;
}             /*End of item */




#ifdef _AFXDLL

/*-----------------------------------------------------------------------------
  Program:      item

  Purpose:      return a char value

  Arguments:    index (I) - the index into the string items
                val (O) - the returned char

  Description:  overloaded for all datatypes
                 RETURNS: 0 if unable to get item
                          1 if item returned.

  History:      David Huff - March 15, 1995 - Original code.
-----------------------------------------------------------------------------*/
short FRAGMENT::item
(
  const short index,
  CString &val
)
{
  if (index < 0 || index >= num_items)
    return 0;

  val = ptrs[index];
  return 1;
}             /*End of item */

#endif



/*-----------------------------------------------------------------------------
  Program:      fragment

  Purpose:      fragment the input string

  Arguments:    in_str (I) - the input string

  Description:  fragments the input string based on a delimiter of ' '.

  History:      David Huff - March 15, 1995 - Original code.
-----------------------------------------------------------------------------*/

short FRAGMENT::fragment
(
  const char *in_str
)
{
  short len;
  len = (short)strlen(in_str) + 1;

  if (len == 0)
    return 0;

  delim = ' ';

  if (work)
    delete []work;

  work = new char[len];

  if (!work)
    return 0;

  strcpy(work, in_str);
  work[len] = '\0';
  
  char * p;
  if ((p = strchr(work, '\n')) != NULL)
    *p = '\0';

  num_items = 0;

  for (int i = 0; i < len; i++)
    if (work[i] == delim)
      num_items++;

  num_items++;

  if (ptrs)
    delete []ptrs;

  ptrs = new char *[num_items];

  if (!ptrs)
    return 0;

  frag();

  return 1;
}             /*End of fragment */





/*-----------------------------------------------------------------------------
  Program:      fragment

  Purpose:      fragment the input string

  Arguments:    in_str (I) - the input string
                in_delim (I) - the input delimiter

  Description:  fragments the input string based on the input delimiter.

  History:      David Huff - March 15, 1995 - Original code.
-----------------------------------------------------------------------------*/

short FRAGMENT::fragment
(
  const char *in_str,
  const char in_delim
)
{
  short len;
  len = (short)strlen(in_str);

  if (len == 0)
    return 0;

  delim = in_delim;

  if (work)
    delete []work;

  work = new char[len + 1];

  if (!work)
    return 0;

  strncpy(work, in_str, len);
  char * p;
  if ((p = strchr(work, '\n')) != NULL)
    *p = '\0';

  work[len] = '\0';
  num_items = 0;

  for (int i = 0; i < len; i++)
    if (work[i] == delim)
      num_items++;

  num_items++;

  if (ptrs)
    delete []ptrs;
    
  ptrs = new char *[num_items];

  if (!ptrs)
    return 0;

  frag();
  return 1;
}             /*End of fragment */

/*-----------------------------------------------------------------------------
  Program:      fragment

  Purpose:      fragment the input string

  Arguments:    in_str (I) - the input string
                in_delim (I) - the input delimiter
                num_frag (I) - the number of items to fragment into

  Description:  fragments the input string based on a delimiter of ' ' into 
                the entered number of fragments.  The final item will
                contain the rest of the string.

  History:      David Huff - March 15, 1995 - Original code.
-----------------------------------------------------------------------------*/

short FRAGMENT::fragment
(
  const char *in_str,
  const char in_delim,
  const short num_frag
)
{
  short len;
  len = (short)strlen(in_str);

  if (len == 0)
    return 0;

  delim = in_delim;

  if (work)
    delete []work;

  work = new char[len + 1];

  if (!work)
    return 0;

  strcpy(work, in_str);

  char *p;
  
  if ((p = strchr(work, '\n')) != NULL)
    *p = '\0';

  work[len] = '\0';
  num_items = num_frag;

  if (ptrs)
    delete []ptrs;
    
  ptrs = new char *[num_items];

  if (!ptrs)
    return 0;

  frag();
  return 1;
}             /*End of fragment */




/*-----------------------------------------------------------------------------
  Program:      FRAGMENT

  Purpose:      constructor for the FRAGMENT class

  Arguments:    none

  Description:  initializes the variables for the FRAGMENT class

  History:      David Huff - March 15, 1995 - Original code.
-----------------------------------------------------------------------------*/

FRAGMENT::FRAGMENT()
{
  num_items = 0;
  work = NULL;
  ptrs = NULL;
  delim = ' ';
}             /*End of FRAGMENT */




/*-----------------------------------------------------------------------------
  Program:      ~FRAGMENT

  Purpose:      Destructor for the fragment class

  Arguments:    none

  Description:  frees up the allocated memory for the class

  History:      David Huff - March 15, 1995 - Original code.
-----------------------------------------------------------------------------*/

FRAGMENT::~FRAGMENT()
{
  if (ptrs)
    delete []ptrs;

  if (work)
    delete []work;
}             /*End of ~FRAGMENT */




/*-----------------------------------------------------------------------------
  Program:      frag

  Purpose:      fragments the work buffer based on the current delimiter
                and num_items value.

  Arguments:    

  Description:  fragments the work buffer.

  History:      David Huff - March 15, 1995 - Original code.
-----------------------------------------------------------------------------*/

void FRAGMENT::frag()
{
  short i;

  if (num_items == 0 || work == NULL || ptrs == NULL)
    return;

  i = (short)(num_items - 1);

  ptrs[0] = &work[0];

  for (i = 1; i < num_items; i++)
  {
    ptrs[i] = strchr(ptrs[i - 1], delim);
    *ptrs[i] = '\0';
    ptrs[i]++;
  }
}             /*End of frag */
/*-----------------------------------------------------------------------------
  Program:      squeeze

  Purpose:      squeeze the input string of multiple delimiters

  Arguments:    in_str (I) - the input string
                in_delim (I) - the input delimiter

  Description:  eliminates multiple delimiters from the input string based
                on the input delimiter.

  History:      David Huff - March 15, 1995 - Original code.
-----------------------------------------------------------------------------*/

void FRAGMENT::squeeze
(
  char *in_str,
  const char in_delim
)
{
  char *p, *new_start = NULL;

  p = in_str;

  while ((*p != '\0') && (*p == in_delim))
  {
    if (new_start == NULL)
      new_start = p;

    p++;
  }

  if (*p == '\0')
  {
    in_str[0] = '\0';
    return;
  }

  if (new_start != NULL)
    p = strcpy(new_start, p) + 1;

  while (*p != '\0')
  {
    if (*p == in_delim)
    {                          /* Save place to move rest of string to */
      new_start = ++p;

      while ((*p != '\0') && (*p == in_delim))
        p++;

      if (new_start != p)
        p = strcpy(new_start, p) + 1;
    }
    else
      p++;
  }

  if (*(p - 1) == in_delim)
    *(p - 1) = '\0';
}             /*End of squeeze */




/*=============================================================================
  Name:        get_item_len

  Purpose:     returns the length of an item assuming it is a char string

  Parameters:  index (I) - the index of the item

  Returns:     the length

  History:     Dave Huff - June 1998 - original code.
=============================================================================*/

short FRAGMENT::get_item_len
(
  const short index
)
{
  if (index < 0 || index >= num_items)
    return 0;
  else
    return(strlen(ptrs[index]));
}             /*End of get_item_len */



short FRAGMENT::item
(
  const short index,
  char *val,
  const short max_len
)
{
  if (index < 0 || index >= num_items || strlen(ptrs[index]) == 0)
    return 0;

  strncpy(val, ptrs[index], max_len);
  val[max_len - 1] = '\0';
  return 1;
}             /*End of item */

