#include <iostream>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>

#include "ats-common.h"
#include "ats-string.h"


int main(int p_argc, char* p_argv[])
{
	std::map <std::string, std::string> m_DBPacketizerColumnNameType;
	
	m_DBPacketizerColumnNameType["mid"] = "INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL";
	m_DBPacketizerColumnNameType["mtid"] = "INTEGER";
	m_DBPacketizerColumnNameType["msg_priority"] = "INTEGER";
	m_DBPacketizerColumnNameType["event_time"] = "TIMESTAMP";
	m_DBPacketizerColumnNameType["fix_time"] = "TIMESTAMP";
	m_DBPacketizerColumnNameType["latitude"] = "DOUBLE";
	m_DBPacketizerColumnNameType["longitude"] = "DOUBLE";
	m_DBPacketizerColumnNameType["altitude"] = "DOUBLE";
	m_DBPacketizerColumnNameType["speed"] = "DOUBLE";
	m_DBPacketizerColumnNameType["heading"] = "DOUBLE";
	m_DBPacketizerColumnNameType["satellites"] = "INTEGER";
	m_DBPacketizerColumnNameType["fix_status"] = "INTEGER";
	m_DBPacketizerColumnNameType["hdop"] = "FLOAT";
	m_DBPacketizerColumnNameType["inputs"] = "INTEGER";
	m_DBPacketizerColumnNameType["unit_status"] = "INTEGER";
	m_DBPacketizerColumnNameType["event_type"] = "INTEGER";
	m_DBPacketizerColumnNameType["acum0"] = "INTEGER";
	m_DBPacketizerColumnNameType["acum1"] = "INTEGER";
	m_DBPacketizerColumnNameType["acum2"] = "INTEGER";
	m_DBPacketizerColumnNameType["acum3"] = "INTEGER";
	m_DBPacketizerColumnNameType["acum4"] = "INTEGER";
	m_DBPacketizerColumnNameType["acum5"] = "INTEGER";
	m_DBPacketizerColumnNameType["acum6"] = "INTEGER";
	m_DBPacketizerColumnNameType["acum7"] = "INTEGER";
	m_DBPacketizerColumnNameType["acum8"] = "INTEGER";
	m_DBPacketizerColumnNameType["acum9"] = "INTEGER";
	m_DBPacketizerColumnNameType["acum10"] = "INTEGER";
	m_DBPacketizerColumnNameType["acum11"] = "INTEGER";
	m_DBPacketizerColumnNameType["acum12"] = "INTEGER";
	m_DBPacketizerColumnNameType["acum13"] = "INTEGER";
	m_DBPacketizerColumnNameType["acum14"] = "INTEGER";
	m_DBPacketizerColumnNameType["acum15"] = "INTEGER";
	m_DBPacketizerColumnNameType["usr_msg_route"] = "INTEGER";
	m_DBPacketizerColumnNameType["usr_msg_id"] = "INTEGER";
	m_DBPacketizerColumnNameType["usr_msg_daCREATE TABLEta"] = "TEXT";
	m_DBPacketizerColumnNameType["rssi"] = "INTEGER";
	m_DBPacketizerColumnNameType["mobile_id"] = "TEXT";
	m_DBPacketizerColumnNameType["mobile_id_type"] = "INTEGER";
	
	std::map<std::string,std::string>::iterator i;

	for(i = m_DBPacketizerColumnNameType.begin(); i != m_DBPacketizerColumnNameType.end(); i++)
	{
		printf("Name %s Type %s\n", i->first.c_str(), i->second.c_str());
	}

	return 0;
}
