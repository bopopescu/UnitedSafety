#include "lmdirect.h"

LMDirect::LMDirect()
{
}

LMDirect::~LMDirect()
{
}

ats::String LMDirect::get_gms_format() const
{
	ats::String s;
	s.append(m_option.get_gms_format());
	s.append(m_message.get_gms_format());
	return s;
}
