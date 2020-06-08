#include "atslogger.h"
#include "socket_interface.h"
#include "db-monitor.h"

#include "messagedatabase.h"

extern ATSLogger g_log;
static const char* g_db_monitor_unix_socket = "db-monitor";

const ats::String	MessageDatabase::m_db_name("messages_db");
const ats::String	MessageDatabase::m_db_file("/mnt/update/database/messages.db");

ats::String	MessageDatabase::m_db_mt_columnnames[]={
	"mid",//0
	"msg_priority",//1
	"event_time",//2
	"fix_time",//3
	"latitude",//4
	"longitude",//5
	"altitude",//6
	"speed",//7
	"heading",//8
	"satellites",//9
	"fix_status",//10
	"hdop",//11
	"inputs",//12
	"unit_status",//13
	"event_type",//14
	"acum0",//15
	"acum1",//16
	"acum2",//17
	"acum3",//18
	"acum4",//19
	"acum5",//20
	"acum6",//21
	"acum7",//22
	"acum8",//23
	"acum9",//24
	"acum10",//25
	"acum11",//26
	"acum12",//27
	"acum13",//28
	"acum14",//29
	"acum15",//30
	"usr_msg_route",//31
	"usr_msg_id",//32
	"usr_msg_data",//33
	"rssi",//34
	"mobile_id", //35
	"mobile_id_type" //36
};


//if add new field into message_table, make sure message_table column count is right.
#define message_table_column_count 37
const ats::String	MessageDatabase::m_create_table =
	"CREATE TABLE IF NOT EXISTS "
	"message_table("
	"	mid		 INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, "
	"	msg_priority	 INTEGER, "
	"	event_time	 TIMESTAMP, "
	"	fix_time	 TIMESTAMP, "
	"	latitude	 DOUBLE, "
	"	longitude	 DOUBLE, "
	"	altitude	 DOUBLE, "
	"	speed		 DOUBLE, "
	"	heading		 DOUBLE, "
	"	satellites	 INTEGER, "
	"	fix_status	 INTEGER, "
	"	hdop		 FLOAT, "
	"	inputs		 INTEGER, "
	"	unit_status	 INTEGER, "
	"	event_type	 INTEGER, "
	"	acum0		 INTEGER, "
	"	acum1		 INTEGER, "
	"	acum2		 INTEGER, "
	"	acum3		 INTEGER, "
	"	acum4		 INTEGER, "
	"	acum5		 INTEGER, "
	"	acum6		 INTEGER, "
	"	acum7		 INTEGER, "
	"	acum8		 INTEGER, "
	"	acum9		 INTEGER, "
	"	acum10		 INTEGER, "
	"	acum11		 INTEGER, "
	"	acum12		 INTEGER, "
	"	acum13		 INTEGER, "
	"	acum14		 INTEGER, "
	"	acum15		 INTEGER, "
	"	usr_msg_route	 INTEGER, "
	"	usr_msg_id	 INTEGER, "
	"	usr_msg_data	 TEXT,"
	"	rssi		 INTEGER, "
	"	mobile_id	 TEXT, "
	"	mobile_id_type   INTEGER"
	"); "
	"CREATE INDEX IF NOT EXISTS event_time "
	"ON message_table(event_time);"
	"CREATE TRIGGER IF NOT EXISTS clean_up "
	"BEFORE INSERT ON message_table "
	"BEGIN "
	"	DELETE FROM message_table "
	"	WHERE event_time < datetime('now', '-86400 seconds'); "
	"END;";

const ats::String MessageDatabase::m_create_message_type_table(
	"CREATE TABLE message_types("
	"mid INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
	"name TEXT,"
	"cantel_id NUMBER,"
	"calamp_id INTEGER"
	");"
	"BEGIN TRANSACTION;"
	"INSERT INTO message_types VALUES(1,'scheduled_message',1,200);"
	"INSERT INTO message_types VALUES(2,'speed_exceeded',2,200);"
	"INSERT INTO message_types VALUES(3,'ping',3,200);"
	"INSERT INTO message_types VALUES(4,'stop_condition',4,200);"
	"INSERT INTO message_types VALUES(5,'start_condition',5,5);"
	"INSERT INTO message_types VALUES(6,'ignition_on',6,100);"
	"INSERT INTO message_types VALUES(7,'ignition_off',7,101);"
	"INSERT INTO message_types VALUES(8,'heartbeat',8,'8');"
	"INSERT INTO message_types VALUES(10,'sensor',10,10);"
	"INSERT INTO message_types VALUES(11,'power_on',11,11);"
	"INSERT INTO message_types VALUES(12,'speed_acceptable',12,12);"
	"INSERT INTO message_types VALUES(16,'hard_brake',16,16);"
	"INSERT INTO message_types VALUES(17,'low_batt',17,17);"
	"INSERT INTO message_types VALUES(19,'sos',19,19);"
	"INSERT INTO message_types VALUES(20,'help',20,20);"
	"INSERT INTO message_types VALUES(21,'ok',21,21);"
	"INSERT INTO message_types VALUES(23,'power_off',23,23);"
	"INSERT INTO message_types VALUES(24,'check_in',24,24);"
	"INSERT INTO message_types VALUES(25,'fall_detected',25,25);"
	"INSERT INTO message_types VALUES(26,'check_out',26,26);"
	"INSERT INTO message_types VALUES(27,'not_check_in',27,27);"
	"INSERT INTO message_types VALUES(30,'gps_fix_invalid',30,30);"
	"INSERT INTO message_types VALUES(31,'fuel_log',31,31);"
	"INSERT INTO message_types VALUES(32,'driver_status',32,32);"
	"INSERT INTO message_types VALUES(33,'engine_on',33,33);"
	"INSERT INTO message_types VALUES(34,'engine_off',34,34);"
	"INSERT INTO message_types VALUES(35,'engine_trouble_code',35,35);"
	"INSERT INTO message_types VALUES(36,'param_exceed',36,36);"
	"INSERT INTO message_types VALUES(37,'period_report',37,37);"
	"INSERT INTO message_types VALUES(38,'other',38,38);"
	"INSERT INTO message_types VALUES(39,'switch_int_power',39,39);"
	"INSERT INTO message_types VALUES(40,'switch_wired_power',40,40);"
	"INSERT INTO message_types VALUES(41,'odometer_update',41,41);"
	"INSERT INTO message_types VALUES(42,'accel_acceptable',42,42);"
	"INSERT INTO message_types VALUES(43,'deccel_acceptable',43,43);"
	"INSERT INTO message_types VALUES(44,'engine_param_normal',44,44);"
	"INSERT INTO message_types VALUES(110,'critt_batt',110,'110');"
	"INSERT INTO message_types VALUES(256,'calamp_user_msg','',256);"
	"COMMIT;"
	);

