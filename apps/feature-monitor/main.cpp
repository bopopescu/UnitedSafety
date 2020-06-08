#include <iostream>

#include <stdio.h>
#include <syslog.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>

#include "ConfigDB.h"
#include "ats-common.h"
#include "ats-string.h"
#include "socket_interface.h"
#include "command_line_parser.h"

/*

Config Database:
   App       Key     Value
   Feature   f1      1/0

Feature Table:
   Feature   appid
   f1        1,3,4,7

      create table t_feature(v_name varchar primary key, v_appid text);

App Table:
   Name     appid    depend
   App      1        3,4,7

      create table t_app(v_name varchar primary key, v_appid int unique, v_depend text);

Algorithm:
   1. get feature list
   2. create list of all apps required to support all features
   3. create application dependency list
   4. run all applications in the dependency list
*/

class MyData : public ats::CommonData
{
public:
	// Description: Features that are ON or that should be turned ON.
	const ats::StringMap& m_on_list;

	// Description: Features that are available but are currently OFF.
	const ats::StringMap& m_off_list;

	// Description: Features that must be turned OFF (they are currently active and must be turned OFF).
	const ats::StringMap& m_disable_list;

	MyData(const ats::StringMap& p_on_list, const ats::StringMap& p_off_list, const ats::StringMap& p_disable_list)
		: m_on_list(p_on_list), m_off_list(p_off_list), m_disable_list(p_disable_list)
	{
	}
};

static const ats::String g_app_name("feature-monitor");
ats::String g_feature_db;
ats::String g_feature_db_backup_dir;

static int to_backup_number(const std::stringstream& p_s)
{
	return strtol(p_s.str().c_str(), 0, 10);
}

static ats::String to_backup_str_number(int p_num)
{
	char buf[16];
	snprintf(buf, sizeof(buf) - 1, "%08d", p_num);
	buf[sizeof(buf) - 1] = '\0';
	return ats::String(buf);
}

static void remove_backup_file(int p_num)
{
	ats::system("rm -f '" + g_feature_db_backup_dir + "/db." + to_backup_str_number(p_num) + "'");
}

static void remove_first_backup_file()
{
	std::stringstream o;
	ats::system("cd '" + g_feature_db_backup_dir + "';ls -1|sort|head -n1|sed 's/^.*\\.//'", &o);
	remove_backup_file(to_backup_number(o));
}

static void remove_last_backup_file()
{
	std::stringstream o;
	ats::system("cd '" + g_feature_db_backup_dir + "';ls -1|sort|tail -n1|sed 's/^.*\\.//'", &o);
	remove_backup_file(to_backup_number(o));
}

static int count_backup_files()
{
	{
		std::stringstream o;
		const int ret = ats::system("cd '" + g_feature_db_backup_dir + "' 2>&1 && ls -1a|grep -c .", &o);

		if(ret)
		{
			fprintf(stderr, "[0x%0X] could not count backup files \"%s\" from \"%s\"\noutput: %s\n", ret, g_feature_db.c_str(), g_feature_db_backup_dir.c_str(), o.str().c_str());
			return -1;
		}

	}

	std::stringstream o;
	ats::system("cd '" + g_feature_db_backup_dir + "';ls -1|grep -c .", &o);
	return to_backup_number(o);
}

static void backup_feature_db()
{
	ats::system("mkdir -p '" + g_feature_db_backup_dir + "'");

	const int count = count_backup_files();

	if(count >= 10)
	{
		remove_first_backup_file();
	}

	std::stringstream o;
	ats::system("cd '" + g_feature_db_backup_dir + "';ls -1|sort|tail -n1|sed 's/^.*\\.//'", &o);
	const int last_num = to_backup_number(o);

	ats::system("cp '" + g_feature_db + "' '" + g_feature_db_backup_dir + "/db." + to_backup_str_number(last_num + 1) + "'");
}

typedef std::vector<ats::String> StrList;

class DB : public db_monitor::ConfigDB
{
public:

};

static int undo_feature_db()
{
	{
		DB db;
		{
			const ats::String& emsg = db.open_db(g_app_name, g_feature_db);

			if(!emsg.empty())
			{
				fprintf(stderr, "%s,%d: could not open feature database (%s,%s)\n", __FILE__, __LINE__, g_app_name.c_str(), g_feature_db.c_str());
				return 1;
			}

		}

		const ats::String& emsg = db.close_db(g_app_name);

		if(!emsg.empty())
		{
			fprintf(stderr, "%s,%d: Error closing feature datbase (%s,%s): %s\n", __FILE__, __LINE__, g_app_name.c_str(), g_feature_db.c_str(), emsg.c_str());
			return 1;
		}

	}

	std::stringstream o;

	if(ats::system("cd '" + g_feature_db_backup_dir + "';ls -1|sort|tail -n1|sed 's/^.*\\.//'", &o))
	{
		fprintf(stderr, "Could not get last backup database, cannot undo last changes to \"%s\"\n", g_feature_db.c_str());
		return 1;
	}

	const int last_num = to_backup_number(o);

	if(!last_num)
	{
		fprintf(stderr, "No backups to restore from\n");
		return 1;
	}

	if(ats::system("cp '" + g_feature_db_backup_dir + "/db." + to_backup_str_number(last_num) + "' '" + g_feature_db + ".tmp';mv '" + g_feature_db + ".tmp' '" + g_feature_db + "'"))
	{
		fprintf(stderr, "Could not restore \"%s\" from backup #%d\n", g_feature_db.c_str(), last_num);
		return 1;
	}

	remove_last_backup_file();
	return 0;
}

