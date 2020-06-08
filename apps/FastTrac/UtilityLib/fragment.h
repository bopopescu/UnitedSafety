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
#ifndef _FRAGMENT_HPP_
#define _FRAGMENT_HPP_

#include <string.h>

class FRAGMENT
{
  private: 
    short num_items;  /* number of items fragmented into */
    char *work;       /* working string                   */
    char **ptrs;      /* allocated array of char pointers */
    char delim;

    void frag();

  public:
    short get_item_len(const short index);
    inline short get_num_items() const {return num_items;};
    short item(const short index, unsigned char *val);
    short item(const short index, unsigned short *val);
    short item(const short index, unsigned short &val);
    short item(const short index, short *val);
    short item(const short index, short &val);
    short item(const short index, long *val);
    short item(const short index, long &val);
    short item(const short index, float *val);
    short item(const short index, float &val);
    short item(const short index, double *val);
    short item(const short index, double &val);
    short item(const short index, char *val);
    short item(const short index, char *val, short max_len);
    short item(const short index, int *val);
    short item(const short index, int &val);
    short item(const short index, bool *val);
    short item(const short index, bool &val);
#ifdef _AFXDLL
    short item(short index, CString &val);
#endif
    short fragment(const char *in_str);
    short fragment(const char *in_str, const char in_delim);
    short fragment(const char *in_str, const char in_delim, const short num_frag);
    void  squeeze(char *in_str, const char in_delim);
    FRAGMENT();
    ~FRAGMENT();
    FRAGMENT(FRAGMENT &rhs);
};

#endif
