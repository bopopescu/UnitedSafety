#pragma once
#include <iostream>
#include <set>
#include <string>
#include <boost/algorithm/string.hpp>
#include "boost/format.hpp"
#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/exceptions.hpp>
#include <boost/foreach.hpp>
using namespace std;
using boost::format;
using boost::io::group;
using boost::property_tree::ptree;

#include "tlp.h"
#include "TLPEvent.h"


/*---------------------------------------------------------------------------------------------------------------------
	TLPFile - Read TruLink Protocol files, Encode and Decode byte vectors based on the TLP XML
	
		Read(short id) - Load the XML file TLP_xx.xml where xx is the id passed in.
		SetParentDir(string parentDir) - sets the parent directory where TLP XML files are found
		Encode(string, vector <char> output) - take the input string in the form "a=x b=y..." and encode the buffer
		Encode(stringmap, vector <char> output) - take the input stringmap and encode the buffer
		Decode(vector <char> input, string) - take the input buffer and decode a string in the form "a=x b=y..."
		Decode(vector <char> input, stringmap) - take the input buffer and decode the data into a stringmap

	Dave Huff - June 2015 - original code.
---------------------------------------------------------------------------------------------------------------------*/


class TLPFile
{
private:
	static std::string m_ParentDir;  // the directory that contains the XML files.
	std::string m_FileName;   // the name of the file that is loaded.
	TLPEvent m_Event;
	short m_ID;  // the id of the event  (used to see if an event is already loaded)
	
public:
	TLPFile();  // construct - don't read a file.
	TLPFile(short id);  // construct and read in the TLP file for id
	
	static void SetParentDir(std::string parentDir);
	
	int Read(short id); //Load the XML file TLP_xx.xml where xx is the id passed in.
	int Encode(std::string strData, vector <char> &output);          // take the input string in the form "a=x b=y..." and encode the buffer
	int Encode(ats::StringMap &smData, vector <char> &output);        // take the input stringmap and encode the buffer
	int Decode(vector <char> inputData, std::string output);  // take the input buffer and decode a string in the form "a=x b=y..."
	int Decode(vector <char> inputData, ats::StringMap &smOutput);    // take the input buffer and decode the data into a stringmap

	void ReadMaps(ptree &pt);
	void ReadClasses(ptree &pt);
	
	void dump() const ; // output the contents to cerr
	
	bool operator==(const short id) const;
};

