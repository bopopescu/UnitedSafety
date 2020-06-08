#include "lmdirect/option.h"

LMDIRECT::Option::Option()
{
	m_option.resize(MAX_OPTION, false);
}

void LMDIRECT::Option::set_option(OPTION p_o, bool p_enabled)
{
	m_option[p_o] = p_enabled;
}

bool LMDIRECT::Option::get_option(OPTION p_o)
{
	return m_option[p_o];
}

void LMDIRECT::Option::set_mobile_id(const ats::String& p_mid)
{
	m_mid = p_mid;
}

void LMDIRECT::Option::set_authentication_word(const ats::String& p_auth)
{
	m_auth = p_auth;
}

static ats::String toBCD(const ats::String& p_s)
{

	if(p_s.empty())
	{
		return p_s;
	}

	ats::String s;
	s.reserve((p_s.size() >> 1) + 1);
	const char* p = p_s.c_str();
	size_t remain = p_s.size();

	while(remain >= 2)
	{
		s.append(1, (unsigned char)(((p[0] - '0') << 4) | (p[1] - '0')));
		remain -= 2;
		p += 2;
	}

	if(remain)
	{
		s.append(1, (unsigned char)(((p[0] - '0') << 4) | 0xF));
	}

	return s;
}

ats::String LMDIRECT::Option::get_gms_format() const
{
	ats::String s;
	s.reserve(64);
	s += (unsigned char)
		((get_option(MOBILE_ID) ? 1 : 0)
		| (get_option(MOBILE_ID_TYPE) ? 0x2 : 0)
		| (get_option(AUTHENTICATION_WORD) ? 0x4 : 0)
		| (get_option(ROUTING) ? 0x8 : 0)
		| (get_option(FORWARDING) ? 0x10 : 0)
		| (get_option(RESPONSE) ? 0x20 : 0)
		| 0x80);

	if(get_option(MOBILE_ID))
	{
		const ats::String& bcd = toBCD(get_mobile_id());
		s.append(1, (unsigned char)(bcd.length()));
		s.append(bcd);
	}

	if(get_option(MOBILE_ID_TYPE))
	{
		s.append(1, 1);
		s.append(1, (get_mobile_id_type().get_gms_format())[0]);
	}

	if(get_option(AUTHENTICATION_WORD))
	{
		const ats::String& w = get_authentication_word();
		s.append(1, (unsigned char)(w.size()));

		if(!w.empty())
		{
			s.append(w.c_str(), w.size());
		}

	}


	return s;
}

bool LMDIRECT::Option::get_option(OPTION p_o) const
{
	return m_option[p_o];
}
