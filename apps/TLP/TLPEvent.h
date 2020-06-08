#pragma once
#include <string>
#include <set>
#include <iostream>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/foreach.hpp>

using namespace std;
using boost::format;
using boost::io::group;
#include <algorithm>    // std::find
#include "TLPMap.h"
#include "TLPClass.h"
#include "TLPElement.h"

/*---------------------------------------------------------------------------------------------------------------------
	TLPEvent - contains an event as defined in a TLP_xx.XML file.
	
	Dave Huff - June 2015 - original code.
---------------------------------------------------------------------------------------------------------------------*/
class TLPEvent
{
private:
  std::string m_Name;
  std::set <std::string> m_DestApp;
  std::vector <TLPElement> m_Elements;
  std::vector <TLPMap> m_Maps;
  std::vector <TLPClass> m_Classes;
  int m_ID; // the ID of the event - shows up as TLP_ID.xml

public:
	TLPEvent();
	void Name(std::string name);
	std::string Name(){return m_Name;};

	void ID(const int id){m_ID = id;};
	int ID() const {return m_ID;};
	

	size_t AddDestApp(std::string appName){m_DestApp.insert(appName); return m_DestApp.size();}  // returns the number of DestApps
	size_t AddDestApps(std::string appNames){boost::split(m_DestApp, appNames, boost::is_any_of(", ")); return m_DestApp.size();}  // returns the number of DestApps
	std::set <std::string> DestApp(){return m_DestApp;}  //allows for iteration over the set.

	std::vector <TLPElement> Elements(){return m_Elements;} // allow for iteration over the vector.
	size_t AddElement(const TLPElement &element);

	std::vector <TLPMap> Maps() const{return m_Maps;} // allow for iteration over the vector.
	size_t AddMap(const TLPMap &aMap);

	std::vector <TLPClass> Classes() const{return m_Classes;} // allow for iteration over the vector.
	size_t AddClass(const TLPClass &aClass);
	
	int Encode(ats::StringMap &smData, vector <char> &output);
	int EncodeElement(ats::StringMap &smData,	vector <char> &output, std::vector<TLPElement>::iterator & it,std::string strPrefix, int index);
	int Decode(vector <char> inputData, ats::StringMap & output);
	int DecodeElement(ats::StringMap &smOutput, vector <char> &vInput, int & index, std::vector<TLPElement>::iterator & it, std::string strPrefix, int iCount);

	void dump() const ;
};

