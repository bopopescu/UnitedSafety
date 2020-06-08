#include "TLPEvent.h"


/*---------------------------------------------------------------------------------------------------------------------
	TLPEvent - contains an event as defined in a TLP_xx.XML file.
	
	Dave Huff - June 2015 - original code.
---------------------------------------------------------------------------------------------------------------------*/
TLPEvent::TLPEvent() : m_Name("undefined") 
{
	TLPClass::m_Parent = this;
};


void TLPEvent::Name(std::string name)
{
	if (name.length())
		m_Name = name;
	else
		m_Name = "undefined";
}
	
size_t TLPEvent::AddElement(const TLPElement &element)
{
	m_Elements.push_back(element);
	return m_Elements.size();
}
	
size_t TLPEvent::AddMap(const TLPMap &aMap)
{
	m_Maps.push_back(aMap);
	return m_Maps.size();
}

size_t TLPEvent::AddClass(const TLPClass &aClass)
{
	m_Classes.push_back(aClass);
	return m_Classes.size();
}

//-----------------------------------------------------------------------
// Encode the data from the input buffer.
//  The input map should have keys that match the element names.  The 
//  values will be encoded based on the element type.  Elements after a
//  'count' will be expected to have 'name'1, 'name'2... up to the count
//  value.
//
//  Returns: int - number of characters in the output buffer after encoding.
int TLPEvent::Encode
(
	ats::StringMap &smData,  // key-value pairs of elements
	vector <char> &output
)
{
	output.empty();
	// insert the ID and the sequence number (seq num will be updated later)
	char tBuf[8];
	memcpy(tBuf, &m_ID, sizeof(short));

	for( int i = 0; i < 2 ; ++i)
		output.push_back( tBuf[i]);

	output.push_back(0); // sequence number place holder
	output.push_back(0); // sequence number place holder

	std::string strDefault = "0";
	std::string value;
	TLP tlp;
	int ret;
	// for each element
	for (std::vector<TLPElement>::iterator it = m_Elements.begin(); it != m_Elements.end(); ++it)
	{
		if ((ret = EncodeElement(smData, output, it, "", 0)) < 0)
			return ret;
	}
	return output.size();
}


//-----------------------------------------------------------------------
int TLPEvent::EncodeElement
(
	ats::StringMap &smData,  // key-value pairs of elements
	vector <char> &output,
	std::vector<TLPElement>::iterator & it,
	std::string strPrefix,
	int index // 0 - regular element, > 0 this is decoding an element in a count
)
{
	std::string strName;
	
	if (strPrefix.length())
			strName = str( format("%s.%s") % strPrefix % it->Name().c_str());
	else
		strName = str( format("%s") % it->Name().c_str());

	if (index > 0)
		strName = str( format("%s%d") % strName % index );

	boost::replace_all(strName, "\"", "");		
	
	if (it->Type() == tlpDT_class)
	{
		std::vector<TLPClass>::iterator theClass;
		theClass = find (m_Classes.begin(), m_Classes.end(), it->TypeName());
		
		if (theClass == m_Classes.end())
			return -1;  // specified Class not found
			
		theClass->Encode(smData, output, strName, index);
	}
	else if (it->Type() == tlpDT_count)
	{
		//  find the value
		std::string value = smData.get(strName, "0");  // defaults to 0
			
		//  convert the string to binary and add it to the vector
		it->Encode(value, output);
		// read the next element type
		
		if (++it == m_Elements.end())
			return -2;  // indicates that the class ends on a 'count' - which is not allowed.
		
		for (int i = 1; i <= atoi(value.c_str()); i++) // loop through 'count' elements
		{
			int ret; 
			if ((ret = EncodeElement(smData, output, it, "", i)) < 0)
				return ret;
		}
	}
	else
	{
		//  find the value
		std::string value = smData.get(strName, "0");  // defaults to 0
				
		//  convert the string to binary and add it to the vector
		it->Encode(value, output);
	}
	return 0;
}


//-----------------------------------------------------------------------
	// Decode the data from the input buffer.
	//  Note that index is moved to the next point in the inputData vector
	//  to be read after this data is decoded.
int TLPEvent::Decode(vector <char> inputData, ats::StringMap & output)
{
	output.empty();
	int index = 0;
	int ret;
	TLP tlp;
	// for each element
	for (std::vector<TLPElement>::iterator it = m_Elements.begin(); it != m_Elements.end(); ++it)
	{
		if ((ret = DecodeElement(output, inputData, index, it, std::string(""), 0)) < 0)
			return ret;
	}
	return output.size();
}
//-----------------------------------------------------------------------
int TLPEvent::DecodeElement
(
	ats::StringMap &smOutput,  // key-value pairs of elements
	vector <char> &vInput,
	int & index, // may be changed by class decoding or 'count' decoding
	std::vector<TLPElement>::iterator & it,
	std::string strPrefix,
	int iCount // 0 - regular element, > 0 this is decoding an element in a count
)
{
	std::string strName;
	std::string strData;
	TLP tlp;

	if (strPrefix.length())
		strName = str( format("%s.%s") % strPrefix % it->Name().c_str());
	else
		strName = str( format("%s") % it->Name().c_str());

	if (iCount > 0)
		strName = str( format("%s%d") % strName % iCount );

	boost::replace_all(strName, "\"", "");		

	if (it->Type() == tlpDT_class)
	{
		std::vector<TLPClass>::iterator theClass;
		theClass = find (m_Classes.begin(), m_Classes.end(), it->TypeName());

		if (theClass == m_Classes.end())
			return -1;  // specified Class not found

		theClass->Decode(vInput, smOutput, index, strName, iCount);
	}
	else if (it->Type() == tlpDT_count)
	{
		//  find the value
		strData = it->Decode(vInput, index);
		index += tlp.getTypeSize(it->Type());
		smOutput.set(strName, strData);
		// read the next element type

		if (++it == m_Elements.end())
			return -2;  // indicates that the class ends on a 'count' - which is not allowed.

		for (int i = 1; i <= atoi(strData.c_str()); i++) // loop through 'count' elements
		{
			int ret; 
			if ((ret = DecodeElement(smOutput, vInput, index, it, "", i)) < 0)
				return ret;
		}
	}
	else  // regular element
	{
		//  find the value
		//  convert the string to binary and add it to the vector
		strData = it->Decode(vInput, index);
		index += tlp.getTypeSize(it->Type());
		smOutput.set(strName, strData);
	}
	return 0;
}

	
void TLPEvent::dump() const 
{
	cerr << "Event Name: " << m_Name << std::endl;
		
  BOOST_FOREACH(const std::string& str, m_DestApp)
  	cerr << "  Dest App: " << str << std::endl;

	cerr << "  Elements:" << std::endl;
  BOOST_FOREACH(const TLPElement& element, m_Elements)
	 	element.dump();

	BOOST_FOREACH(const TLPMap &aMap,  m_Maps)
		aMap.dump();

	BOOST_FOREACH(const TLPClass &aClass,  m_Classes)
		aClass.dump();
}

