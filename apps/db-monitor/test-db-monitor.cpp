#include <iostream>
#include <stdio.h>

#include "db-monitor.h"

int main( int argc, char* argv[])
{
	ats::String app_name("test-db-monitor");

	if(argc >= 1)
	{
		app_name = argv[0];
	}

	db_monitor::DBMonitorContext db;

	printf("1. Testing Opening DB for the first time...");
	fflush(stdout);
	{
		const ats::String& err = db.open_db("db_key", "/tmp/db_key_test.db");

		if(!err.empty())
		{
			printf("FAIL(%d): %s\n", __LINE__, err.c_str());
			return 1;
		}

		printf("PASS\n");
	}

	printf("2. Testing Opening DB for the second time...");
	fflush(stdout);
	{
		const ats::String& err = db.open_db("db_key", "/tmp/db_key_test.db");

		if(!err.empty())
		{
			printf("FAIL(%d): %s\n", __LINE__, err.c_str());
			return 1;
		}

		printf("PASS\n");
	}

	printf("3. Testing Opening DB for the 2nd time with a different file name...");
	fflush(stdout);
	{
		const ats::String& err = db.open_db("db_key", "/tmp/db_key_alternate_file_test.db");

		if(err.empty())
		{
			printf("FAIL(%d): Expected error about using a different file with the same key\n", __LINE__);
			return 1;
		}

		printf("PASS\n");
	}

	printf("\nTEST PASSED\n");

	return 0;
}
