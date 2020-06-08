#pragma once

#include "ats-common.h"

typedef enum {
	MT_MID = 0,
	MT_MSG_PRIORITY = 1,
	MT_EVENT_TIME = 2,
	MT_FIX_TIME = 3,
	MT_LATITUDE = 4,
	MT_LONGITUDE = 5,
	MT_ALTITUDE = 6,
	MT_SPEED = 7,
	MT_HEADING = 8,
	MT_SATELLITES = 9,
	MT_FIX_STATUS = 10,
	MT_HDOP = 11,
	MT_INPUTS = 12,
	MT_UNIT_STATUS = 13,
	MT_EVENT_TYPE = 14,
	MT_ACCUM0 = 15,
	MT_ACCUM1 = 16,
	MT_ACCUM2 = 17,
	MT_ACCUM3 = 18,
	MT_ACCUM4 = 19,
	MT_ACCUM5 = 20,
	MT_ACCUM6 = 21,
	MT_ACCUM7 = 22,
	MT_ACCUM8 = 23,
	MT_ACCUM9 = 24,
	MT_ACCUM10 = 25,
	MT_ACCUM11 = 26,
	MT_ACCUM12 = 27,
	MT_ACCUM13 = 28,
	MT_ACCUM14 = 29,
	MT_ACCUM15 = 30,
	MT_USR_MSG_ROUTE = 31,
	MT_USR_MSG_ID = 32,
	MT_USR_MSG_DATA = 33,
	MT_RSSI = 34,
	MT_MOBILE_ID = 35,
	MT_MOBILE_ID_TYPE = 36
} MT_COLUMN_NAMES; //message_table column names

class MessageDatabase
{
public:
	static const ats::String	m_db_name;
	static const ats::String	m_db_file;
	static const ats::String	m_create_table;
	static const ats::String	m_create_message_type_table;
	static ats::String    m_db_mt_columnnames[];
	static void executeQuery(const ats::String& query);
	static ats::String escapeValue(const ats::String& col, const ats::String& val);
	static void InitMessageTypeDatabase();
	static void MessageTableSanityCheck();
	static long GetLastMID();

};
