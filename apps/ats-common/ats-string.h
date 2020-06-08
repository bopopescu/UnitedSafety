#pragma once

#include <vector>

#include "ats-common.h"

namespace ats
{

typedef std::vector<String> StringList;

class StringListMap : public std::map <const String, StringList>
{
public:
	typedef std::pair <const String, StringList> StrListPair;

	// Description: Returns the StringList with key "p_key". If StringList "p_key" does
	//	not exist, then it will be created and returned (use "has_key" to test
	//	for the presence of a StringList without creating one).
	StringList& get(const String& p_key);

	// Description: Returns true if "p_key" exists in the mapping, and false is
	//	returned otherwise.
	bool has_key(const String &p_key) const;

	// Description: Removes key "p_key" from the map.
	void unset(const String &p_key);
};

void split(std::vector<ats::String>& p_list, const ats::String& p_src, const ats::String& p_char, size_t p_max=0);

String tolower(const String& p_s);

String toupper(const String& p_s);

String to_line_printable(const String& p_s);

String urlencode(const ats::String& p_url);

// Description: Finds the very next token from string "p_src" at index "p_index". Tokens
//	are delimited by any character appearing in "p_delim".
//
//	p_index must be set to zero on the first call to this function.
//
// Return: True is returned if a token was found, and false otherwise. "p_token" is undefined when false is returned.
bool next_token(ats::String& p_token, const ats::String& p_src, size_t& p_index, const ats::String& p_delim);

class PrefixContext
{
public:
	virtual void append(const char* p_str, size_t p_len) const = 0;
};

class PrefixContext_String : public PrefixContext
{
public:
	PrefixContext_String(ats::String& p_s) : m_s(p_s)
	{
	}

	virtual void append(const char* p_str, size_t p_len) const;
	ats::String& m_s;
};

class PrefixContext_FILE : public PrefixContext
{
public:
	PrefixContext_FILE(FILE* p_f) : m_f(p_f)
	{
	}

	virtual void append(const char* p_str, size_t p_len) const;
	FILE* m_f;
};

void prefix_lines(const ats::PrefixContext& p_des, const ats::String& p_s, const ats::String& p_tab);

}
