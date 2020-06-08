#include <iostream>
#include <stdio.h>

#include "db-monitor.h"
#include "ConfigDB.h"


int main( int argc, char* argv[])
{
	ats::String app_name("test-db-monitor");

	if(argc >= 1)
	{
		app_name = argv[0];
	}

	db_monitor::ConfigDB db;

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
		const ats::String& err = db.open_db( "db_key", "/tmp/db_key_test.db");

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

	printf("4. Testing Opening of config DB...");
	fflush(stdout);
	{
		const ats::String& err = db.open_db_config();

		if(!err.empty())
		{
			printf("FAIL(%d): %s\n", __LINE__, err.c_str());
			return 1;
		}

		printf("PASS\n");
	}

	printf("5. Testing setting a config value...");
	fflush(stdout);
	{
		const ats::String& err = db.set_config(app_name, "key_test", "key_value");
		
		if(!err.empty())
		{
			printf("FAIL(%d): %s\n", __LINE__, err.c_str());
			return 1;
		}

		printf("PASS\n");
	}

	printf("6. Testing getting a config value...");
	fflush(stdout);
	{
		const ats::String& strVal = db.GetValue(app_name, "key_test");

		if (strVal != "key_value")
		{
			printf("FAIL(%d): Expected value=\"key_test\", but got value=\"%s\"\n", __LINE__, ((db.Table()[0])[1]).c_str());
			return 1;
		}

		printf("PASS\n");
	}

	printf("7. Testing setting an integer value...");
	fflush(stdout);
	{
		const ats::String& err = db.set_config(app_name, "key_test", "100");
		
		if(!err.empty())
		{
			printf("FAIL(%d): %s\n", __LINE__, err.c_str());
			return 1;
		}

		printf("PASS\n");
	}

	printf("8. Testing getting an integer value...");
	fflush(stdout);
	{
		int iVal = db.GetInt(app_name, "key_test", -999);

		if (iVal != 100)
		{
			printf("FAIL(%d): Expected value=100, but got value=%d\n", __LINE__, iVal);
			return 1;
		}

		printf("PASS\n");
	}
	printf("\nTEST PASSED\n");

	return 0;
}
