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


#include "TLPFile.h"
#include "command_line_parser.h"

std::string TLPFile::m_ParentDir("/etc/redstone/TLP/");	// the directory that contains the XML files.

const ptree& empty_ptree()
{
	static ptree t;
	return t;
}
//---------------------------------------------------------------------------------------------------------------------
TLPFile::TLPFile()
{
	// if not a TruLink set the default directory for a windows system	
}
//---------------------------------------------------------------------------------------------------------------------
TLPFile::TLPFile(short id)
{
	// if not a TruLink set the default directory for a windows system
	Read(id);
	m_Event.ID(id);
}

/*---------------------------------------------------------------------------------------------------------------------
	SetParentDir: Sets the parent directory where TLP XML files are found
	
	Input: parentDir - the path to the parent directory
	
	Return: void
	
	Note:	The parent directory will be defaulted for TruLinks to /etc/redstone/TLP at construction
	
	Dave Huff - June 2015 - original code.
---------------------------------------------------------------------------------------------------------------------*/
void TLPFile::SetParentDir(std::string parentDir)
{
	if (parentDir.size() > 0)
		m_ParentDir = parentDir;
}

	
/*---------------------------------------------------------------------------------------------------------------------
	Read: Read a TLP XML file
	
	Input: id - the id of the file to be read.
	
	Return:	 0 if read successfully
					-1 if file not found
					-2 if file not read (format error, xml error, etc)
	
	Dave Huff - June 2015 - original code.
---------------------------------------------------------------------------------------------------------------------*/
int TLPFile::Read(short id) //Load the XML file TLP_xx.xml where xx is the id passed in.
{
	m_ID = id;
	m_FileName = str( format("%s/TLP_%d.xml") % m_ParentDir.c_str() % id);
	ptree pt;

	try
	{
		read_xml(m_FileName, pt);
	}
	catch (std::exception const& e)
	{
		std::cerr << e.what() << std::endl;
		return -2;
	}

	ReadMaps(pt);
	ReadClasses(pt);

	try
	{
		m_Event.Name(pt.get<std::string>("events.event.<xmlattr>.name"));
	}
	catch (std::exception const& e)
	{
		std::cerr << "No name found in event: " << e.what() << std::endl;
		return -2;
	}
	
	try
	{
		m_Event.ID(pt.get<int>("events.event.<xmlattr>.id"));
		std::string apps = pt.get<std::string>("events.event.<xmlattr>.destApp");
		m_Event.AddDestApps(apps);
	}
	catch (std::exception const& e)
	{
		std::cerr << "Warning: Missing ID or destApp value." << std::endl;
	}

	try
	{
		ptree eventEntries = pt.get_child("events.event");
		
		BOOST_FOREACH(const ptree::value_type &v, eventEntries)
		{
			TLPElement element;
			
			if (v.first != "<xmlattr>")
			{
				try
				{
					if (v.first == "map" || v.first == "class")
					{
						const ptree & attributes = v.second;
						element.Set(v.first.data(), v.second.data(), attributes.get<std::string>("<xmlattr>.name"));
					}
					else
						element.Set(v.first.data(), v.second.data() );
					m_Event.AddElement(element);
				}
				catch (std::exception const& e)
				{
					std::cerr << "No Attributes: " << e.what() << std::endl;
				}
			}
		}
	}
	catch (std::exception const& e)
	{
		std::cerr << "Probable Empty List: " << e.what() << std::endl;
	}
	
	cerr << "Finished Reading " << m_FileName << std::endl;
	return 0;
}

/*---------------------------------------------------------------------------------------------------------------------
	Dave Huff - June 2015 - original code.
---------------------------------------------------------------------------------------------------------------------*/
void TLPFile::ReadMaps
(
	ptree &pt
)
{
	try
	{
		ptree maps = pt.get_child("events.maps");
		BOOST_FOREACH(const ptree::value_type &v, maps)  // for each map in maps
		{
			TLPMap aMap;

			aMap.Name(v.second.get<std::string>("<xmlattr>.name", ""));

			ptree thisMap = v.second;
			BOOST_FOREACH(const ptree::value_type &vv, thisMap )  // for each mapValue in map
			{
				if (vv.first != "<xmlattr>")
				{
					int id = vv.second.get<int>("<xmlattr>.id", 255);
					std::string value=vv.second.get<std::string>("");

					if (id == 255)
						id = aMap.AddEntry(value);
					else
						id = aMap.AddEntry((unsigned char)(id), value);
				}
			}
			m_Event.AddMap(aMap);
		}
	}
	catch (std::exception const& e)
	{
		std::cerr << "Probable Empty Map: " << e.what() << std::endl;
	}
}

