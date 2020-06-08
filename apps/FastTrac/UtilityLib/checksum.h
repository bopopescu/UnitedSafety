/*=============================================================================
  Class:       CHECKSUM

  Purpose:     

  History:     Dave Huff - April 1999 - original code.
=============================================================================*/
#ifndef _CHECKSUM_H_
#define _CHECKSUM_H_

#include "ats-string.h"

class CHECKSUM
{
public:          // methods
  enum CS_STATES   // CHECK SUM STATES
  {
    NO_CHECKSUM,
    BAD_CHECKSUM,
    VALID_CHECKSUM
  };

  CHECKSUM(){};         // constructor.
  ~CHECKSUM(){};        // destructor.

  bool add_checksum(char *buf, short max_len);  // return false if not enough room
  void add_checksum(ats::String &buf);
  void remove_checksum(char *buf);
  CS_STATES is_valid(const char *buf);


private:
  char atohex(char c);  // convert ascii value to hex
};

#endif
