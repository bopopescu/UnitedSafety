#pragma once

#include "packetizerDB.h"

class PacketizerIridiumDB : public PacketizerDB
{
public:
	PacketizerIridiumDB(MyData& pData,const ats::String p_packetizerdb_name, const ats::String p_packetizerdb_path);
	bool dbcopy(int mid);
	void dbEmpty();
};
