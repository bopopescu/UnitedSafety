#pragma once
#include "messagedatabase.h"

#include "packetizerDB.h"

class PacketizerDashDB : public PacketizerDB
{
public:
	PacketizerDashDB(MyData& pData,const ats::String p_packetizerdb_name, const ats::String p_packetizerdb_path);
	bool dbcopy(int mid);
};