/*---------------------------------------------------------------------------------------------------------------------
	Dave Huff - June 2015 - original code.
---------------------------------------------------------------------------------------------------------------------*/
void TLPFile::ReadClasses
(
	ptree &pt
)
{
	try
	{
		ptree ptClasses = pt.get_child("events.classes");
		BOOST_FOREACH(const ptree::value_type &v, ptClasses)  // for each class in classes
		{
			TLPClass aClass;
			aClass.Name(v.second.get<std::string>("<xmlattr>.name", ""));
			ptree thisClass = v.second;
			
			BOOST_FOREACH(const ptree::value_type &vv, thisClass )  // for each element in the class
			{
				TLPElement element;
				if (vv.first != "<xmlattr>")
				{
					try
					{
						if (vv.first == "map" || vv.first == "class")
						{
							const ptree & attributes = vv.second;
							element.Set(vv.first.data(), vv.second.data(), attributes.get<std::string>("<xmlattr>.name"));
						}
						else
							element.Set(vv.first.data(), vv.second.data() );
							
						aClass.AddElement(element);
					}
					catch (std::exception const& e)
					{
						std::cerr << "No Attributes: " << e.what() << std::endl;
					}
				}
			}
			
			m_Event.AddClass(aClass);
		}
	}

	catch (std::exception const& e)
	{
		std::cerr << "Probable Empty Map: " << e.what() << std::endl;
	}
}


/*---------------------------------------------------------------------------------------------------------------------
	Encode: take the input string in the form "a=x b=y..." and encode the buffer
	
	Input: parentDir - the path to the parent directory
	
	Return:	0: Encoded OK
					-1: No XML file loaded yet
					-2: Invalid parameters in input string (doesn't match the XML definition)
	
	Dave Huff - June 2015 - original code.
---------------------------------------------------------------------------------------------------------------------*/
int TLPFile::Encode(std::string strData, vector <char> &output)
{
	// convert input string into smData string map
	CommandBuffer cb;
	init_CommandBuffer(&cb);
  const char * err;

	if ((err = gen_arg_list(strData.c_str(), (int)strData.length(), &cb)))
	{
		return -2;
	}
	
	ats::StringMap s;
	s.from_args(cb.m_argc, cb.m_argv);
	return Encode(s, output);
}

/*---------------------------------------------------------------------------------------------------------------------
	Encode: take the input stringmap and encode the buffer
	
	Input: parentDir - the path to the parent directory
	
	Return: 
						-1: No XML file loaded yet

	
	Dave Huff - June 2015 - original code.
---------------------------------------------------------------------------------------------------------------------*/
int TLPFile::Encode(ats::StringMap &smData, vector <char> &output)
{
	return m_Event.Encode(smData, output);
}

/*---------------------------------------------------------------------------------------------------------------------
	Decode: take the input buffer and decode a string in the form "a=x b=y..."
	Input: 
	Return:	 0: Decode OK
					-1: No XML file loaded yet
					-2: incoming vector short than required
					-3: incoming vector longer than required
	
	Dave Huff - June 2015 - original code.
---------------------------------------------------------------------------------------------------------------------*/
int TLPFile::Decode(vector <char> inputData, std::string output)
{
	ats::StringMap strMap;
	int ret = Decode(inputData, strMap);
	
	if (ret < 0)
		return ret;
	
	output.empty();
	
	typedef std::pair <std::string, std::string> mapPair;

	BOOST_FOREACH( mapPair entry, strMap)
	{
		output += entry.first;
		output += "=";
		output += entry.second;
	}

	return 0;
}

/*---------------------------------------------------------------------------------------------------------------------
	Decode: take the input buffer and decode the data into a stringmap of [Name] [Value]
	Input: parentDir - the path to the parent directory
	Return:	 0: Decode OK
					-1: No XML file loaded yet
					-2: incoming vector short than required
					-3: incoming vector longer than required
	
	Dave Huff - June 2015 - original code.
---------------------------------------------------------------------------------------------------------------------*/
int TLPFile::Decode(vector <char> inputData, ats::StringMap & output)
{
	if (m_FileName.length() == 0)
		return -1;

	output.empty();  // start with an empty string
	
	m_Event.Decode(inputData, output);
	
	return 0;
}

/*---------------------------------------------------------------------------------------------------------------------
	dump: dump the contents of the class to cerr

	Dave Huff - June 2015 - original code.
---------------------------------------------------------------------------------------------------------------------*/
void TLPFile::dump() const 
{
  std::cerr << "Parent Directory: " << m_ParentDir << endl;
  std::cerr << "Current File: " << m_FileName << endl;
  m_Event.dump(); 
}
 
bool TLPFile::operator==(const short id) const
{
	return (id == m_ID);
}
