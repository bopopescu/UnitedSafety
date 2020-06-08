#pragma once
#include <string>
#include <map>
#include <iostream>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/foreach.hpp>

using namespace std;
using boost::format;
using boost::io::group;
/*---------------------------------------------------------------------------------------------------------------------
	TLPMap - contains a TLP Map that maps a unsigned char value to a string
	
	Notes:	The map entry is defined as <mapvalue id="1">string</mapvalue>.  The id is optional in which case the
					next value will be used.  Valid values are 0-255.  
					IDs must be sorted in the XML file!  You cannot do id 9, 10, 7.
					If an ID is assigned and the next entry is not the ID of the second entry will be 1 more than the
					assigned entry.
						<mapvalue >string0</mapvalue>               	<-- ID of this entry is 0
						<mapvalue id="9">string9</mapvalue>						<-- ID of this entry is 9
						<mapvalue>string10</mapvalue>               	<-- ID of this entry is 10
						<mapvalue id="2">string2</mapvalue>						<-- INVALID -- Ids cannot go backwards!!!
	
	Dave Huff - June 2015 - original code.
---------------------------------------------------------------------------------------------------------------------*/
class TLPMap
{
private:
	std::string m_Name;
	std::map <unsigned char, std::string> m_Entry;
	
	unsigned char m_MaxID;  // the next ID assigned will be this + 1
	
public:
	TLPMap () : m_MaxID(0) {};
	~TLPMap () {};
	
	void Name(std::string name){m_Name = name;};
	std::string Name() const {return m_Name;};
	
	int AddEntry(unsigned char id, std::string strValue)
	{
		if (int(id) < m_MaxID || m_MaxID == 254)  // invalid ID or we filled the list
			return -1;
			
		m_MaxID = int(id);
		m_Entry[m_MaxID] = strValue;
		m_MaxID++;
		return m_MaxID;
	}
	int AddEntry(std::string strValue)  // no ID - use the next available ID
	{
		if (m_MaxID == 254)  // invalid ID or we filled the list
			return -1;
		m_Entry[m_MaxID] = strValue;
		m_MaxID++;
		return m_MaxID;
	}
	
	std::string GetValue(unsigned char id)  
	{ 
		std::map <unsigned char, std::string>::iterator iter;
		iter = m_Entry.find(id);  // look for the existing ID
		if (iter != m_Entry.end())
			return iter->second; 
		else  // ID is undefined - return string will reflect that.
		{
			std::string strUndef = str( format("%s_%d_undefined") % m_Name % id);
			return strUndef;
		}
	};
	
	void dump() const 
	{
		cerr << "  Map Name: " << m_Name << endl;

		typedef std::pair <const unsigned char, std::string> mapPair;
		BOOST_FOREACH( mapPair entry, m_Entry)
		{
			cerr << "    " << int(entry.first) << ": " << entry.second << endl;
		}
	}
};
