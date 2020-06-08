#pragma once
/*---------------------------------------------------------------------------------------------------------------------
	TLPClass - contains a TLP Class that merges a series of event elements into a single entry element

	Dave Huff - June 2015 - original code.
---------------------------------------------------------------------------------------------------------------------*/
#include <string>
#include <iostream>

#include <boost/algorithm/string.hpp>
#include "boost/format.hpp"
#include <boost/foreach.hpp>

#include "TLPElement.h"
#include "tlp.h"


using namespace std;
using boost::format;

class TLPEvent;

class TLPClass
{
private:
	std::string m_Name;
  std::vector <TLPElement> m_Elements;
	
public:
	static TLPEvent *m_Parent;  // declared public so it can be set by TLPEvent
	
public:
	TLPClass () {};
	~TLPClass () {};
	
	void Name(std::string name){m_Name = name;};
	std::string Name() const {return m_Name;};
	std::vector <TLPElement> Elements(){return m_Elements;} // allow for iteration over the vector.
	
	bool operator == (const TLPClass &rhs)
	{
		return rhs.m_Name == m_Name;
	}
	bool operator == (const std::string &rhs)
	{
		return (m_Name == rhs);
	}
	
	size_t AddElement(const TLPElement &element)
	{
		m_Elements.push_back(element);
		return m_Elements.size();
	}
	
	int Encode(ats::StringMap &smData, vector <char> &output, std::string strPrefix, int index);
	int EncodeElement(ats::StringMap &smData, vector <char> &output, std::vector<TLPElement>::iterator & it, std::string strPrefix, int index);
	int Decode(vector <char> inputData, ats::StringMap & output, int &index, std::string strPrefix, int iCount);
	int DecodeElement(ats::StringMap &smOutput, vector <char> &vInput, int & index, std::vector<TLPElement>::iterator & it, std::string strPrefix, int iCount);

	void dump() const 
	{
		cerr << "  Class Name: " << m_Name << endl;

	  BOOST_FOREACH(const TLPElement& element, m_Elements)
		 	element.dump();
	}
};