static void append_dependency(ats::StringMap& p_m, const ats::String& p_app, const ats::String& p_d)
{
	ats::StringMap::iterator i = p_m.find(p_app);

	if(i != p_m.end())
	{
		ats::StringList l;
		ats::split(l, i->second, ",");

		ats::StringMap m;

		m.set(p_d, "");

		for(size_t j = 0; j < l.size(); ++j)
		{
			m.set(l[j], "");
		}

		l.clear();

		ats::String s;
		{
			ats::StringMap::iterator i = m.begin();

			while(i != m.end())
			{
				s += ((s.empty()) ? "" : ",") + i->first;
				++i;
			}

		}
		i->second = s;

		++i;
	}
	else
	{
		p_m.set(p_app, p_d);
	}

}

class AppList : public std::map<const ats::String, ats::StringList>
{
public:

	void from_app_list(const ats::StringMap& p_m)
	{
		clear();

		ats::StringMap::const_iterator i = p_m.begin();

		while(i != p_m.end())
		{
			ats::StringList l;
			ats::split(l, i->second, ",");
			insert(std::pair<const ats::String, ats::StringList>(i->first, l));

			++i;
		}

	}

	void to_app_list(ats::StringMap& p_m) const
	{
		p_m.clear();

		const_iterator i = begin();

		while(i != end())
		{
			const ats::StringList l = i->second;
			const ats::String& app = i->first;
			++i;

			ats::String s;
			size_t i;

			for(i = 0; i < l.size(); ++i)
			{
				s += ((s.empty()) ? "" : ",") + l[i];
			}

			p_m.set(app, s);
		}

	}

	void reverse_meaning()
	{
		ats::StringMap m;
		const_iterator i = begin();

		while(i != end())
		{

			if(!(m.has_key(i->first)))
			{
				m.set(i->first, "");
			}

			ats::StringList::const_iterator j = (i->second).begin();

			while(j != (i->second).end())
			{
				append_dependency(m, *j, i->first);
				++j;
			}

			++i;
		}

		from_app_list(m);
	}

	void print(FILE *p_f) const
	{
		const_iterator i = begin();

		while(i != end())
		{
			fprintf(p_f, "%s: ", (i->first).c_str());

			size_t j;

			for(j = 0; j < (i->second).size(); ++j)
			{
				fprintf(p_f, "%s%s", (j ? "," : ""), ((i->second)[j]).c_str());
			}

			fprintf(p_f, "\n");

			++i;
		}

	}

	void remove_edge(const ats::String& p_from, const ats::String& p_to)
	{
		iterator i = find(p_from);

		if(i != end())
		{
			ats::StringList& l = i->second;

			ats::StringList::iterator i = l.begin();

			while(i != l.end())
			{

				if((*i) == p_to)
				{
					l.erase(i);
					break;
				}

				++i;
			}

		}

	}

	void remove_app(const ats::String& p_app)
	{
		iterator i = find(p_app);

		if(i != end())
		{
			erase(i);
		}

	}

};

static void find_apps_with_no_incoming_edges(ats::StringMap& p_S, const ats::StringMap& p_app_list)
{
	ats::StringMap& S = p_S;

	ats::StringMap has_incoming_edge;
	ats::StringMap::const_iterator i = p_app_list.begin();

	while(i != p_app_list.end())
	{
		S.set(i->first, "");
		++i;
	}

	i = p_app_list.begin();

	// Create dependency list
	while(i != p_app_list.end())
	{
		const ats::String& app_id = i->first;

		const ats::String& list = i->second;
		++i;

		ats::StringList l;
		ats::split(l, list, ",");

		size_t i;

		for(i = 0; i < l.size(); ++i)
		{

			if(!l[i].empty())
			{
				S.unset(app_id);

				ats::String s(has_incoming_edge.get(app_id));
				has_incoming_edge.set(app_id, s + ((!s.empty()) ? "," : "") + l[i]);
			}
		}

	}

}


/*
Algorithm (http://en.wikipedia.org/wiki/Topological_sorting):
   L ← Empty list that will contain the sorted elements
   S ← Set of all nodes with no incoming edges
   while S is non-empty do
       remove a node n from S
       insert n into L
       for each node m with an edge e from n to m do
           remove edge e from the graph
           if m has no other incoming edges then
               insert m into S
   if graph has edges then
       return error (graph has at least one cycle)
   else 
       return L (a topologically sorted order)
*/
static void create_dependency_list(const ats::StringMap& p_app_list, ats::StringList& p_dep_list)
{
	ats::StringList& L = p_dep_list;
	p_dep_list.clear();

	ats::StringMap S;

	AppList al;
	al.from_app_list(p_app_list);
	al.reverse_meaning();

	ats::StringMap app_list;
	al.to_app_list(app_list);
	find_apps_with_no_incoming_edges(S, app_list);

	{
		ats::StringMap::iterator i = S.begin();

		while(i != S.end())
		{
			al.remove_app(i->first);
			++i;
		}
	}

	while(!S.empty())
	{
		ats::String n((S.begin())->first);
		S.erase(S.begin());

		L.push_back(n);

		AppList::iterator i = al.begin();

		while(i != al.end())
		{
			const ats::String& m = (i->first);
			al.remove_edge(m, n);
			++i;
		}

		al.to_app_list(app_list);
		find_apps_with_no_incoming_edges(S, app_list);

		{
			ats::StringMap::iterator i = S.begin();

			while(i != S.end())
			{
				al.remove_app(i->first);
				++i;
			}
		}

	}

}

