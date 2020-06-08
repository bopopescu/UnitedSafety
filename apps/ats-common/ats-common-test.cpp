#include <stdio.h>

#include "ats-common.h"
#include "atslogger.h"

static void print_label(const ats::String& p_label)
{
	printf(
		"\n"
		"=========================================================================\n"
		"= %s\n"
		"=========================================================================\n",
		p_label.c_str()
		);
	fflush(stdout);
}

int main()
{
	int errors = 0;

	print_label("Testing ats::rtrim");
	{
		printf("\tStrip 0x0D and 0x0A...");
		fflush(stdout);
		{
			const ats::String test("Test string");

			const ats::String& result = ats::rtrim(test + "\r\n", "\r\n");
			bool pass = (result == test);
			printf("%s\n", pass ? "PASS" : "FAIL");

			if(!pass)
			{
				printf("\t\tresult is: \"%s\"\n", result.c_str());
			}
		}

		printf("\tStrip 0x0A...");
		fflush(stdout);
		{
			ats::String test("Test string");
			ats::String test2(test + "\n");

			ats::rtrim_newline(test2);
			bool pass = (test == test2);
			printf("%s\n", pass ? "PASS" : "FAIL");

			if(!pass)
			{
				printf("\t\tstring1=\"%s\", string2=\"%s\"\n", test.c_str(), test2.c_str());
			}
		}

		printf("\tStrip only one 0x0A character...");
		fflush(stdout);
		{
			ats::String test("Test string");
			ats::String test2(test + "\n\n");

			ats::rtrim_newline(test2);
			bool pass = ((test + "\n") == test2);
			printf("%s\n", pass ? "PASS" : "FAIL");

			if(!pass)
			{
				printf("\t\tstring1=\"%s\", string2=\"%s\"\n", test.c_str(), test2.c_str());
			}
		}


		printf("\tStrip an empty string...");
		fflush(stdout);
		{
			const ats::String test;

			const ats::String& result = ats::rtrim(test, "\r\n");
			bool pass = (result == test);
			printf("%s\n", pass ? "PASS" : "FAIL");

			if(!pass)
			{
				printf("\t\tresult is: \"%s\"\n", result.c_str());
			}
		}

		printf("\tStrip pure white space...");
		fflush(stdout);
		{
			const ats::String test("          \r        \n  \r\n\r\n\r\r\r\r           \t\t\t\r\n\r\n\t\r  \t \t \r \t  ");

			const ats::String& result = ats::rtrim(test, " \t\n\r");
			bool pass = ("" == result);
			printf("%s\n", pass ? "PASS" : "FAIL");

			if(!pass)
			{
				printf("\t\tresult is: \"%s\"\n", result.c_str());
			}
		}

	}

	print_label("Testing ats::StringMap");

	ats::StringMap m;
	m.set("1:y", "y");
	m.set("1:yes", "yes");
	m.set("0:yess", "yess");
	m.set("0:t", "t");
	m.set("1:true", "true");
	m.set("1:on", "on");
	m.set("1:1", "1");
	m.set("1:-1", "-1");
	m.set("0:", "");
	m.set("1:0xff", "0xff");
	m.set("1:set", "set");
	m.set("0:Y", "Y");
	m.set("0:0.123", "0.123");

	ats::StringMap::const_iterator i = m.begin();

	while(i != m.end())
	{
		const ats::String& key = i->first;
		const ats::String& val = i->second;
		++i;

		printf("\tTesting \"%s\"...", val.c_str());
		fflush(stdout);

		const bool value = m.get_bool(key);
		const bool pass = (!key.compare(0, 1, "1")) == value;

		if(!pass)
		{
			++errors;
		}

		printf("value is %s...%s\n", value ? "true" : "false", pass ? "PASS" : "FAIL");

	}

	print_label("Testing ATSLogger class");
	{
		ATSLogger log;
		log.open("./atslogger.log", false);

		int ret = ats_log(log, "Hello, World");
		ats_log(log, "\"ats_log\" returned " + ats::toStr(ret));

		ret = ats_logf(&log, "%s,%d:%s: Hello, World", __FILE__, __LINE__, __PRETTY_FUNCTION__);
		ats_logf(&log, "%s,%d:%s: \"ats_logf\" returned %d", __FILE__, __LINE__, __PRETTY_FUNCTION__, ret);

	}

	return (!errors) ? 0 : 1;
}
