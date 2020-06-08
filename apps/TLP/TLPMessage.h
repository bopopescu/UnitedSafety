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
	TLPMessage - encode/decode a TLP Message - including the header and one or more messages.
	
	The message header contains the following:
		Type (1 byte) 0xFEFE (always for header)
		SequenceID (2 bytes) -  sequential from startup
		IMEI - (8 bytes BCD) unit IMEI - either the source IMEI if message is from the TruLink or 
																		 the dest IMEI if sending to the TruLink
	  nEvents (1 byte) - number of events in this message block - max of 50 events.
		
		
	
	Dave Huff - Aug 2015 - original code.
---------------------------------------------------------------------------------------------------------------------*/

#define TRULINK_HEADER_SIZE (13)

class TLPMessage
{
private:
	static const int m_MAX_EVENTS = 25;  // 
	static const int m_HEADER_SIZE = 13;
	std::string m_IMEI;   // the IMEI of the unit - 15 byte string (not BCD encoded yet)
	vector <unsigned char> m_Events;  // the vector of data containing the events.
	int  m_nEvents;  // the number of events.
	short m_seq;  // The message sequence number -  set externally
	
public:
	TLPMessage();
	
	bool AddEvent(std::vector <unsigned char> const event);  // add an encoded event to be sent
	bool AddEvent(std::string const event);  // add an encoded event to be sent
	std::vector <unsigned char> GetMessage();
	void IMEI(const std::string& imei) {this->m_IMEI = imei;}
	void Seq(short seq) {this->m_seq = seq;}	
};

