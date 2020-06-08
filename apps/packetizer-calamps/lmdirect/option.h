#pragma once

#include <vector>

#include "lmdirect/mobileidtype.h"
#include "ats-common.h"

namespace LMDIRECT
{

// Description: Implements the "Options" header of an LMDirect packet.
class Option
{
public:
	Option();

	typedef enum
	{
		MOBILE_ID,
		MOBILE_ID_TYPE,
		AUTHENTICATION_WORD,
		ROUTING,
		FORWARDING,
		RESPONSE,
		//
		MAX_OPTION
	}
	OPTION;

	void set_option(OPTION, bool p_enabled);

	bool get_option(OPTION);

	bool get_option(OPTION) const;

	const ats::String& get_mobile_id() { return m_mid;}
	const ats::String& get_mobile_id() const { return m_mid;}

	const MobileIDType& get_mobile_id_type() { return m_mobile_id_type;}
	const MobileIDType& get_mobile_id_type() const { return m_mobile_id_type;}

	const ats::String& get_authentication_word() {return m_auth;}
	const ats::String& get_authentication_word() const {return m_auth;}

	void set_mobile_id(const ats::String& p_mid);

	void set_mobile_id_type(MobileIDType::MOBILE_ID_TYPE p_type)
	{
		m_mobile_id_type.set(p_type);
	}

	void set_mobile_id_type(MobileIDType p_type)
	{
		m_mobile_id_type = p_type;
	}

	void set_authentication_word(const ats::String& p_word);

	void set_forwarding_address(const ats::String& p_addr);

	void set_forwarding_protocol(const ats::String& p_proto);

	void set_response_address(const ats::String& p_addr);

	void set_response_port(const ats::String& p_port);

	// Description: Returns the option data in Generic Message Structure (GMS) format.
	ats::String get_gms_format() const;

private:
	std::vector<bool> m_option;
	ats::String m_mid;
	ats::String m_auth;

	MobileIDType m_mobile_id_type;
};

}