static void create_list_of_all_apps_required_to_support_all_features(
	const ats::StringMap& p_feature,
	ats::StringMap& p_app_list)
{
	p_app_list.clear();
	DB db;
	{
		const ats::String& emsg = db.open_db(g_app_name, g_feature_db);

		if(!emsg.empty())
		{
			fprintf(stderr, "%s,%d: Failed to open feature database. %s\n", __FILE__, __LINE__, emsg.c_str());
			return;
		}

	}

	{
		const ats::String& emsg = db.query(g_app_name, "select * from t_feature");

		if(!emsg.empty())
		{
			fprintf(stderr, "%s,%d: Failed to query features. %s\n", __FILE__, __LINE__, emsg.c_str());
			return;
		}

	}

	ats::StringMap required;
	{
		db_monitor::ResultTable& t = db.Table();
		size_t i;

		for(i = 0; i < t.size(); ++i)
		{
			db_monitor::ResultRow& r = t[i];

			if(r.size() < 2)
			{
				continue;
			}

			if(!p_feature.has_key(r[0]))
			{
				continue;
			}

			ats::StringList s;
			ats::split(s, r[1], ",");

			size_t i;

			for(i = 0; i < s.size(); ++i)
			{
				required.set(s[i], "1");
			}

		}
	}

	ats::StringMap& app_list = p_app_list;

	{
		const ats::String& emsg = db.query(g_app_name, "select * from t_app");

		if(!emsg.empty())
		{
			fprintf(stderr, "%s,%d: Failed to query applications. %s\n", __FILE__, __LINE__, emsg.c_str());
			return;
		}

		db_monitor::ResultTable& t = db.Table();
		size_t i;

		for(i = 0; i < t.size(); ++i)
		{
			db_monitor::ResultRow& r = t[i];

			if(r.size() < 4)
			{
				continue;
			}

			if(required.has_key(r[1]))
			{
				app_list.set(r[1], r[2]);
			}

		}

	}

}

// Description: Returns a list of shell commands for executing/running features.
//
//	"p_feature_db" must be an open database to "feature.db".
//
//	String map formats:
//		"p_appid" is a [v_name ]--->[v_appid] mapping
//		"p_cmd" is a   [v_appid]--->[v_exec ] mapping
//		"p_unset" is a [v_appid]--->[v_unset] mapping
//		"p_fboot" is a [v_appid]--->[v_fboot] mapping
//
//	Table t_app definition as of SVN 5343:
//		CREATE TABLE t_app(v_name varchar primary key, v_appid int unique, v_depend varchar, v_exec varchar, "v_unset" VARCHAR DEFAULT null, "v_fboot" VARCHAR DEFAULT null);
//
static void get_feature_execlist(
	DB& p_feature_db,
	ats::StringMap& p_appid,
	ats::StringMap& p_cmd,
	ats::StringMap& p_unset,
	ats::StringMap& p_fboot)
{
	DB& db = p_feature_db;

	{
		const ats::String& emsg = db.query(g_app_name, "select * from t_app");

		if(!emsg.empty())
		{
			fprintf(stderr, "%s,%d: Failed to query features. %s\n", __FILE__, __LINE__, emsg.c_str());
			return;
		}

	}

	{
		const db_monitor::ResultTable& t = db.Table();
		size_t i;

		for(i = 0; i < t.size(); ++i)
		{
			const db_monitor::ResultRow& r = t[i];

			if(r.size() < 6)
			{
				continue;
			}

			p_appid.set(r[0], r[1]);
			p_cmd.set(r[1], r[3]);
			p_unset.set(r[1], r[4]);
			p_fboot.set(r[1], r[5]);
		}
	}
}

// Description: Returns a list of all enabled features (in "p_list") and a list of all
//	"known" disabled features (in "p_off_list").
//
//	A feature that is known to the caller but which does not appear in either list is a disabled
//	"unknown" feature.
static void get_feature_list(DB& p_db, ats::StringMap& p_list, ats::StringMap& p_off_list, ats::StringMap& p_disable_list)
{
	p_list.clear();
	p_off_list.clear();
	p_disable_list.clear();

	DB& db = p_db;
	const ats::String& emsg = db.Get("feature");

	if(!emsg.empty())
	{
		fprintf(stderr, "%s,%d: could not get feature list: %s\n", __FILE__, __LINE__, emsg.c_str());
		return;
	}

	db_monitor::ResultTable& t = db.Table();

	size_t i;

	for(i = 0; i < t.size(); ++i)
	{
		db_monitor::ResultRow& r = t[i];

		size_t i;

		bool blank = true;
		bool on = false;

		for(i = 0; i < r.size(); ++i)
		{
			if(1 == i)
			{

				if("" != r[i])
				{
					blank = false;
					on = (strtol(r[i].c_str(), 0, 0) != 0);
				}

			}

			if(on)
			{

				if(3 == i)
				{
					p_list.set(r[i], "");
				}

			}
			else
			{

				if(3 == i)
				{
					p_off_list.set(r[i], "");

					if(!blank)
					{
						p_disable_list.set(r[i], "");
					}

				}

			}

		}

	}

}

