#include <string>
#include "BCD.h"
//-------------------------------------------------------------------------------------------
//
//  BCD - implement conversion to and from Binary Coded Decimal
//
//  ToBCD takes a string '123456' and returns a string '0x12','0x34', '0x56'
//  FromBCD takes a string '0x12','0x34', '0x56' and returns '123456'

//-------------------------------------------------------------------------------------------
//  ToBCD takes a string '123456' and returns a string '0x12','0x34', '0x56'
//
std::string ToBCD(std::string strASC)
{
	std::string data;

	if((str.length() % 2) != 0)
		str += '0xff';

	for(int i=0; i < (str.length()/2); i++)
	{
		data[i] = (strASC[2*i] << 4) + (strASC[2*i + 1]);
	}

	return data;
}

//-------------------------------------------------------------------------------------------
//  FromBCD takes a string '0x12','0x34', '0x56' and returns '123456'
// 
std::string <char> FromBCD(const std::string <char> strBCD)
{

	if(strBCD.length() <= 0)
	{
		return data;
	}
	
  std::string strASC;  // the output ASCII string
  
  for(std::string::iterator it = strBCD.begin(); it != strBCD.end(); ++it) 
  {
  	strASC =+ (*it & 0xf0) >> 4;
  	strASC =+ (*it & 0x0f);  	
	}
	return strASC;
}
