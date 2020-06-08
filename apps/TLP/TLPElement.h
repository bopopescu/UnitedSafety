#pragma once
#include "tlp.h"
/*---------------------------------------------------------------------------------------------------------------------
	TLPElement - pairs a TLP type with its name.
		A name of 'undefined' means that the element has not been set and should not be used. 
		
		Examples:
		<byte>element1</byte>     byte is the type, element1 is the name
		<map name="Days">Day</map>   map is the Type, Day is the Name, Days is the TypeName
		
---------------------------------------------------------------------------------------------------------------------*/
class TLPElement
{
private:
  TLPDataType m_Type;
  std::string m_Name;
	std::string m_TypeName;  // the name of the map or the name of the class

 public:
 	TLPElement(): m_Type(tlpDT_unsignedByte), m_Name("undefined") {}
 	
 	void Set(const std::string type, const std::string name)
 	{
 		if (name.length())
 			m_Name = name;
 		else
 			m_Name ="undefined";
 			
 		m_Type = TLP::m_DataTypes[type];
		m_TypeName.empty();
 	}
 	void Set(const std::string type, const std::string name, const std::string typeName)
 	{
		Set(type, name);
		
 		if (typeName.length())
 			m_TypeName = typeName;
 		else
 			m_TypeName.empty();
 	}
 	
 	TLPDataType Type() const {return m_Type;}
 	const std::string Name() const {return m_Name;}
 	const std::string TypeName() const {return m_TypeName;}
	
	std::string Decode(vector <char> inputData, int &index)
	{
		TLP tlp;
		return tlp.getData(inputData, Type(), index);
	}
	
	// Encode string based data to on output vector of chars
	bool Encode(std::string value, vector <char> &output)
	{
		TLP tlp;
		return tlp.setData(output, Type(), value);
	}

	void dump() const
	{
		if (m_TypeName.length() > 0)
			cerr << "    Element: " << m_Name << "  Type: " << m_Type << "  Name: " << m_TypeName << std::endl;
		else
			cerr << "    Element: " << m_Name << "  Type: " << m_Type << std::endl;
	}
};