// Description: Creates a dependency list based on the applications that will be executed.
//	The dependency list will be stored in "p_dep_list".
//
//	"p_feature_db" must be an opened database to "feature.db".
static void create_app_run_script(DB& p_feature_db, const ats::StringList& p_dep_list)
{
	DB& db = p_feature_db;

	{
		const ats::String& emsg = db.query(g_app_name, "select * from t_app");

		if(!emsg.empty())
		{
			fprintf(stderr, "%s,%d: Failed to query features. %s\n", __FILE__, __LINE__, emsg.c_str());
			return;
		}

	}

	ats::StringMap app_name;
	ats::StringMap required;
	{
		db_monitor::ResultTable& t = db.Table();
		size_t i;

		for(i = 0; i < t.size(); ++i)
		{
			db_monitor::ResultRow& r = t[i];

			if(r.size() < 2)
			{
				continue;
			}

			app_name.set(r[1], r[0]);
		}
	}

}

static void run_apps(
	const ats::StringList& p_dep,
	const ats::StringMap& p_cmd,
	const ats::StringMap& p_fboot_cmd,
	ats::StringMap& p_pid)
{

	if(p_dep.empty())
	{
		return;
	}

	const bool first_boot = (NULL != getenv("REDSTONE_FIRST_BOOT"));

	const ats::StringMap& map = first_boot ? p_fboot_cmd : p_cmd;

	ats::StringList::const_iterator i = p_dep.end();

	do
	{
		--i;

		const ats::String& app_id = *i;

		ats::StringMap::const_iterator j = map.find(app_id);

		if(j == map.end())
		{
			// ATS FIXME: Signal an error.
			continue;
		}

		const ats::String& cmd = "exec " + j->second;

		const int pid = fork();

		if(!pid)
		{
			const char* command = cmd.c_str();
			execl("/bin/sh", "/bin/sh", "-c", command, NULL);
			syslog(LOG_ERR, "%s,%d: Failed to run command \"%s\": (%d) %s", __FILE__, __LINE__, command, errno, strerror(errno));
			exit(1);
		}
		else if(pid)
		{
			p_pid.set(ats::toStr(pid), cmd);
		}
		else
		{
			syslog(LOG_ERR, "%s,%d: Fork failed trying to run \"%s\": (%d) %s", __FILE__, __LINE__, cmd.c_str(), errno, strerror(errno));
		}

	} while(i != p_dep.begin());

}

class ForEachFeatureFunction
{
public:
	ForEachFeatureFunction(){}
	virtual~ ForEachFeatureFunction(){}

	virtual void process(const ats::String& p_feature_name, const db_monitor::ResultRow& p_r) = 0;
};

class IncrementFeatureCount : public ForEachFeatureFunction
{
public:

	IncrementFeatureCount()
	{
		m_count = 0;
	}

	virtual~ IncrementFeatureCount(){}

	virtual void process(const ats::String& p_feature_name, const db_monitor::ResultRow& p_r)
	{
		m_count++;
	}

	size_t m_count;
};

class ListFeature : public ForEachFeatureFunction
{
public:
	bool m_color;

	ListFeature()
	{
		m_color = true;
	}

	virtual~ ListFeature(){}

	virtual void process(const ats::String& p_feature_name, const db_monitor::ResultRow& p_r)
	{
		#define RED_ON "\x1b[1;31m"
		#define GREEN_ON "\x1b[1;32m"
		#define RESET_COLOR "\x1b[0m"

		const bool on = ats::get_bool(p_r[1]);

		if(m_color)
		{
			printf("\t%s: %s\n", p_feature_name.c_str(), on ? GREEN_ON "ON" RESET_COLOR : RED_ON "OFF" RESET_COLOR);
		}
		else
		{
			printf("\t%s: %s\n", p_feature_name.c_str(), on ? "ON" : "OFF");
		}

		#undef RED_ON
		#undef GREEN_ON
		#undef RESET_COLOR
	}

};

static void for_each_feature(const db_monitor::ResultTable& p_t, const ats::StringMap& p_filter, ForEachFeatureFunction& p_fn)
{
	size_t i;

	for(i = 0; i < p_t.size(); ++i)
	{
		const db_monitor::ResultRow& r = p_t[i];

		if(r.size() < 3)
		{
			continue;
		}

		const ats::String& feature_name = r[3];

		if(!p_filter.empty() && !p_filter.has_key(feature_name))
		{
			continue;
		}

		p_fn.process(feature_name, r);
	}

}

static int show_feature_state(DB& p_config_db, const ats::StringMap& p_filter, const ats::StringMap& p_option)
{
	const ats::String& emsg = p_config_db.Get("feature");

	if(!emsg.empty())
	{
		fprintf(stderr, "%s\n", emsg.c_str());
		return 1;
	}

	db_monitor::ResultTable& t = p_config_db.Table();

	size_t feature_count = 0;

	if(p_filter.empty())
	{
		feature_count = t.size();
	}
	else
	{
		IncrementFeatureCount fn;
		for_each_feature(t, p_filter, fn);
		feature_count = fn.m_count;
	}

	printf("Total Features Listed: %zu\n", feature_count);

	ListFeature fn;

	fn.m_color = p_option.has_key("s") ? false : true;

	for_each_feature(t, p_filter, fn);

	return 0;
}

