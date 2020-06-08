#include <string>
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
	char pad = 0xff;

	if((strASC.length() % 2) != 0)
		strASC += pad;

	for(size_t i=0; i < (strASC.length()/2); i++)
	{
		char  b1, b2;
		b1 = strASC[2*i];
		b2 = strASC[2*i + 1];
		data += (char)((b1 << 4) + (b2 & 0x0f));
	}

	return data;
}

//-------------------------------------------------------------------------------------------
//  FromBCD takes a string '0x12','0x34', '0x56' and returns '123456'
// 
std::string FromBCD(std::string strBCD)
{
  std::string strASC;  // the output ASCII string

	if(strBCD.length() <= 0)
	{
		return strASC;
	}
	 
  for(std::string::iterator it = strBCD.begin(); it != strBCD.end(); ++it) 
  {
  	strASC =+ (*it & 0xf0) >> 4;
  	strASC =+ (*it & 0x0f);  	
	}
	return strASC;
}
