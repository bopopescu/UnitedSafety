#pragma once
#include <string.h>
#include <ats-common.h>

//----------------------------------------------------------------------------
//  LensMAC.h
// 
// defines LensMAC - the eight byte lens mac address that uses the lower
//                   three bytes for most identification.
//  The class can work without the 8 byte definition - relying only on the
//  3 byte sub-string to match.
//
//	Equivalence is based only on the 3 byte portion
class LensMAC
{
#pragma pack(1)
	unsigned char m_Full[8];
	unsigned char m_MAC[3];
#pragma pack()	
public:
	LensMAC(){m_MAC[0] = m_MAC[1] = m_MAC[2] = 0x00;};
	virtual ~LensMAC(){};

	void MAC(unsigned char *buf)
	{
		for (short i = 0; i < 3; i++)
			m_MAC[i] = buf[i];
	}
	void MAC8(unsigned char *buf)
	{
		for (short i = 0; i < 8; i++)
			m_Full[i] = buf[i];
			
		memcpy(m_MAC, &m_Full[5], 3);
	}

	void SetMACHex(std::string hex) // convert a 6 digit hex string to a short MAC
	{
		char buf[8];
		buf[0] = '0';
		buf[1] = 'x';
		buf[4] = '\0';
		
		for (short i = 0; i < 3; i++)
		{
			buf[2] = hex[i * 2];
			buf[3] = hex[i * 2 + 1];
			m_MAC[i] = (char)std::strtoul(buf,NULL, 16);
		}
	}
	
	bool operator== (const LensMAC &rhs)
	{
		return (rhs.m_MAC[0] == m_MAC[0] && rhs.m_MAC[1] == m_MAC[1] && rhs.m_MAC[2] == m_MAC[2] );
	}
	
	bool operator== (const std::string &rhs)
	{
		return (rhs[0] == m_MAC[0] && rhs[1] == m_MAC[1] && rhs[2] == m_MAC[2] );
	}

	std::string toHex()
	{
		std::string mac;
		ats_sprintf(&mac, "%02x%02x%02x", m_MAC[0], m_MAC[1], m_MAC[2]);
		return mac;
	}
	std::string toHex8()
	{
		std::string mac;
		ats_sprintf(&mac, "%02x%02x%02x%02x%02x%02x%02x%02x", m_MAC[0], m_MAC[1], m_MAC[2], m_MAC[3], m_MAC[4], m_MAC[5], m_MAC[6], m_MAC[7]);
		return mac;
	}
	char * GetMac(char *buf)
	{
		memcpy(buf, m_MAC, 3);
		return buf;
	}

	void Dump()
	{
		printf("MAC: %s\n", toHex().c_str());
		printf("MAC8: %s\n", toHex8().c_str());
	}
};