static ats::String get_options_and_args(int argc, char* argv[], const ats::StringMap& p_takes_param, ats::StringMap& p_option, ats::StringList& p_arg)
{
	int i;

	for(i = 0; i < argc; ++i)
	{
		const char* arg = argv[i];

		if('-' == arg[0])
		{

			if('-' == arg[1])
			{

				if('\0' == arg[2])
				{

					for(++i; i < argc; ++i)
					{
						p_arg.push_back(argv[i]);
					}

					return ats::String();
				}

				p_option.set(arg + 2, ats::String());
			}
			else
			{

				while(*arg)
				{
					ats::String c;
					c += *(arg++);

					if(p_takes_param.has_key(c))
					{
						++i;

						if(i >= argc)
						{
							return "expected parameter for argument \"" + ats::String(arg) + "\"";
						}

						p_option.set(c, argv[i]);

					}
					else
					{
						p_option.set(c, ats::String());
					}

				}

				continue;
			}

		}

		p_arg.push_back(arg);
	}

	return ats::String();
}

static void schedule_configuration_on_next_boot()
{
	ats::system("mkdir -p /etc/redstone/first-boot;ln -s /usr/bin/feature-monitor /etc/redstone/first-boot/;sync");
}

static int main_feature(DB& p_config_db, int argc, char* argv[])
{
	ats::StringMap takes_param;
	ats::StringMap option;
	ats::StringList arg;

	const ats::String& err = get_options_and_args(argc, argv, takes_param, option, arg);

	if(!err.empty())
	{
		fprintf(stderr, "%s\n", err.c_str());
		return 1;
	}

	if(arg.size() < 2)
	{
		return show_feature_state(p_config_db, ats::StringMap(), option);
	}

	const ats::String& cmd = ats::tolower(arg[1]);

	if("set" == cmd || "enable" == cmd)
	{

		if(argc < 3)
		{
			fprintf(stderr, "no feature name was given to %s\n", cmd.c_str());
			return 1;
		}

		int i;

		for(i = 2; i < argc; ++i)
		{
			const ats::String name(argv[i]);

			if(!p_config_db.Exists("feature", name))
			{
				fprintf(stderr, "feature \"%s\" does not exist\n", name.c_str());
				return 1;
			}

			const ats::String& setting = p_config_db.GetValue("feature", name);
			const bool on = (strtol(setting.c_str(), 0, 0) != 0);

			if(!on)
			{
				p_config_db.Set("feature", name, "1");
				schedule_configuration_on_next_boot();
			}

		}

	}
	else if("unset" == cmd || "disable" == cmd)
	{

		if(argc < 3)
		{
			fprintf(stderr, "no feature name was given to %s\n", cmd.c_str());
			return 1;
		}

		int i;

		for(i = 2; i < argc; ++i)
		{
			const ats::String name(argv[i]);

			if(!p_config_db.Exists("feature", name))
			{
				fprintf(stderr, "feature \"%s\" does not exist\n", name.c_str());
				return 1;
			}

			const ats::String& setting = p_config_db.GetValue("feature", name);
			const bool on = (strtol(setting.c_str(), 0, 0) != 0);

			if(on)
			{
				p_config_db.Set("feature", name, "0");
				schedule_configuration_on_next_boot();
			}

		}

	}
	else if("get" == cmd)
	{
		ats::StringMap m;

		for(int i = 2; i < argc; ++i)
		{
			m.set(argv[i], "");
		}

		return show_feature_state(p_config_db, m, option);
	}
	else if("add" == cmd || "new" == cmd)
	{

		if(argc < 3)
		{
			fprintf(stderr, "no feature given to add\n");
			return 1;
		}

		ats::String appid;
		int i;

		for(i = 3; i < argc; ++i)
		{
			appid += (appid.empty() ? "" : ",") + ats::toStr(strtol(argv[i], 0, 0));
		}

		DB db;
		{
			const ats::String& emsg = db.open_db(g_app_name, g_feature_db);

			if(!emsg.empty())
			{
				fprintf(stderr, "%s,%d: could not open feature database (%s,%s)\n", __FILE__, __LINE__, g_app_name.c_str(), g_feature_db.c_str());
				return 1;
			}

		}

		backup_feature_db();

		const ats::String name(argv[2]);

		{
			const ats::String& emsg = db.query(g_app_name, "insert into t_feature (v_name,v_appid) values('" + name + "','" + appid + "')");

			if(!emsg.empty())
			{
				fprintf(stderr, "%s,%d: Failed to insert new feature ('%s','%s') into database: %s\n", __FILE__, __LINE__, name.c_str(), appid.c_str(), emsg.c_str());
				return 1;
			}

		}

		printf("Feature \"%s\" with appid \"%s\" has been added to the database\n", name.c_str(), appid.c_str());
	}
	else if("remove" == cmd || "delete" == cmd || "del" == cmd || "rm" == cmd)
	{

		if(argc < 3)
		{
			fprintf(stderr, "no feature given to remove\n");
			return 1;
		}

		DB db;
		{
			const ats::String& emsg = db.open_db(g_app_name, g_feature_db);

			if(!emsg.empty())
			{
				fprintf(stderr, "%s,%d: could not open feature database (%s,%s)\n", __FILE__, __LINE__, g_app_name.c_str(), g_feature_db.c_str());
				return 1;
			}

		}

		backup_feature_db();

		int ret_val = 0;
		int i;

		for(i = 2; i < argc; ++i)
		{
			const ats::String name(argv[i]);

			const ats::String& emsg = db.query(g_app_name, "delete from t_feature where v_name='" + name + "'");

			if(!emsg.empty())
			{
				fprintf(stderr, "there was an error removing feature \"%s\"\n", name.c_str());
				ret_val = 1;
			}
			else
			{
				printf("removed feature \"%s\"", name.c_str());
			}

		}

		return ret_val;
	}
	else if("bootcfg" == cmd)
	{
		schedule_configuration_on_next_boot();
	}
	else if("help" == cmd || "h" == cmd || "?" == cmd)
	{
		printf(
			"Usage: %s [command] [arg 1] ... [arg N]\n"
			"\n"
			"Commands:\n"
			"\tset/enable <feature name 1> [feature name 2] ... [feature name N]\n"
			"\t   - Sets the named feature(s)\n"
			"\n"
			"\tunset/disable <feature name 1> [feature name 2] ... [feature name N]\n"
			"\t   - Turns off the named feature(s)\n"
			"\n"
			"\tget [feature name 1] ... [feature name N]\n"
			"\t   - Gets the status/value of the named feature(s), or all features\n"
			"\t     if no specific feature is given\n"
			"\t   - Options:\n"
			"\t       -s\n"
			"\t          Output is \"simple\", containing no special terminal codes (such as\n"
			"\t          for colour output)\n"
			"\n"
			"\tadd/new <feature name> [app ID 1] ... [app ID N]\n"
			"\t   - Adds the named feature\n"
			"\n"
			"\tremove/rm/delete/del <feature name>\n"
			"\t   - Removes the named feature\n"
			"\n"
			"\tbootcfg\n"
			"\t   - Schedule feature configuration at next boot\n"
			"\n"
			"\thelp/h/?\n"
			"\t   - Displays this help message\n"
			, argv[0]
			);
	}
	else if("undo" == cmd)
	{
		return undo_feature_db();
	}
	else
	{
		fprintf(stderr, "Unsupported feature command \"%s\"\n", cmd.c_str());
		return 1;
	}

	return 0;
}