void MessageDatabase::InitMessageTypeDatabase()
{
	ClientSocket cs;
	init_ClientSocket(&cs);
	connect_redstone_ud_client(&cs, g_db_monitor_unix_socket);

	db_monitor::DBMonitorContext db(cs);
	{
		const ats::String err = db_monitor::open_db(db, m_db_name, m_db_file);
		if(!err.empty())
		{
			ats_logf(&g_log, "Error opening database: %s",err.c_str());
			close_ClientSocket(&cs);
			return;
		}
	}
	const ats::String& err = db_monitor::query(db, m_db_name, m_create_message_type_table);
	if(!err.empty())
	{
		ats_logf(&g_log, "Error: %s", err.c_str());
	}
	close_ClientSocket(&cs);
}

void MessageDatabase::MessageTableSanityCheck()
{
	ClientSocket cs;
	init_ClientSocket(&cs);
	connect_redstone_ud_client(&cs, g_db_monitor_unix_socket);

	db_monitor::DBMonitorContext db(cs);
	{
		const ats::String err = db_monitor::open_db(db, m_db_name, m_db_file);
		if(!err.empty())
		{
			ats_logf(&g_log, "Error opening data: %s", err.c_str());
			close_ClientSocket(&cs);
			return;
		}
	}

	const ats::String& err = db_monitor::query(db, m_db_name, "PRAGMA table_info(message_table);");
	if(!err.empty())
	{
		ats_logf(&g_log, "Error: %s", err.c_str());
	}

	if( db.m_table.size() != message_table_column_count)
	{
		{
			const ats::String& drop = "drop table if exists message_table" ;
			const ats::String& err = db_monitor::query(db, m_db_name, drop.c_str());
			if(!err.empty())
			{
				ats_logf(&g_log, "Error: %s", err.c_str());
			}
		}

		{
			const ats::String& err = db_monitor::query(db, m_db_name, m_create_table);
			if(!err.empty())
			{
				ats_logf(&g_log, "Error: %s", err.c_str());
			}
		}
	}
	close_ClientSocket(&cs);
}

void MessageDatabase::executeQuery(const ats::String& query)
{
	ClientSocket cs;
	init_ClientSocket(&cs);
	connect_redstone_ud_client(&cs, g_db_monitor_unix_socket);
	db_monitor::DBMonitorContext db(cs);

	{
		const ats::String err = db_monitor::open_db(db, m_db_name, m_db_file);

		if(!err.empty())
		{
			ats_logf(&g_log, "Error opening database: %s", err.c_str());
			close_ClientSocket(&cs);
			return;
		}
	}

	const ats::String& err = db_monitor::query(db, m_db_name, query);

	if(!err.empty())
	{

		const ats::String& err = db_monitor::query(db, m_db_name, m_create_table + query);

		if(!err.empty())
		{
			ats_logf(&g_log, "Error: %s", err.c_str());
			ats_logf(&g_log, "\tQuery:\n%s", query.c_str());
		}

	}

	close_ClientSocket(&cs);
}

ats::String MessageDatabase::escapeValue(const ats::String& col, const ats::String& val)
{
	if(col.find("time") != std::string::npos)
	{
		if(val.find("datetime") == std::string::npos)
		{
			return "'" + val + "'";
		}
	}
	else if(col == "usr_msg_data")
	 {
		 return "'" + val + "'";
	 }

	return val;
}
//----------------------------------------------------------------------------------------------------
long MessageDatabase::GetLastMID()
{
	ClientSocket cs;
	init_ClientSocket(&cs);
	connect_redstone_ud_client(&cs, g_db_monitor_unix_socket);
	db_monitor::DBMonitorContext db(cs);

	{
		const ats::String err = db_monitor::open_db(db, m_db_name, m_db_file);

		if(!err.empty())
		{
			ats_logf(&g_log, "Error opening database: %s", err.c_str());
			close_ClientSocket(&cs);
			return -1;
		}
	}

	ats::String buf;
	ats_sprintf(&buf, "SELECT MAX(mid) FROM message_table");
	const ats::String& err = db_monitor::query(db, m_db_name, buf.c_str());

	if(!err.empty())
	{
		ats_logf(&g_log, "Error: %s - Unable to GetLastMID", err.c_str());
	}

	db_monitor::ResultRow& row = db.m_table[db.m_table.size() - 1];
	close_ClientSocket(&cs);
	return strtol(row[row.size()-1].c_str(),0,0);
}
