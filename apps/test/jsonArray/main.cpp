#include <stdio.h>
#include <string>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

using namespace rapidjson;

//------------------------------------------------------------------------------
void dLog(const Document& dom)
{
	StringBuffer buffer;
	Writer<StringBuffer> writer(buffer);
	dom.Accept(writer);
	std::string rs = buffer.GetString();
	printf( "%s\n", rs.c_str());
}
//------------------------------------------------------------------------------
void vLog(const Value& val)
{
	StringBuffer buffer;
	Writer<StringBuffer> writer(buffer);
	val.Accept(writer);
	std::string rs = buffer.GetString();
	printf("%s\n", rs.c_str());
}
	
int main()
{
	std::string result;  // hex data for adding to message

	Document dom;
	Document::AllocatorType &allocator = dom.GetAllocator();
	dom.SetObject();

	std::string base = "https://dave/huff/";
	std::string sn = "Daves#1";
	std::string url;
	url = base + sn + "/createinst";
	dom.AddMember("url", StringRef(url.c_str(), url.size()), allocator);

	// now build the data to be sent
	Value dataObj(kObjectType);
	dataObj.SetObject();

	std::string ts = "2019-01-27 11:22:31";
	int m_Sequence = 0;
	dataObj.AddMember("sn", StringRef(sn.c_str(), sn.size()), allocator);
	dataObj.AddMember("t", StringRef(ts.c_str(), ts.size()), allocator);
	dataObj.AddMember("seq", ++m_Sequence, allocator);
	dataObj.AddMember("s",  ++m_Sequence, allocator);

	std::string user = "DavidRoy Huff";
	dataObj.AddMember("user", StringRef(user.c_str(), user.size()), allocator);
	std::string site = "TheFourSeasons";
	dataObj.AddMember("site", StringRef(site.c_str(), site.size()), allocator);

	Value sensorArray(kArrayType);
	sensorArray.SetArray();
	
	std::string types[4]  = {"S111", "s222", "S333", "s444"};
	std::string gasses[4] = {"G001", "G002", "G003", "G004"};
	double reading[4] = {1.2, 2.4, 3.9, 4.16};
	
	for (short i = 0; i < 4; i++)
	{
		Value sensorObj(kObjectType);
		sensorObj.SetObject();
		std::string cc = types[i];
		sensorObj.AddMember("cc", StringRef(cc.c_str(), cc.size()), allocator);
		std::string gc = gasses[i];
		sensorObj.AddMember("gc", StringRef(gc.c_str(), gc.size()), allocator);
		sensorObj.AddMember("gr", reading[i], allocator);
		vLog(sensorObj);
		sensorArray.PushBack(sensorObj, allocator);
		vLog(sensorArray);
	}
	dataObj.AddMember("sensors", sensorArray, allocator);
	dom.AddMember("data", dataObj, allocator);
	dLog(dom);
	return 0;
}