class ForEachAppFunction
{
public:
	ForEachAppFunction(){}
	virtual~ ForEachAppFunction(){}

	virtual void process(const ats::String& p_app_name, const db_monitor::ResultRow& p_r) = 0;
};

class IncrementAppCount : public ForEachAppFunction
{
public:

	IncrementAppCount()
	{
		m_count = 0;
	}

	virtual~ IncrementAppCount(){}

	virtual void process(const ats::String& p_app_name, const db_monitor::ResultRow& p_r)
	{
		m_count++;
	}

	size_t m_count;
};

class ListApp : public ForEachAppFunction
{
public:

	virtual~ ListApp(){}

	virtual void process(const ats::String& p_app_name, const db_monitor::ResultRow& p_r)
	{
		printf("\t%s:\n", p_app_name.c_str());

		if(p_r.size() >= 2)
		{
			printf("\t\tid: %s\n", p_r[1].c_str());
		}

		if(p_r.size() >= 3)
		{
			printf("\t\tdepend: %s\n", p_r[2].c_str());
		}

		if(p_r.size() >= 4)
		{
			printf("\t\texec: %s\n", p_r[3].c_str());
		}

	}

};

static void for_each_app(const db_monitor::ResultTable& p_t, const ats::StringMap& p_filter, ForEachAppFunction& p_fn)
{
	size_t i;

	for(i = 0; i < p_t.size(); ++i)
	{
		const db_monitor::ResultRow& r = p_t[i];

		if(r.size() < 1)
		{
			continue;
		}

		const ats::String& app_name = r[0];

		if(!p_filter.empty() && !p_filter.has_key(app_name))
		{
			continue;
		}

		p_fn.process(app_name, r);
	}

}

static int show_app_state(DB& p_config_db, const ats::StringMap& p_filter)
{
	DB db;
	{
		const ats::String& emsg = db.open_db(g_app_name, g_feature_db);

		if(!emsg.empty())
		{
			fprintf(stderr, "%s,%d: could not open feature database (%s,%s)\n", __FILE__, __LINE__, g_app_name.c_str(), g_feature_db.c_str());
			return 1;
		}

	}

	const ats::String& emsg = db.query(g_app_name, "select * from t_app");

	if(!emsg.empty())
	{
		fprintf(stderr, "Failed to get app list: %s\n", emsg.c_str());
		return 1;
	}

	db_monitor::ResultTable& t = db.Table();

	size_t app_count = 0;

	if(p_filter.empty())
	{
		app_count = t.size();
	}
	else
	{
		IncrementAppCount fn;
		for_each_app(t, p_filter, fn);
		app_count = fn.m_count;
	}

	printf("Total Apps Listed: %zu\n", app_count);

	ListApp fn;
	for_each_app(t, p_filter, fn);

	return 0;
}

