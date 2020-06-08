#ifndef _TLP_H
#define _TLP_H

#include <assert.h>
#include <map>
#include <iomanip>
#include <boost/algorithm/string.hpp>
#include "boost/format.hpp"

using namespace std;
using boost::format;
using boost::io::group;

#include "NMEA_Client.h"
#define GET_TLP_ENUM_FROM_NAME(P_NAME) (tlpDT_ ## P_NAME)
//A Trulink protocol encoding/decoding class
typedef enum 
{
		tlpDT_unsignedByte,
		tlpDT_byte,
		tlpDT_char,
		tlpDT_short,
		tlpDT_unsignedShort,
		tlpDT_tenthsShort,
		tlpDT_tenthsUShort,
		tlpDT_hundredthsShort,
		tlpDT_hundredthsUShort,
		tlpDT_int,
		tlpDT_long,
		tlpDT_tenthsInt,
		tlpDT_tenthsUInt,
		tlpDT_hundredthsInt,
		tlpDT_hundredthsUInt,
		tlpDT_unsignedInt,
		tlpDT_unsignedLong,
		tlpDT_float,
		tlpDT_dateTime,
		tlpDT_latitude,
		tlpDT_longitude,
		tlpDT_altitude,
		tlpDT_map,
		tlpDT_map2,
		tlpDT_rssi,
		tlpDT_count,
		tlpDT_string,//variable length
		tlpDT_class,//variable length
}TLPDataType;

class TLP
{
public:

	static std::map <std::string, TLPDataType> m_DataTypes;
	static std::map <std::string, TLPDataType> SetTypes();


	TLP();
	~TLP(){};

	
	int getTypeSize( TLPDataType type )const 
	{
		int d = -1;
		switch( type )
		{
		case tlpDT_unsignedByte:
		case tlpDT_byte:
		case tlpDT_char:
		case tlpDT_map:
		case tlpDT_rssi:
		case tlpDT_count:
			d = 1;
			break;
		case tlpDT_short:
		case tlpDT_unsignedShort:
		case tlpDT_tenthsShort:
		case tlpDT_tenthsUShort:
		case tlpDT_hundredthsShort:
		case tlpDT_hundredthsUShort:
		case tlpDT_map2:
		case tlpDT_altitude:
			d = 2;
			break;
		case tlpDT_int:
		case tlpDT_long:
		case tlpDT_tenthsInt:
		case tlpDT_tenthsUInt:
		case tlpDT_hundredthsInt:
		case tlpDT_hundredthsUInt:
		case tlpDT_unsignedInt:
		case tlpDT_unsignedLong:
		case tlpDT_float:
		case tlpDT_dateTime:
			d = 4;
			break;
		case tlpDT_latitude:
		case tlpDT_longitude:
			d = 3;
			break;
		default:
		case tlpDT_class:
		case tlpDT_string:
			break;
		}
		return d;
	}

	std::string getData(const std::vector<char>& buf, TLPDataType type, int& index ) const
	{
		std::string strOutput;
		
		switch( type )
		{
			case tlpDT_tenthsInt:
			case tlpDT_tenthsUInt:
			case tlpDT_tenthsShort:
			case tlpDT_tenthsUShort:
			{
				int data = h_getData<int>(buf, type, index);
				data = data / 10.0;
				strOutput = str( format("%d") % data);
				return strOutput;
			}
			case tlpDT_hundredthsInt:
			case tlpDT_hundredthsUInt:
			case tlpDT_hundredthsShort:
			case tlpDT_hundredthsUShort:
			{
				int data = h_getData<int>(buf, type, index);
				data = data / 100.0;
				strOutput = str( format("%d") % data);
				return strOutput;
			}
			case tlpDT_latitude:
			{
				strOutput = str( format("%.9f") % latitude(buf, index));
				return strOutput;
			}
			case tlpDT_longitude:
			{
				strOutput = str( format("%.9f") % longitude(buf, index));
				return strOutput;
			}
			case tlpDT_altitude:
			{
				strOutput = str( format("%.9f") % altitude(buf, index));
				return strOutput;
			}
			case tlpDT_rssi:
			{
				int data = h_getData<int>(buf, type, index);
				data = data*(-1);
				strOutput = str( format("%d") % data);
				return strOutput;
			}
			case tlpDT_unsignedByte:
			case tlpDT_byte:
			case tlpDT_char:
			case tlpDT_short:
			case tlpDT_unsignedShort:
			case tlpDT_int:
			case tlpDT_long:
			case tlpDT_unsignedInt:
			case tlpDT_unsignedLong:
			case tlpDT_dateTime:
			case tlpDT_map:
			case tlpDT_map2:
			case tlpDT_count:
			case tlpDT_float:
			{
				int data = h_getData<int>(buf, type, index);
				strOutput = str( format("%d") % data);
				return strOutput;
			}
			default:
				break;
		}
		return strOutput;
	}

