/*============================================================================
  Name:        Utility

  Purpose:     holds the header files for the utility classes library

  Methods:     doubles_are_equal - test two doubles for equality within a 
                                   specified tolerance
               minus_PI_to_PI - converts an angle into -PI to PI range

  History:     Dave Huff - May 1998 - original code.
=============================================================================*/
#ifndef _Utility_H_
#define _Utility_H_
#pragma once


#include <fstream>
#include <string>
#include <vector>

using namespace std;

short doubles_are_equal
(
  const double dble1,
  const double dble2,
  const double tol
);

double minus_PI_to_PI(double radians);
void  delete_trailing_blanks(char *string);
void  delete_leading_blanks(char *string);
void copy_file(const char *from, const char *to);
bool FileExists(const char *fname);
bool IsInternetAvailable(); // checks that internet connection actually exists by doing a ping to google.

#include "fragment.h"
#include "angle.h"
#include "checksum.h"
	
void ParseCSV(vector<string> &record, const string& line, char delimiter);

std::string GetInterfaceAddr( const std::string &iface_name);
std::string ReplaceLastOctet(const std::string strIPAddr, const int octet);

char * StripCRLF(char * str);

#endif
