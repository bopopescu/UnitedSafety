/*---------------------------------------------------------------------------------------------------------------------
	TLPClass - contains a TLP Class that merges a series of event elements into a single entry element

	Dave Huff - June 2015 - original code.
---------------------------------------------------------------------------------------------------------------------*/
#include "TLPClass.h"
#include "TLPEvent.h"

TLPEvent * TLPClass::m_Parent = NULL;

//-----------------------------------------------------------------------
// Encode the data from the input buffer.
//  The input map should have keys that match the element names.  The 
//  values will be encoded based on the element type.  Elements after a
//  'count' will be expected to have 'name'1, 'name'2... up to the count
//  value.
//
//  Returns: int - number of characters in the output buffer after encoding.
int TLPClass::Encode
(
	ats::StringMap &smData,  // key-value pairs of elements
	vector <char> &output,
	std::string strPrefix,
	int index
)
{
	std::string strDefault = "0";
	std::string value;
	TLP tlp;
	int ret;
	// for each element
	for (std::vector<TLPElement>::iterator it = m_Elements.begin(); it != m_Elements.end(); ++it)
	{
		if ((ret = EncodeElement(smData, output, it, strPrefix, 0)) < 0)
			return ret;
	}
	return output.size();
}

//-----------------------------------------------------------------------
int TLPClass::EncodeElement
(
	ats::StringMap &smData,  // key-value pairs of elements
	vector <char> &output,
	std::vector<TLPElement>::iterator & it,
	std::string strPrefix,
	int index // 0 - regular element, > 0 this is decoding an element in a count
)
{
	std::string strName;
	std::string strDefault = "0";
	
	if (strPrefix.length())
		strName = str( format("%s.%s") %  strPrefix % it->Name().c_str());
	else
		strName = str( format("%s") % it->Name().c_str());
		
	if (index > 0)
		strName = str( format("%s%d") % strName.c_str() % index);

	boost::replace_all(strName, "\"", "");		

	if (it->Type() == tlpDT_class)
	{
		std::vector<TLPClass>::iterator theClass;
		theClass = find (m_Parent->Classes().begin(), m_Parent->Classes().end(), it->TypeName());

		if (theClass == m_Parent->Classes().end())
			return -1;  // specified Class not found

		theClass->Encode(smData, output, strName, index);
	}
	else if (it->Type() == tlpDT_count)
	{
		//  find the value
		std::string value = smData.get(strName, strDefault);
				
		//  convert the string to binary and add it to the vector
		it->Encode(value, output);
		// read the next element type
			
		if (++it == m_Elements.end())
			return -1;  // indicates that the class ends on a 'count' - which is not allowed.

		int ret; 
			
		for (int i = 1; i <= atoi(value.c_str()); i++)
		{
			if ((ret = EncodeElement(smData, output, it, it->Name(), i)) < 0)
				return ret;
		}
	}
	else
	{
		//  find the value
		std::string value = smData.get(strName, strDefault);
				
		//  convert the string to binary and add it to the vector
		it->Encode(value, output);
	}
	
	return 0;  // specified Class not found
}

//-----------------------------------------------------------------------
	// Decode the data from the input buffer.
	//  Note that index is moved to the next point in the inputData vector
	//  to be read after this data is decoded.
int TLPClass::Decode
(
	vector <char> inputData,
	ats::StringMap & output,
	int &index,
	std::string strPrefix,
	int iCount // 0 - regular element, > 0 this is decoding an element in a count
)
{
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
int TLPClass::DecodeElement
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
		theClass = find (m_Parent->Classes().begin(), m_Parent->Classes().end(), it->TypeName());

		if (theClass == m_Parent->Classes().end())
			return -1;  // specified Class not found

//		theClass->Decode(vInput, index, output, strName, iCount);
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

	