	// setData - convert a string (data) to its appropriate data type (type) and add its encoded
	//           value to the vector (buf)
	template<typename T>
		bool setData(std::vector<char>& buf, TLPDataType type, T data )
		{
		switch( type )
		{
			case tlpDT_tenthsInt:
			case tlpDT_tenthsUInt:
			case tlpDT_tenthsShort:
			case tlpDT_tenthsUShort:
			data = data * 10.0;
			break;
			case tlpDT_hundredthsInt:
			case tlpDT_hundredthsUInt:
			case tlpDT_hundredthsShort:
			case tlpDT_hundredthsUShort:
			data = data * 100.0;
			break;
			case tlpDT_latitude:
			{
				latitude(buf, (double)data); 
				return true;
			}
			case tlpDT_longitude:
			{
				longitude(buf, (double)data); 
				return true;
			}
			case tlpDT_altitude:
			{
				altitude(buf, (double)data); 
				return true;
			}
			case tlpDT_rssi:
			data = data*(-1);
			break;
			case tlpDT_float: 
			{
				setFloat(buf, (float)data);
				return true;
			}
			case tlpDT_class:
			case tlpDT_string:
				return false;
				
			default:
				break;
		}

		h_setData(buf, type, data);
		return true;
		}

	// setData - convert a string (data) to its appropriate data type (type) and add its encoded
	//           value to the vector (buf)
	bool setData(std::vector<char>& buf, TLPDataType type, std::string data )
	{
		switch( type )
		{
			case tlpDT_unsignedByte:
			case tlpDT_byte:
			case tlpDT_char:
			case tlpDT_short:
			case tlpDT_unsignedShort:
			case tlpDT_int:
			case tlpDT_long:
			case tlpDT_unsignedInt:
			case tlpDT_unsignedLong:
			case tlpDT_dateTime:
			case tlpDT_tenthsInt:
			case tlpDT_tenthsUInt:
			case tlpDT_tenthsShort:
			case tlpDT_tenthsUShort:
			case tlpDT_hundredthsInt:
			case tlpDT_hundredthsUInt:
			case tlpDT_hundredthsShort:
			case tlpDT_hundredthsUShort:
			case tlpDT_rssi:
			case tlpDT_count:
			case tlpDT_map:
			case tlpDT_map2:
				int val;
				val = atoi(data.c_str());
				return setData<int>(buf, type, val);
			case tlpDT_latitude:
			case tlpDT_longitude:
			case tlpDT_altitude:
			{
				double val = atof(data.c_str());
				return setData<double>(buf, type, val);
			}
			case tlpDT_float: 
			{
				float val = (float)atof(data.c_str());
				return setData<float>(buf, type, val);
			}
			case tlpDT_class:
			case tlpDT_string:
				return false;
			default:
				break;
		}
		return false;
	}

	void latitude(std::vector<char>& buf, double lat = 0.0)
	{
		int iLat;
		if( lat == 0.0 )
		{
		lat = m_GPS.Lat();
		}

		iLat = (long)((90.0 - lat) * 93206.75);

		h_setData(buf, tlpDT_latitude, iLat);
	}

	double latitude( const std::vector<char>& buf, int &index ) const
	{
		assert( index + getTypeSize(tlpDT_latitude) <= (int)buf.size() ); 
		char d[8]={0};

		for( int i = index; i < index + getTypeSize(tlpDT_latitude) ; ++i)
		{
		memcpy(d+i-index, &buf[i], 1);
		}

		return (double)(90.0- ((*(int*)d)/93206.75));
	}


	double longitude(const std::vector<char>& buf, int& index ) const
	{
		assert( index + getTypeSize(tlpDT_longitude) <= (int)buf.size() ); 
		char d[8]={0};

		for( int i = index; i < index + getTypeSize(tlpDT_longitude) ; ++i)
		memcpy(d+i-index, &buf[i], 1);

		double f = ((*(int*)d)/46603.375);
		if (f - 360.0 < 0.0 )
		f -= 360.0;
		return f;
	}

	double altitude(const std::vector<char>& buf, int& index) const
	{
		assert( index + getTypeSize(tlpDT_altitude) <= (int)buf.size() );
		char d[8]={0};

		for( int i = index; i < index + getTypeSize(tlpDT_altitude) ; ++i)
		memcpy(d+i-index, &buf[i], 1);
		return (double)((*(int*)d) - 1000.0);
	}

	void longitude(std::vector<char>& buf, double lon = 0.0 )
	{
		int iLon;
		if( lon == 0.0 )
		{
		lon = m_GPS.Lon();
		}

		if (lon < 0.0)
		lon += 360.0;

		iLon = (long)(lon * 46603.375);
		h_setData( buf, tlpDT_longitude, iLon);
	}

