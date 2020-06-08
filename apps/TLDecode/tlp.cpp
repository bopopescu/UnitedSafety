#include "tlp.h"

std::map <std::string, TLPDataType> TLP::m_DataTypes = TLP::SetTypes();

std::map <std::string, TLPDataType> TLP::SetTypes()
{
	std::map <std::string, TLPDataType> myMap;
	myMap["unsignedByte"]  = tlpDT_unsignedByte;
	myMap["byte"]  = tlpDT_byte;
	myMap["char"]  = tlpDT_char;
	myMap["short"]  = tlpDT_short;
	myMap["unsignedShort"]  = tlpDT_unsignedShort;
	myMap["tenthsShort"]  = tlpDT_tenthsShort;
	myMap["tenthsUShort"]  = tlpDT_tenthsUShort;
	myMap["hundredthsShort"]  = tlpDT_hundredthsShort;
	myMap["hundredthsUShort"]  = tlpDT_hundredthsUShort;
	myMap["int"]  = tlpDT_int;
	myMap["long"]  = tlpDT_long;
	myMap["tenthsInt"]  = tlpDT_tenthsInt;
	myMap["tenthsUInt"]  = tlpDT_tenthsUInt;
	myMap["hundredthsInt"]  = tlpDT_hundredthsInt;
	myMap["hundredthsUInt"]  = tlpDT_hundredthsUInt;
	myMap["unsignedInt"]  = tlpDT_unsignedInt;
	myMap["unsignedLong"]  = tlpDT_unsignedLong;
	myMap["float"]  = tlpDT_float;
	myMap["dateTime"]  = tlpDT_dateTime;
	myMap["latitude"]  = tlpDT_latitude;
	myMap["longitude"]  = tlpDT_longitude;
	myMap["altitude"]  = tlpDT_altitude;
	myMap["map"]  = tlpDT_map;
	myMap["map2"]  = tlpDT_map2;
	myMap["rssi"]  = tlpDT_rssi;
	myMap["count"]  = tlpDT_count;
	myMap["string"]  = tlpDT_string;
	myMap["class"]  = tlpDT_class;
	return myMap;
};


//A Trulink protocol encoding/decoding class
TLP::TLP()
{
}


