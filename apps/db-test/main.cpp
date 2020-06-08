#include <stdio.h>

#include "ats-common.h"
#include "socket_interface.h"
#include "db-monitor.h"

int main(int argc, char* argv[])
{

	if(argc < 2)
	{
		fprintf(stderr, "usage: %s <sql query> [database name]\n", argv[0]);
		return 1;
	}

	//===================================================================
	// 1. Connect to db-monitor
	//===================================================================
	ClientSocket cs;
	init_ClientSocket(&cs);
	connect_client(&cs, "127.0.0.1", 41012);
	db_monitor::DBMonitorContext db(cs);

	//===================================================================
	// 2. Perform a query
	//    argv[1] = Query
	//    argv[2] = Database name (optional)
	//===================================================================
	const ats::String& err = db_monitor::query(db, (argc >= 3) ? argv[2] : "", argv[1]);

	if(!err.empty())
	{
		printf("Error: %s\n", err.c_str());
		return 1;
	}

	//===================================================================
	// 3. Process results
	//===================================================================
	// Display all of the columns
	{
		printf("Total Columns: %d\n", int(db.m_column.size()));
		size_t i;

		for(i = 0; i < db.m_column.size(); ++i)
		{
			printf("%s%s", i ? " " : "", db.m_column[i].c_str());
		}

		printf("\n");
	}

	// Disaply all rows (and all values in each row)
	{
		printf("Total Rows: %d\n", int(db.m_table.size()));
		size_t i;

		for(i = 0; i < db.m_table.size(); ++i)
		{
			db_monitor::ResultRow& row = db.m_table[i];
			size_t i;

			for(i = 0; i < row.size(); ++i)
			{
				printf("%s'%s'", i ? " " : "", row[i].c_str());
			}

			printf("\n");
		}

	}

	return 0;
}