	void altitude( std::vector<char>& buf, double alt = 0.0 )
	{
		int iAlt;
		if( alt == 0.0 )
		{
		alt = m_GPS.H_ortho();
		}

		iAlt = (long)( alt + 1000.0 );
		h_setData( buf, tlpDT_altitude, iAlt);
	}

	void setString(std::vector<char>& buf, const ats::String& str)
	{
		buf.push_back(str.size());
		for( int i = 0; i < (int)str.size(); ++i)
		buf.push_back( str[i]);
	}

	ats::String getString(std::vector<char>& buf, int& index)
	{
		assert( index + 1 <= (int)buf.size() );
		int size = buf[index];
		assert( index + size + 1 <= (int)buf.size() );
		char d[size];
		d[size]='\0';
		for( int i = index+1; i < index + size + 1; ++i)
		memcpy(d+(i-index-1), &buf[i], 1);

		index += 1 + size;
		return ats::String(d);
	}

	void dumpBuffer( const std::vector<char>& buffer )
	{
		std::ostringstream outstr;
		int size = buffer.size();
		outstr << "MSG[" << size << "]:";

		for(int i = 0; i < size; ++i)
		{
		outstr << std::hex <<	std::setfill('0') << std::setw(2) << std::uppercase << (buffer[i] & 0xff) << ',';
		}

		fprintf(stderr, "%s", outstr.str().c_str()); 
	}

	protected:	
	template<typename T>
		void h_setData(std::vector<char>& buf, TLPDataType type, T data )
		{
		char tBuf[8];
		memcpy(tBuf, &data, sizeof(T));

		for( int i = 0; i < getTypeSize(type) ; ++i)
			buf.push_back( tBuf[i]);
		}
//---------------------------------------------------------------------------------------------------------
	template<typename T>	T h_getData( const std::vector<char>& buf, TLPDataType type, int &index ) const
	{
		assert( index + getTypeSize(type) <= (int)buf.size() ); 
		
		char data[8]={0};

		for( int i = index; i < index + getTypeSize(type) ; ++i)
		{
			memcpy(data+i-index, &buf[i], 1);
		}

		return (T)(*(T*)data);
	}

//---------------------------------------------------------------------------------------------------------
	void setFloat(std::vector<char>& buf, float data)
	{
		union{
		float a;
		unsigned char bytes[4];
		}u_f;

		u_f.a = data;

		for( int i = 0; i < getTypeSize(tlpDT_float) ; ++i)
		buf.push_back( u_f.bytes[i]);
	}

		//------------------------------------------------------------------------------------------
		//  Decode value and return as a string.
		std::string Decode(const std::vector<char>& buf, TLPDataType type, int& index ) const
		{
			double dval = 0.0;
			long lval = 0;
			std::string strVal;
			
			switch( type )
			{
				case tlpDT_tenthsInt:
				case tlpDT_tenthsUInt:
				case tlpDT_tenthsShort:
				case tlpDT_tenthsUShort:
					lval = h_getData<int>(buf, type, index);
					dval = double(lval) / 10.0;
					strVal = str ( format("%.1f") % dval);
					break;
				case tlpDT_hundredthsInt:
				case tlpDT_hundredthsUInt:
				case tlpDT_hundredthsShort:
				case tlpDT_hundredthsUShort:
					lval = h_getData<int>(buf, type, index);
					dval = double(lval) / 100.0;
					strVal =  str( format("%.2f") % dval);
					break;
				case tlpDT_latitude:
					strVal =  str( format("%.9f") % latitude(buf, index));
					break;
				case tlpDT_longitude:
					strVal =  str( format("%.9f") % longitude(buf, index));
					break;
				case tlpDT_altitude:
					strVal =  str( format("%.1f") % altitude(buf, index));
					break;
				case tlpDT_rssi:
					strVal =  str( format("%d") % (h_getData<int>(buf, type, index)*(-1)));
					break;
				case tlpDT_char:
					strVal =  str( format("'%c'") % (h_getData<char>(buf, type, index)));
					break;
				case tlpDT_byte:
				case tlpDT_short:
				case tlpDT_int:
				case tlpDT_long:
				case tlpDT_map:
				case tlpDT_map2:
				case tlpDT_count:
						strVal =  str( format("%d") % (h_getData<long>(buf, type, index)));
						break;
				case tlpDT_unsignedByte:
				case tlpDT_unsignedShort:
				case tlpDT_unsignedInt:
				case tlpDT_unsignedLong:
				case tlpDT_dateTime:
						strVal =  str( format("%d") % (h_getData<unsigned long>(buf, type, index)));
						break;
				case tlpDT_float:
						strVal =  str( format("%.6f") % (h_getData<float>(buf, type, index)));
						break;
				default:
					strVal = "0";
					break;
			}
			
			return strVal;
		}
	private:
	NMEA_Client m_GPS;
};

#endif