static int main_feature_app(DB& p_config_db, int argc, char* argv[])
{

	if(argc < 2)
	{
		return show_app_state(p_config_db, ats::StringMap());
	}

	const ats::String& cmd = ats::tolower(argv[1]);

	if("get" == cmd)
	{
		ats::StringMap m;

		for(int i = 2; i < argc; ++i)
		{
			m.set(argv[i], "");
		}

		return show_app_state(p_config_db, m);
	}
	else if("add" == cmd || "new" == cmd)
	{

		if(argc < 3)
		{
			fprintf(stderr, "no application given to add\n");
			return 1;
		}

		ats::StringMap m;
		m.from_args(argc - 3, argv + 3);

		{
			ats::StringMap valid_properties;
			valid_properties.set("appid", "");
			valid_properties.set("depid", "");
			valid_properties.set("exec", "");

			ats::StringMap::const_iterator i = m.begin();

			while(i != m.end())
			{
				const ats::String& key = i->first;
				++i;

				if(!valid_properties.has_key(key))
				{
					fprintf(stderr, "invalid application property \"%s\"\n", key.c_str());
					return 1;
				}

			}

		}

		DB db;
		{
			const ats::String& emsg = db.open_db(g_app_name, g_feature_db);

			if(!emsg.empty())
			{
				fprintf(stderr, "%s,%d: could not open application database (%s,%s)\n", __FILE__, __LINE__, g_app_name.c_str(), g_feature_db.c_str());
				return 1;
			}

		}

		backup_feature_db();

		const ats::String name(argv[2]);

		{
			const ats::String& emsg = db.query(g_app_name, ats::String("insert into t_app (v_name,v_appid,v_depend,v_exec) values(")
				+ "'" + name + "'"
				+ "," + m.get("appid")
				+ ",'" + m.get("depid") + "'"
				+ ",'" + m.get("exec") + "'"
				+ ")"
				);

			if(!emsg.empty())
			{
				fprintf(stderr, "%s,%d: Failed to insert new feature ('%s') into database: %s\n", __FILE__, __LINE__, name.c_str(), emsg.c_str());
				return 1;
			}

		}

		printf("Application \"%s\" has been added to the database\n", name.c_str());
	}
	else if("remove" == cmd || "delete" == cmd || "del" == cmd || "rm" == cmd)
	{

		if(argc < 3)
		{
			fprintf(stderr, "no application given to remove\n");
			return 1;
		}

		DB db;
		{
			const ats::String& emsg = db.open_db(g_app_name, g_feature_db);

			if(!emsg.empty())
			{
				fprintf(stderr, "%s,%d: could not open application database (%s,%s)\n", __FILE__, __LINE__, g_app_name.c_str(), g_feature_db.c_str());
				return 1;
			}

		}

		backup_feature_db();

		int ret_val = 0;
		int i;

		for(i = 2; i < argc; ++i)
		{
			const ats::String name(argv[i]);

			const ats::String& emsg = db.query(g_app_name, "delete from t_app where v_name='" + name + "'");

			if(!emsg.empty())
			{
				fprintf(stderr, "there was an error removing application \"%s\"\n", name.c_str());
				ret_val = 1;
			}
			else
			{
				printf("removed application \"%s\"", name.c_str());
			}

		}

		return ret_val;
	}
	else if("help" == cmd || "h" == cmd || "?" == cmd)
	{
		printf(
			"Usage: %s [command] [arg 1] ... [arg N]\n"
			"\n"
			"Commands:\n"
			"\tget [options] [app name 1] ... [app name N]\n"
			"\t   - Gets the status/value of the named application(s), or all applications\n"
			"\t     if no specific application is given\n"
			"\n"
			"\tadd/new <app name> <appid=#> [depid=#[,#,#...,#]] [exec=\"shell command\"]\n"
			"\t   - Adds the named application\n"
			"\n"
			"\tremove/rm/delete/del <app name 1> [app name 2] ... [app name N]\n"
			"\t   - Removes the named application\n"
			"\n"
			"\thelp/h/?\n"
			"\t   - Displays this help message\n"
			, argv[0]
			);
	}
	else if("undo" == cmd)
	{
		return undo_feature_db();
	}
	else
	{
		fprintf(stderr, "Unsupported application command \"%s\"\n", cmd.c_str());
		return 1;
	}

	return 0;
}

static void* feature_server(void* p)
{
	ClientData* cd = (ClientData*)p;
	MyData& md = *((MyData*)(cd->m_data->m_hook));

	bool command_too_long = false;
	ats::String cmdline;

	const size_t max_command_length = 1024;

	ClientDataCache cache;
	init_ClientDataCache(&cache);

	const int fd = cd->m_sockfd;

	for(;;)
	{
		char ebuf[1024];
		const int c = client_getc_cached(cd, ebuf, sizeof(ebuf), &cache);

		if(c < 0)
		{
			if(c != -ENODATA) syslog(LOG_ERR, "%s,%d:ERROR: %s", __FILE__, __LINE__, ebuf);
			break;
		}

		if((c != '\r') && (c != '\n'))
		{
			if(cmdline.length() >= max_command_length) command_too_long = true;
			else cmdline += c;

			continue;
		}

		if(command_too_long)
		{
			cmdline.clear();
			command_too_long = false;
			send_cmd(fd, MSG_NOSIGNAL, "error: command is too long\n\r");
			continue;
		}

		CommandBuffer cb;
		init_CommandBuffer(&cb);
		const char* err;

		if((err = gen_arg_list(cmdline.c_str(), cmdline.length(), &cb)))
		{
			send_cmd(fd, MSG_NOSIGNAL, "error: gen_arg_list failed: %s\n\r", err);

		}
		else if(cb.m_argc > 0)
		{
			const ats::String cmd(cb.m_argv[0]);

			if("feature" == cmd)
			{

				if(cb.m_argc < 2)
				{
					send_cmd(fd, MSG_NOSIGNAL, "%s: ok\n", cmd.c_str());
					ats::StringMap::const_iterator i = md.m_on_list.begin();

					while(i != md.m_on_list.end())
					{
						send_cmd(fd, MSG_NOSIGNAL, "%s\"%s\"", (md.m_on_list.begin() == i) ? "" : ",", i->first.c_str());
						++i;
					}

					send_cmd(fd, MSG_NOSIGNAL, "\n\r");
				}
				else
				{
					const ats::String feature(cb.m_argv[1]);
					send_cmd(fd, MSG_NOSIGNAL, "%s,%s: ok\n%s\n\r", cmd.c_str(), feature.c_str(), md.m_on_list.has_key(feature) ? "on" : "off");
				}

			}
			else if("features" == cmd)
			{
				bool is_first = true;
				send_cmd(fd, MSG_NOSIGNAL, "%s: ok\n", cmd.c_str());
				ats::StringMap::const_iterator i = md.m_on_list.begin();

				while(i != md.m_on_list.end())
				{
					send_cmd(fd, MSG_NOSIGNAL, "%s\"%s\"", is_first ? "" : "\n", i->first.c_str());
					is_first = false;
					++i;
				}

				if(!(md.m_off_list.empty()))
				{
					send_cmd(fd, MSG_NOSIGNAL, ";");
					bool is_first = true;
					ats::StringMap::const_iterator i = md.m_off_list.begin();

					while(i != md.m_off_list.end())
					{
						send_cmd(fd, MSG_NOSIGNAL, "%s\"%s\"", is_first ? "" : "\n", i->first.c_str());
						is_first = false;
						++i;
					}

				}

				send_cmd(fd, MSG_NOSIGNAL, "\r");
			}
			else
			{
				send_cmd(fd, MSG_NOSIGNAL, "error: invalid command %s\n\r", cmd.c_str());
			}

		}

		cmdline.clear();
	}

	return 0;
}

