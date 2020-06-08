#include <iostream>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <syslog.h>

#include "socket_interface.h"
#include "command_line_parser.h"
#include "ConfigDB.h"
#include "ats-string.h"
#include "atslogger.h"

static void print_usage(const char* p_prog_name)
{
	fprintf(stderr,

		"Usage: %s <set/get/unset> <app name> [key] [value]\n"
		"\n"
		"Options:\n"
		"   -c, --csv\n"
		"           Return values in CSV format (App, Key, Date, Value, Previous Value).\n"
		"           The first line of the CSV response will always be a list of column names.\n"
		"           This means that two lines are always returned if at least a one value is found.\n"
		"\n"
		"   --file=<path to file>\n"
		"           Read the value to set from the specified file (for storing multi-line text, special chars, etc.).\n"
		"\n"
		"   --ignore_errors\n"
		"           Try to continue when errors are detected (for example, a file not found will be treated as the\n"
		"           empty string input, and processing can continue). Note that some errors cannot be ignored, and\n"
		"           will cause further processing (such as set/get) to abort.\n"
		"\n"
		"   -n\n"
		"           Do not print a newline at the end of the value (only used with the \"-v\" option).\n"
		"\n"
		"   --stdin\n"
		"           Read the value to set from standard input (for storing multi-line text, special chars, etc.).\n"
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
		"           app_name  key  value\n"
		"           on a single line\n"
		"\n"
		"   --bool\n"
		"          Only valid with the \"-v\" option. Causes \"0\" or \"1\" to be returned based on the boolean\n"
		"          result of the value queried.\n"
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
			case 'c': // CSV format
				p_opt.set("csv", "1");
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

static void write_config_settings(const db_monitor::ConfigDB& p_db, FILE* p_des)
{
	const db_monitor::ResultTable& t = p_db.Table();

	for(size_t i = 0; i < t.size(); ++i)
	{
		const db_monitor::ResultRow& r = t[i];

		if(r.size() < 5)
		{
			continue;
		}

		const int app = 4;
		const int key = 3;
		const int val = 1;
		fprintf(p_des, "set \"%s\" \"%s\" \"%s\"\n",
			ats::to_line_printable(r[app]).c_str(),
			ats::to_line_printable(r[key]).c_str(),
			ats::to_line_printable(r[val]).c_str());
	}

}

//------------------------------------------------------------------------------------------------
//	write_generic_config_settings
//
//  called via "db-export generic"
//
//	function to output all non-device specific settings to a settings.txt file
//  device specific settings are IMEIs, MAC addresses, HW types (GPS), SN related items (WiFi ssid)
//	Dave Huff - Nov 2014
static void write_generic_config_settings(const db_monitor::ConfigDB& p_db, FILE* p_des)
{
	const db_monitor::ResultTable& t = p_db.Table();
	const int app = 4;
	const int key = 3;
	const int val = 1;

	for(size_t i = 0; i < t.size(); ++i)
	{
		const db_monitor::ResultRow& r = t[i];

		if(r.size() < 5)
		{
			continue;
		}

		// check for the things we skip - hardware/unit specific values.
		if (ats::to_line_printable(r[app]) == "RedStone" &&  (ats::to_line_printable(r[key]) == "GPS" || ats::to_line_printable(r[key]) == "IMEI") )
		{
			continue;
		}
		if (ats::to_line_printable(r[app]) == "WiFi" &&  (ats::to_line_printable(r[key]) == "password" || ats::to_line_printable(r[key]) == "ssid") )
		{
			continue;
		}
		if (ats::to_line_printable(r[app]) == "system" &&  (ats::to_line_printable(r[key]) == "ethmacaddr" || ats::to_line_printable(r[key]) == "wifimacaddr") )
		{
			continue;
		}
		if (ats::to_line_printable(r[app]) == "Iridium" &&  ats::to_line_printable(r[key]) == "IMEI" )
		{
			continue;
		}
		if (ats::to_line_printable(r[app]) == "zigbee" &&  ats::to_line_printable(r[key]) == "written-link-key" )
		{
			continue;
		}

		if (ats::to_line_printable(r[app]) == "j1939-db" || ats::to_line_printable(r[app]) == "modbus-db" )
		{
		    const ats::String& Key = ats::to_line_printable(r[key]);
			size_t found = Key.find("template_");
			if(found != std::string::npos)
			{
			  continue;
			}
		}

		fprintf(p_des, "set \"%s\" \"%s\" \"%s\"\n",
			ats::to_line_printable(r[app]).c_str(),
			ats::to_line_printable(r[key]).c_str(),
			ats::to_line_printable(r[val]).c_str());
	}

}

static void write_config_settings(const db_monitor::ConfigDB& p_db, FILE* p_des, const ats::String& p_app)
{
	const db_monitor::ResultTable& t = p_db.Table();
	const ats::String& app = ats::to_line_printable(p_app);

	for(size_t i = 0; i < t.size(); ++i)
	{
		const db_monitor::ResultRow& r = t[i];

		if(r.size() < 4)
		{
			continue;
		}

		const int key = 3;
		const int val = 1;
		fprintf(p_des, "set \"%s\" \"%s\" \"%s\"\n",
			app.c_str(),
			ats::to_line_printable(r[key]).c_str(),
			ats::to_line_printable(r[val]).c_str());
	}

}

static void write_config_settings(const db_monitor::ConfigDB& p_db, FILE* p_des, const ats::String& p_app, const ats::String& p_key)
{
	const db_monitor::ResultTable& t = p_db.Table();

	if(t.empty())
	{
		return;
	}

	const db_monitor::ResultRow& r = t[0];

	if(r.size() < 2)
	{
		return;
	}

	const int val = 1;
	const ats::String& app = ats::to_line_printable(p_app);
	const ats::String& key = ats::to_line_printable(p_key);
	fprintf(p_des, "set \"%s\" \"%s\" \"%s\"\n",
		app.c_str(),
		key.c_str(),
		ats::to_line_printable(r[val]).c_str());
}

static bool is_comment_or_blank_line(const ats::String& p_line)
{
	const char* s = p_line.c_str();

	for(size_t i = 0; i < p_line.size(); ++i)
	{
		const char c = s[i];

		switch(c)
		{
		case ' ':
		case '\t':
		case '\n':
			break;

		case '#':
			return true;

		default:
			return false;
		}

	}

	return true;
}

static int process_spec_file(db_monitor::ConfigDB& p_db, ats::StringMap& p_m, FILE* p_des)
{
	const char* fname = p_m.get("spec").c_str();
	FILE* f = fopen(fname, "r");

	if(!f)
	{
		fprintf(stderr, "ERROR: Could not open spec file \"%s\": (%d) %s\n", fname, errno, strerror(errno));
		return 1;
	}

	ats::ReadDataCache_FILE rdc(f);

	CommandBuffer cb;
	init_CommandBuffer(&cb);
	const int max_param = 32;
	const int max_buf_size = 256 * 1024;
	alloc_dynamic_buffers(&cb, max_param, max_buf_size);
	int line_no = 1;

	for(;; ++line_no)
	{
		ats::String line;
		{
			const int ret = get_file_line(line, rdc, 1);

			if(ret < 0)
			{

				if(-ENODATA != ret)
				{
					fprintf(stderr, "ERROR: spec file line %d, (%d) %s\n", line_no, -ret, strerror(-ret));
					return 1;
				}

				break;
			}

		}

		if(is_comment_or_blank_line(line))
		{
			continue;
		}

		const char* err;

		if((err = gen_arg_list(line.c_str(), int(line.size()), &cb)))
		{
			fprintf(stderr, "ERROR: spec file line %d, gen_arg_list failed (%s)\n", line_no, err);
			return 1;
		}

		if(cb.m_argc <= 0)
		{
			continue;
		}

		const char* app = cb.m_argv[0];

		if(cb.m_argc >= 2)
		{
			const char* key = cb.m_argv[1];
			const ats::String& err = p_db.Get(app, key);

			if(!err.empty())
			{
				fprintf(stderr, "ERROR5: %s\n", err.c_str());
				return 1;
			}

			write_config_settings(p_db, p_des, app, key);
		}
		else
		{
			const ats::String& err = p_db.Get(app);

			if(!err.empty())
			{
				fprintf(stderr, "ERROR6: %s\n", err.c_str());
				return 1;
			}

			write_config_settings(p_db, p_des, app);
		}

	}

	free_dynamic_buffers(&cb);
	fclose(f);
	return 0;
}

// Description:
//
//	XXX: Only "main" may call this function, and "main" must return when this function returns.
static int db_export(int argc, char* argv[])
{
	ats::StringMap m;
	ats::StringMap opt;
	m.from_args(argc - 1, argv + 1, opt);

	db_monitor::ConfigDB db(opt.get("--dbfile"), opt.get("--dbkey"));
	const ats::String& err = db.open_db_config();

	if(!err.empty())
	{
		fprintf(stderr, "ERROR7: %s\n", err.c_str());
		return 1;
	}

	FILE* f = m.has_key("des") ? fopen(m.get("des").c_str(), "r") : stdout;

	if(!f)
	{
		fprintf(stderr, "ERROR: Failed to open destination file (%d) %s\n", errno, strerror(errno));
		return 1;
	}

	fprintf(f, "# TRULink Configuration Settings\n");

	{
		ats::String sn;
		ats::String imei;
		ats::String version_no;
		ats::String version_date;
		ats::get_file_line(sn, "/mnt/nvram/rom/sn.txt", 1);
		ats::get_file_line(imei, "/tmp/config/imei", 1);
		ats::get_file_line(version_date, "/version", 2);
		ats::get_file_line(version_no, "/version", 3);
		fprintf(f, "# SN: %s\n", sn.c_str());
		fprintf(f, "# IMEI: %s\n", imei.c_str());
		fprintf(f, "# Version: %s, %s\n", version_no.c_str(), version_date.c_str());
		fprintf(f, "# Date: %s\n", ats::human_readable_date().c_str());

		if(m.has_key("info"))
		{
			fprintf(f, "# INFO: %s\n", ats::to_line_printable(m.get("info")).c_str());
		}

	}

	if(m.has_key("spec"))
	{
		const int ret = process_spec_file(db, m, f);

		if(ret)
		{
			return ret;
		}

	}
	else if (m.has_key("generic"))  // all settings except HW/unit specific values
	{
		const ats::String& err = db.Get();

		if(!err.empty())
		{
			fprintf(stderr, "ERROR8: %s\n", err.c_str());
			return 1;
		}

		write_generic_config_settings(db, f);
	}
	else
	{
		const ats::String& err = db.Get();

		if(!err.empty())
		{
			fprintf(stderr, "ERROR9: %s\n", err.c_str());
			return 1;
		}

		write_config_settings(db, f);
	}

	return 0;
}

static int parse_app_key_value_array(ATSLogger& p_log, CommandBuffer& p_cb, int p_line_no, ats::String(&p_app_key_value)[3])
{
	int i;

	for(i = 0; i < 3; ++i)
	{
		ats::String& s = p_app_key_value[i];
		const char* err;

		if((err = gen_arg_list(s.c_str(), int(s.size()), &p_cb)))
		{
			ats_logf(&p_log, "ERROR: line %d, gen_arg_list(sbuf[%d]) failed (%s)", p_line_no, i, err);
			return 1;
		}

		if(p_cb.m_argc >= 1)
		{
			s = p_cb.m_argv[0];
		}
		else
		{
			s.clear();
		}

	}

	return 0;
}

// Description:
//
//	XXX: Only "main" may call this function, and "main" must return when this function returns.
static int db_import(int argc, char* argv[])
{
	bool reboot = false;
	ats::StringMap m;
	ats::StringMap opt;
	m.from_args(argc - 1, argv + 1, opt);

	ATSLogger log;

	if(!m.has_key("syslog"))
	{
		log.set_output(stderr);
	}
	else
	{
		log.set_syslog_priority(LOG_ERR);
	}

	db_monitor::ConfigDB db(opt.get("--dbfile"), opt.get("--dbkey"));
	const ats::String& err = db.open_db_config();

	if(!err.empty())
	{
		ats_logf(&log, "ERROR10: %s", err.c_str());
		return 1;
	}

	ats::ReadDataCache_FILE rdc(stdin);

	CommandBuffer cb;
	init_CommandBuffer(&cb);
	const int max_param = 32;
	const int max_buf_size = 256 * 1024;
	alloc_dynamic_buffers(&cb, max_param, max_buf_size);
	int line_no = 1;

	for(;; ++line_no)
	{
		ats::String line;
		{
			const int ret = get_file_line(line, rdc, 1);

			if(ret < 0)
			{

				if(-ENODATA != ret)
				{
					ats_logf(&log, "ERROR: line %d, (%d) %s", line_no, -ret, strerror(-ret));
					return 1;
				}

				break;
			}

		}

		if(is_comment_or_blank_line(line))
		{
			continue;
		}

		const char* err;

		if((err = gen_arg_list(line.c_str(), int(line.size()), &cb)))
		{
			ats_logf(&log, "ERROR: line %d, gen_arg_list failed (%s)", line_no, err);
			return 1;
		}

		if(cb.m_argc < 1)
		{
			continue;
		}

		const ats::String action(cb.m_argv[0]);

		if("set" == action)
		{

			if(cb.m_argc < 3)
			{
				ats_logf(&log, "ERROR: line %d, expected <set> <app> <key> [value]", line_no);
				return 1;
			}

			ats::String sbuf[3] =
			{
				'"' + ats::String(cb.m_argv[1]) + '"',
				'"' + ats::String(cb.m_argv[2]) + '"',
				(cb.m_argc >= 4) ? ('"' + ats::String(cb.m_argv[3]) + '"') : ""
			};

			{
				const int ret = parse_app_key_value_array(log, cb, line_no, sbuf);

				if(ret)
				{
					return ret;
				}

			}

			const ats::String& app = sbuf[0];
			const ats::String& key = sbuf[1];
			const ats::String& val = sbuf[2];
			ats::String err;

			if(m.has_key("update_only"))
			{
				err = db.Update(app, key, val);
			}
			else
			{
				err = db.Set(app, key, val);
			}

			if(!err.empty())
			{
				ats_logf(&log, "ERROR: line %d, %s", line_no, err.c_str());
				return 1;
			}

		}
		else if("unset" == action)
		{

			if(cb.m_argc < 2)
			{
				ats_logf(&log, "ERROR: line %d, expected <unset> <app> [key]", line_no);
				return 1;
			}

			const ats::String app(cb.m_argv[1]);
			ats::String err;

			if(cb.m_argc >= 3)
			{
				const ats::String key(cb.m_argv[2]);
				err = db.Unset(app, key);
			}
			else
			{
				err = db.Unset(app);
			}

			if(!err.empty())
			{
				ats_logf(&log, "ERROR: line %d, %s", line_no, err.c_str());
				return 1;
			}

		}
		else if("reboot" == action)
		{
			printf("Reboot requested\n");
			reboot = true;
		}
		else
		{
			ats_logf(&log, "line %d, invalid action \"%s\"", line_no, action.c_str());
		}

	}

	free_dynamic_buffers(&cb);

	if(reboot)
	{
		fflush(stdout);
		static const int reboot_success_code = 10;
		return reboot_success_code;
	}

	return 0;
}

int main( int argc, char* argv[])
{

	if(argc >= 1)
	{

		if(ats::called_as(argv[0], "db-export"))
		{
			return db_export(argc, argv);
		}
		else if(ats::called_as(argv[0], "db-import"))
		{
			return db_import(argc, argv);
		}

	}

	std::vector<ats::String> arg;
	ats::StringMap opt;
	get_args_and_options(arg, opt, argc - 1, argv + 1);

	if(arg.size() < 1)
	{
		print_usage(argv[0]);
		return 1;
	}

	const bool csv = opt.get_bool("csv");
	const bool value_only = opt.get_bool("value_only");
	const bool brief = opt.get_bool("brief");

	const ats::String cmd(arg[0]);

	const bool has_app_name = (arg.size() >= 2);
	const bool has_key = (arg.size() >= 3);

	const ats::String app_name(has_app_name ? arg[1] : "");
	const ats::String key(has_key ? arg[2] : "");

	db_monitor::ConfigDB db(opt.get("--dbfile"), opt.get("--dbkey"));
	const ats::String& err = db.open_db_config();

	if(!err.empty())
	{
		fprintf(stderr, "ERROR1: %s\n", err.c_str());
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

		if(opt.has_key("stdin"))
		{
			const int err = read_from_file(stdin, value);

			if(err)
			{
				fprintf(stderr, "ERROR(%d,%s): An error occurred while reading stdin\n", err, strerror(err));
				error = 1;

				if(!ignore_errors)
				{
					return error;
				}

			}

		}
		else if(opt.has_key("file"))
		{
			const ats::String& fname = opt.get("file");
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

		}
		else
		{

			if(arg.size() < 4)
			{
				print_usage(argv[0]);
				return 1;
			}

			value = arg[3];
		}

		ats::String err;

		if(opt.has_key("update_only"))
		{
			err = db.Update(app_name, key, value);
		}
		else
		{
			err = db.Set(app_name, key, value);
		}

		if(!err.empty())
		{
			fprintf(stderr, "ERROR2: %s\n", err.c_str());
			return 1;
		}

		return 0;
	}
	else if("unset" == cmd)
	{
		ats::String err;

		if(has_app_name)
		{

			if(has_key)
			{
				err = db.Unset(app_name, key);
			}
			else
			{
				err = db.Unset(app_name);
			}

		}
		else
		{
			err = db.Unset();
		}

		if(!err.empty())
		{
			fprintf(stderr, "ERROR3: %s\n", err.c_str());
			return 1;
		}

		return 0;
	}
	else if("get" == cmd)
	{
		ats::String err;

		if(has_key)
		{
			err = db.Get(app_name, key);
		}
		else if(has_app_name)
		{
			err = db.Get(app_name);
		}
		else
		{
			err = db.Get();
		}

		if(!err.empty())
		{
			fprintf(stderr, "ERROR4: %s\n", err.c_str());
			return 1;
		}

	}
	else
	{
		fprintf(stderr, "ERROR: Invalid command \"%s\"\n", cmd.c_str());
		return 1;
	}

	{

		if(csv)
		{
			printf("v_App,v_Key,v_Date,v_Value,v_Previous\n");
		}

		size_t i;

		for(i = 0; i < db.Table().size(); ++i)
		{
			const db_monitor::ResultRow& row = db.Table()[i];

			if(row.size() < 3)
			{
				fprintf(stderr, "ERROR: Expected at least 3 columns not %d\n", int(row.size()));
				error = 1;
				continue;
			}

			const bool has_app_column = (db.Table().size() > 0) && (row.size() >= 5);
			const bool has_key_column = (db.Table().size() > 0) && (row.size() >= 4);

			if(csv)
			{
				printf("%s,%s,%s,%s,%s\n"
					,has_app_column ? row[4].c_str() : app_name.c_str()
					,has_key_column ? row[3].c_str() : key.c_str()
					,row[0].c_str()
					,row[1].c_str()
					,row[2].c_str()
					);
			}
			else
			{

				if(brief)
				{

					if((db.Table().size() > 0) && (row.size() >= 5))
					{
						printf("%s\t%s\t%.60s\n", row[4].c_str(), row[3].c_str(), row[1].c_str());
					}
					else if((db.Table().size() > 0) && (row.size() >= 4))
					{
						printf("%s\t%.60s\n", row[3].c_str(), row[1].c_str());
					}
					else
						printf("%.60s\n", row[1].c_str());

				}
				else if(!value_only)
				{

					if((db.Table().size() > 0) && (row.size() >= 5))
					{
						printf("%d: v_App=%s\n", int(i), row[4].c_str());
					}

					if((db.Table().size() > 0) && (row.size() >= 4))
					{
						printf("%d:   v_Key=%s\n", int(i), row[3].c_str());
					}

					printf("%d:     v_Value=%s\n", int(i), row[1].c_str());

					const time_t sec = strtol((row[0].substr(0, row[0].length() - 6)).c_str(), 0, 0);
					printf("%d:     v_Date=%s (%s)\n", int(i), row[0].c_str(), ats::human_readable_date(sec).c_str());
				}
				else
				{
					// Optimization: Only query "opt" once regardless of how many values will
					//	be returned (since command line options will always be the same for
					//	all values returned).
					static bool first = true;
					static bool no_newline;
					static bool bool_convert;

					if(first)
					{
						first = false;
						no_newline = opt.has_key("no_newline");
						bool_convert = opt.has_key("bool");
					}

					printf("%s%s", bool_convert ? (ats::get_bool(row[1]) ? "1" : "0") : row[1].c_str(), no_newline ? "" : "\n");
				}

				if(!value_only && !brief)
				{
					printf("%d:     v_Previous=%s\n", int(i), row[2].c_str());
				}

			}

		}

	}

	return error;
}
