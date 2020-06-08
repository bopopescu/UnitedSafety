#pragma once

#include "ats-common.h"

namespace LMDIRECT
{

class MobileIDType
{
public:
	typedef enum
	{
		OFF,
		ESN,
		IMEI,
		EID,
		IMSI,
		GSM,
		GPRS,
		USER_DEFINED,
		PHONE_NUMBER,
		LMU_IP
	}
	MOBILE_ID_TYPE;

	MobileIDType()
	{
		m_id = OFF;
	}

	int enum_to_id(MOBILE_ID_TYPE p_id) const
	{
		switch(p_id)
		{
		case ESN: return 1;

		case IMEI:
		case EID: return 2;

		case IMSI:
		case GSM:
		case GPRS: return 3;

		case USER_DEFINED: return 4;
		case PHONE_NUMBER: return 5;
		case LMU_IP: return 6;

		default: return 0;
		}
	}

	void set(MOBILE_ID_TYPE p_id) { m_id = p_id;}

	MOBILE_ID_TYPE get() {return m_id;}

	MOBILE_ID_TYPE get() const {return m_id;}

	operator unsigned char() const
	{
		return m_id;
	}

	// Description: Returns the mobile ID data in Generic Message Structure (GMS) format.
	ats::String get_gms_format() const
	{
		ats::String s;
		s.append(1, enum_to_id(get()));
		return s;
	}

private:
	MOBILE_ID_TYPE m_id;
};

}