static int main_server(const ats::StringMap& p_on_list, const ats::StringMap& p_off_list, const ats::StringMap& p_disable_list)
{
	ats::daemonize();
	MyData md(p_on_list, p_off_list, p_disable_list);

	ats::su("applet");
	ServerData sd;
	init_ServerData(&sd, 64);
	sd.m_hook = &md;
	sd.m_cs = feature_server;
	start_redstone_ud_server(&sd, g_app_name.c_str(), 1);
	signal_app_unix_socket_ready(g_app_name.c_str(), g_app_name.c_str());
	signal_app_ready(g_app_name.c_str());
	ats::infinite_sleep();
	return 0;
}

// Description: Turns ON all features in "p_on_list", and turns OFF all features in "p_disable_list".
//
// Return: 0 is returned on success, and an error value is returned otherwise.
static int process_features(DB& p_config_db, const ats::StringMap& p_on_list, const ats::StringMap& p_disable_list)
{
	DB& db = p_config_db;
	ats::StringMap app_id;
	ats::StringMap app_cmd_list;
	ats::StringMap app_unset_cmd_list;
	ats::StringMap app_fboot_cmd_list;

	DB feature_db;
	{
		const ats::String& emsg = feature_db.open_db(g_app_name, g_feature_db);

		if(!emsg.empty())
		{
			fprintf(stderr, "%s,%d: Failed to open feature database. %s\n", __FILE__, __LINE__, emsg.c_str());
			return 1;
		}

	}

	get_feature_execlist(feature_db, app_id, app_cmd_list, app_unset_cmd_list, app_fboot_cmd_list);

	{
		ats::StringMap::const_iterator i = p_disable_list.begin();

		while(i != p_disable_list.end())
		{
			const char* feature = (i->first).c_str();
			db.Set("feature", feature, "");

			{
				ats::StringMap::iterator j = app_unset_cmd_list.find(app_id.get(i->first));
				const ats::String& exec = (j != app_unset_cmd_list.end()) ? j->second : "";

				if((!exec.empty()) && (!fork()))
				{
					const ats::String full_exec("FEATURE=OFF exec " + exec);
					execl("/bin/sh", "/bin/sh", "-c", full_exec.c_str(), NULL);
					syslog(LOG_ERR, "%s,%d: Failed to run command \"%s\". (%d) %s", __FILE__, __LINE__, full_exec.c_str(), errno, strerror(errno));
					exit(1);
				}

			}

			++i;
		}

	}

	ats::StringMap app_list;

	create_list_of_all_apps_required_to_support_all_features(p_on_list, app_list);

	ats::StringList dep_list;
	create_dependency_list(app_list, dep_list);

	create_app_run_script(feature_db, dep_list);

	ats::StringMap pid;
	run_apps(dep_list, app_cmd_list, app_fboot_cmd_list, pid);

	for(;;)
	{
		int status;
		const int ret = wait3(&status, 0, 0);

		if(-1 == ret)
		{

			if(ECHILD != errno)
			{
				syslog(LOG_ERR, "%s,%d: Failed to wait for child processes. (%d) %s", __FILE__, __LINE__, errno, strerror(errno));
				return 1;
			}

			break;
		}

		if(!WIFEXITED(status) || WEXITSTATUS(status))
		{
			syslog(LOG_ERR, "Child %d [%s] exited abnormally", ret, pid.get(ats::toStr(ret)).c_str());
		}

	}

	return 0;
}

int main(int argc, char* argv[])
{
	ats::StringMap on_list;
	ats::StringMap off_list;
	ats::StringMap disable_list;

	{
		DB db;
		// ATS FIXME: ConfigDB class should automatically open the configuration database.
		//	Also, do not inherit from ConfigDB for non-configuration databases.
		db.open_db_config();
		g_feature_db = db.GetValue(g_app_name, "feature.db", "/etc/redstone/feature.db");
		g_feature_db_backup_dir = "/tmp/." + g_app_name + "/feature-db-backup";

		if((argc > 0) && (ats::String("feature") == argv[0]))
		{
			return main_feature(db, argc, argv);
		}
		else if((argc > 0) && (ats::String("feature-app") == argv[0]))
		{
			return main_feature_app(db, argc, argv);
		}

		get_feature_list(db, on_list, off_list, disable_list);

		if(fork())
		{
			return process_features(db, on_list, disable_list);
		}

	}

	return main_server(on_list, off_list, disable_list);
}
