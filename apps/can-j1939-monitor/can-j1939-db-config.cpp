#include <iostream>

#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "socket_interface.h"
#include "can-j1939-db.h"

ats::String db_key;
static const ats::String& g_db_pgn_key("pgndb");
static const ats::String& g_db_config_key("configdb");

static void print_usage(const char* p_prog_name)
{
	fprintf(stderr,

		"Usage: %s <pgndb/configdb> <set/get/unset> <pgn number> [path to xml file]\n"
		"\n"
		"Options:\n"
		"   --ignore_errors\n"
		"           Try to continue when errors are detected (for example, a file not found will be treated as the\n"
		"           empty string input, and processing can continue). Note that some errors cannot be ignored, and\n"
		"           will cause further processing (such as set/get) to abort.\n"
		"\n"
		"   -n\n"
		"           Do not print a newline at the end of the value (only used with the \"-v\" option).\n"
		"\n"
		"   -u\n"
		"           Update only (applies to \"set\" command only). When a request to set a value is given, the value\n"
		"           will only be set if it differs from the value currently in the configuration database.\n"
		"           If the value does not exist, then it is also interpreted as \"differing\".\n"
		"           This option is useful for eliminating redundant NVRAM writes for the configuration database\n"
		"           (as NVRAM contains limited write-cycles).\n"
		"\n"
		"   -v, --value_only\n"
		"           When getting a specific value, only return the value part (not the key or other info).\n"
		"\n"
		"   -b, --brief\n"
		"           When getting a specific value, return strings as\n"
		"           app  value\n"
		"           on a single line\n"
		"\n"

		,p_prog_name);
}

static int get_args_and_options(std::vector<ats::String>& p_arg, ats::StringMap& p_opt, int p_argc, char* p_argv[])
{
	int i;

	for(i = 0; i < p_argc; ++i)
	{
		const ats::String& arg = p_argv[i];

		if("--" == arg) // End of options
		{

			for(++i; i < p_argc; ++i)
			{
				p_arg.push_back(p_argv[i]);
			}

			break;
		}
		else if('-' != (arg.c_str())[0]) // Not an option
		{
			p_arg.push_back(arg);

			continue;
		}
		else if('-' == (arg.c_str())[1]) // Is a long option (a name/word)
		{
			ats::String key;
			ats::String val;
			ats::StringMap::get_key_val(arg.c_str() + 2, key, val);
			p_opt.set(key, val);
			continue;
		}

		// Is a short option
		const char* s = arg.c_str() + 1;

		while(*s)
		{
			const char c = *(s++);

			switch(c)
			{
			case 'b': // Return just the value part
				p_opt.set("brief", "1");
				break;

			case 'n':
				p_opt.set("no_newline", "1");
				break;

			case 'u':
				p_opt.set("update_only", "1");
				break;

			case 'v': // Return just the value part
				p_opt.set("value_only", "1");
				break;

			default:
				fprintf(stderr, "Error: Invalid option '%c'\n", c);
				return 1;
			}
		}

	}

	return 0;
}

static int read_from_file(FILE* p_f, ats::String& p_des)
{
	p_des.clear();
	char buf[256];

	for(;;)
	{
		const size_t nread = fread(buf, 1, 256, p_f);

		if(!nread)
		{

			if(!feof(p_f))
			{
				return -EIO;
			}

			return 0;
		}

		p_des.append(buf, nread);
	}
}


int main( int argc, char* argv[])
{

	std::vector<ats::String> arg;
	ats::StringMap opt;
	get_args_and_options(arg, opt, argc - 1, argv + 1);

	if(arg.size() < 2)
	{
		print_usage(argv[0]);
		return 1;
	}

	const bool value_only = opt.get_bool("value_only");
	const bool brief = opt.get_bool("brief");

	ats::String dbkey = ats::String(arg[0]);
	if(g_db_pgn_key != dbkey && g_db_config_key != dbkey )
	{
		print_usage(argv[0]);
		return 1;
	}

	db_key = dbkey;

	const ats::String cmd(arg[1]);

	const bool has_pgn = (arg.size() >= 3);

	const ats::String pgn(has_pgn ? arg[2] : "");

	CanJ1939DB db;
	const ats::String& err = db.open_db_config();

	if(!err.empty())
	{
		fprintf(stderr, "ERROR: %s\n", err.c_str());
		return 1;
	}

	const bool ignore_errors = opt.has_key("ignore_errors");

	int error = 0;

	if("set" == cmd)
	{

		if(arg.size() < 3)
		{
			print_usage(argv[0]);
			return 1;
		}

		ats::String value;

		const ats::String& fname = arg[3];
		FILE* f = fopen(fname.c_str(), "r");

		if(f)
		{
			const int err = read_from_file(f, value);
			fclose(f);

			if(err)
			{
				fprintf(stderr, "ERROR(%d,%s): An error occurred while reading from \"%s\"\n", err, strerror(err), fname.c_str());
				error = 1;
			}

		}
		else
		{
			fprintf(stderr, "ERROR: Could not open \"%s\" for reading\n", fname.c_str());
			error = 1;
		}

		if(error && (!ignore_errors))
		{
			return error;
		}

		ats::String err;

		if(opt.has_key("update_only"))
		{
			err = db.Update(db_key, pgn, value);
		}
		else
		{
			err = db.Set(db_key, pgn, value);
		}

		if(!err.empty())
		{
			fprintf(stderr, "ERROR: %s\n", err.c_str());
			return 1;
		}

		return 0;
	}
	else if("unset" == cmd)
	{
		ats::String err;

		if(has_pgn)
		{
			{
				err = db.Unset(db_key, pgn);
			}
		}
		else
		{
			err = db.Unset(db_key);
		}

		if(!err.empty())
		{
			fprintf(stderr, "ERROR: %s\n", err.c_str());
			return 1;
		}

		return 0;
	}
	else if("get" == cmd)
	{
		ats::String err;

		if(has_pgn)
		{
			err = db.Get(db_key, pgn);
		}
		else
		{
			err = db.Get(db_key);
		}

		if(!err.empty())
		{
			fprintf(stderr, "ERROR: %s\n", err.c_str());
			return 1;
		}

	}
	else
	{
		fprintf(stderr, "ERROR: Invalid command \"%s\"\n", cmd.c_str());
		return 1;
	}

	{
		size_t i;

		for(i = 0; i < db.Table().size(); ++i)
		{
			const ResultRow& row = db.Table()[i];

			if(row.size() < 2)
			{
				fprintf(stderr, "ERROR: Expected at least 2 columns not %d\n", int(row.size()));
				error = 1;
				continue;
			}

			if (brief)
			{
				if((db.Table().size() > 0) && (row.size() >= 2))
				{
					if(row.size() == 2)
						printf("\t%s\t%s\n", row[1].c_str(), row[0].c_str());
					else
						printf("%s\t%s\t%s\n", row[2].c_str(), row[1].c_str(), row[0].c_str());
				}
			}
			else if(!value_only)
			{
				if((db.Table().size() > 0) && (row.size() >= 3))
				{
					printf("%d:   v_App=%s\n", int(i), row[2].c_str());
				}

				printf("%d:     v_Date=%s\n", int(i), row[0].c_str());

				printf("%d:     v_Value=%s\n", int(i), row[1].c_str());
			}
			else
			{
				printf("%s%s", row[1].c_str(), opt.has_key("no_newline") ? "" : "\n");
			}

		}

	}

	return error;
}
