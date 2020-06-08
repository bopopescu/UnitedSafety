#include "TLPMessage.h"
#include "BCD.h"

/*---------------------------------------------------------------------------------------------------------------------
	TLPMessage - encode/decode a TLP Message - including the header and one or more messages.
	
	Basic Usage:  Instantiate object of the TLPMessage
								SetIMEI - set the IMEI of the message
								AddEvent
								[AddEvent] - repeat for up to 25 events.
								GetMessage - this will increment the sequence number for the next message
	
	The message header contains the following:
		Type (1 byte) 0xFEFE (always for header)
		SequenceID (2 bytes) -  sequential from startup
		IMEI - (8 bytes BCD) unit IMEI - either the source IMEI if message is from the TruLink or 
																		 the dest IMEI if sending to the TruLink
	  nEvents (1 byte) - number of events in this message block - max of 50 events.
		
		
	
	Dave Huff - Aug 2015 - original code.
---------------------------------------------------------------------------------------------------------------------*/



//---------------------------------------------------------------------------------------------------------------------
TLPMessage::TLPMessage()
{
	m_nEvents = 0;
	m_Events.empty();
	m_Events.resize(m_HEADER_SIZE, 0);
	m_IMEI.empty();
	m_seq  = 0;
}
	
//---------------------------------------------------------------------------------------------------------------------
// AddEvent - add the encoded event in vector to the end of the message, increments the number of events
// Return:  true if event added
//          false if event limit of 25 is reached.
//
bool TLPMessage::AddEvent(std::vector <unsigned char> const event)  // add an encoded event to be sent
{
	if (m_nEvents >= 25)
		return false;
		
	m_Events.insert(m_Events.end(), event.begin(), event.end());
	m_nEvents++;
	return true;
}

bool TLPMessage::AddEvent(std::string const event)  // add an encoded event to be sent
{
	if (m_nEvents >= 25)
		return false;
		
	m_Events.insert(m_Events.end(), event.begin(), event.end());
	m_nEvents++;
	return true;
}

//---------------------------------------------------------------------------------------------------------------------
// GetMessage - return the vector of all the messages combined with a header and checksum added.
//   - at this point the m_Events vector has a dummy header, all the events and no checksum.
//
std::vector <unsigned char> TLPMessage::GetMessage()
{
	m_Events[0] = 0xFE;
	m_Events[1] = 0xFE;
	m_Events[2] = m_seq & 0xff;
	m_Events[3] = (m_seq >> 8) & 0xff;
	
	std::string strBCD_IMEI;
	strBCD_IMEI = ToBCD(m_IMEI);
	
  for (short i = 0; i < 8; i++)
		m_Events[4 + i] = strBCD_IMEI[i];

	m_Events[12] = (unsigned char)m_nEvents;

	unsigned char cs = 0;

	for (std::vector<unsigned char>::iterator it = m_Events.begin() ; it != m_Events.end(); ++it)
		cs += *it;
		
	m_Events.push_back(cs);
	
  return m_Events;
}

