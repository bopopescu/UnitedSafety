#pragma once
#include <vector>

#include "lmdirect/option.h"
#include "lmdirect/message.h"
#include "ats-common.h"

// Description: Implements the LMDirect Basic Message Structure (also known as
//	Generic Message Structure).
class LMDirect
{
public:
	LMDirect();

	virtual~ LMDirect();

	// Description: Returns LMDirect data in Generic Message Structure (GMS) format.
	ats::String get_gms_format() const;

	LMDIRECT::Option m_option;
	LMDIRECT::Message m_message;
};
