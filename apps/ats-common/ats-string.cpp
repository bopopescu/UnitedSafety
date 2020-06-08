#include <algorithm>

#include <stdio.h>
#include <string.h>

#include "ats-string.h"

void ats::split(std::vector<ats::String>& p_list, const ats::String& p_src, const ats::String& p_char, size_t p_max)
{
	p_list.clear();

	ats::String s;

	for(size_t i = 0; i < p_src.size(); ++i)
	{
		const char c = p_src[i];

		ats::String::const_iterator j;

		for(j = p_char.begin(); j != p_char.end(); ++j)
		{

			if((*j) == c)
			{
				p_list.push_back(s);

				if(p_max && (p_list.size() == p_max))
				{

					if(i < (p_src.size() - 1))
					{
						p_list.push_back(p_src.substr(i + 1));
					}

					return;
				}

				s.clear();
				break;
			}

		}

		if(p_char.end() == j)
		{
			s += c;
		}

	}

	p_list.push_back(s);

}

bool ats::chop(const String& p_match, String& p_str)
{

	if(0 != p_str.find(p_match))
	{
		return false;
	}

	p_str = p_str.substr(p_match.size());
	return true;
}

ats::String ats::tolower(const ats::String& p_s)
{
	String s(p_s);
	std::transform(s.begin(), s.end(), s.begin(), ::tolower);
	return s;
}

ats::String ats::toupper(const ats::String& p_s)
{
	String s(p_s);
	std::transform(s.begin(), s.end(), s.begin(), ::toupper);
	return s;
}

ats::String ats::to_line_printable(const ats::String& p_s)
{
	String s;

	ats::String::const_iterator i = p_s.begin();

	while(i != p_s.end())
	{
		const char c = *i;
		++i;

		if(c >= ' ')
		{

			if(c == '\\')
			{
				s += "\\\\";
			}
			else
			{
				s += c;
			}

		}
		else
		{

			if('\t' == c)
			{
				s += c;
			}
			else
			{
				char hex[16];
				snprintf(hex, sizeof(hex) - 1, "\\x%02X", c & 0xff);
				hex[sizeof(hex) - 1] = '\0';
				s += hex;
			}

		}

	}

	return s;
}

bool ats::next_token(ats::String& p_token, const ats::String& p_src, size_t& p_index, const ats::String& p_delim)
{

	if(p_index >= p_src.size())
	{
		return false;
	}

	const size_t start_index = p_index;
	const char* s = p_src.c_str();

	for(;p_index < p_src.size();)
	{
		const char c = s[p_index++];

		for(size_t i = 0; i < p_delim.size(); ++i)
		{

			if(p_delim[i] == c)
			{
				p_token = p_src.substr(start_index, (p_index - 1) - start_index);
				return true;
			}

		}

	}

	p_token = p_src.substr(start_index);
	return true;
}

ats::StringList& ats::StringListMap::get(const String& p_key)
{
	iterator i = find(p_key);

	if(end() == i)
	{
		i = (insert(StrListPair(p_key, StringList()))).first;
	}

	return i->second;
}

bool ats::StringListMap::has_key(const String& p_key) const
{
	return (find(p_key) != end());
}

void ats::StringListMap::unset(const String& p_key)
{
	iterator i = find(p_key);

	if(end() != i)
	{
		erase(i);
	}

}

void ats::prefix_lines(const ats::PrefixContext& p_des, const ats::String& p_s, const ats::String& p_tab)
{
	const char* s = p_s.c_str();

	for(;;)
	{
		const char* p = strchr(s, '\n');

		if(p)
		{
			p_des.append(s, (p + 1) - s);
			s = p + 1;

			if(*s)
			{
				p_des.append(p_tab.c_str(), p_tab.size());
			}

		}
		else
		{
			p_des.append(s, (p_s.length() + p_s.c_str()) - s);
			break;
		}

	}

}

void ats::PrefixContext_String::append(const char* p_str, size_t p_len) const
{
	m_s.append(p_str, p_len);
}

void ats::PrefixContext_FILE::append(const char* p_str, size_t p_len) const
{
	fwrite(p_str, 1, p_len, m_f);
}

ats::String ats::urlencode(const ats::String& p_url)
{
	ats::String s;
	ats::String::const_iterator i = p_url.begin();

	while(i != p_url.end())
	{
		const char c = *(i++);

		if(
			((c >= '0') && (c <= '9')) ||
			((c >= 'A') && (c <= 'Z')) ||
			((c >= 'a') && (c <= 'z')) ||
			('!' == c) ||
			('(' == c) ||
			(')' == c) ||
			('-' == c) ||
			('_' == c) ||
			('.' == c)
		)
		{
			s += c;
		}
		else
		{
			char buf[8];
			snprintf(buf, sizeof(buf) - 1, "%%%02X", ((unsigned int)c) & 0xff);
			buf[sizeof(buf) - 1] = '\0';
			s += buf;
		}

	}

	return s;
}
