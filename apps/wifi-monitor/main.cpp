#include <set>
#include <iostream>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <syslog.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
// AWARE360 FIXME: "#include <linux/*>" is reserved for the Kernel and drivers. This program should use alternate headers or
//	should be restructured to separate these header files from standard application code. An example is "linux/if.h"
//	and "net/if.h" being incompatible during the compile process.
#include <linux/if.h>
#include <linux/wireless.h>

#include "ats-common.h"
#include "atslogger.h"
#include "db-monitor.h"
#include "ConfigDB.h"
#include "command_line_parser.h"

#include "wifi-monitor.h"

ATSLogger g_log;
static const int g_default_route_metric = 5;
static int g_route_metric = g_default_route_metric;

int g_dbg = 0;
#define scan_interval 2*60
ats::String trim(const ats::String& str);

// ac_updatedb: updatedb [option or key=value] ...
//
// Description: Updates the WiFi network database.
//
// Options:
//	H - Indicates that the input is NOT in hexidecimal format, and output should not be returned in
//	    hexidecimal format. "H" is for human-readable.
//
// Parameters:
//	username - The SSID of the WiFi network (this is deprecated, do not use).
//	ssid     - The SSID of the WiFi network
//	password - The password for the WiFi network
//
// ATS FIXME: Rename "username" to SSID.
int ac_updatedb(AdminCommandContext &p_acc, int p_argc, char *p_argv[]);

int ac_unsetdb(AdminCommandContext &p_acc, int p_argc, char *p_argv[]);

int ac_debug(AdminCommandContext &p_acc, int p_argc, char *p_argv[])
{
	if(p_argc <= 1)
	{
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "debug=%d\n", g_dbg);
	}
	else
	{
		g_dbg = strtol(p_argv[1], 0, 0);
		g_log.set_level(g_dbg);
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "debug=%d\n", g_dbg);
	}
	return 0;
}

int ac_help(AdminCommandContext &p_acc, int p_argc, char *p_argv[])
{
	send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "This is the help system\n\r");
	return 0;
}

int ac_stats(AdminCommandContext &p_acc, int p_argc, char *p_argv[])
{
	const db_monitor::DBMonitorContext& db = (p_acc.my_data()).DB();

	if(db.Table().size() <= 0)
	{
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "DataBase empty\n\r");
		return 0;
	}

	const ats::String& s = (p_acc.my_data()).check_assoc();
	if(s.empty())
	{
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s not associated\n\r", APCLI_IFNAME);
	}
	else
	{
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s associated with \"%s\"\n\r",APCLI_IFNAME , s.c_str());
	}

	return 0;
}

MyData::MyData():m_db_empty(true)
{
	m_command.insert(AdminCommandPair("debug", ac_debug));
	m_command.insert(AdminCommandPair("help", ac_help));
	m_command.insert(AdminCommandPair("updatedb", ac_updatedb));
	m_command.insert(AdminCommandPair("unsetdb", ac_unsetdb));
	m_command.insert(AdminCommandPair("stats", ac_stats));
}

assoc_essid MyData::getassocEssid()
{
	wifiSecurityList::iterator it;
	assoc_essid as;
	as.tb=0;
	as.password="";
	ats_logf(ATSLOG(0), "Scanning for networks...");
	{
		ats::String buf;
		ats_sprintf(&buf, "ifconfig '%s' up", APCLI_IFNAME);
		ats::system(buf);
	}
	get_site_survey();

	for(it = m_wsl.begin(); it != m_wsl.end(); ++it)
	{
		const ats::String& name = trim((*it).first);
		survey_table * c = wifi_find_ap(name);
		if(c)
		{
			as.tb=c;
			as.password=(*it).second.c_str();
		    break;
		}
	}
	return as;
}

bool h_check_for_ip()
{
	ats::String buf;
	ats_sprintf(&buf,"ifconfig %s | grep -q \"inet addr:[0-9]\\+\\.[0-9]\\+\\.[0-9]\\+\\.[0-9]\"", APCLI_IFNAME);
	int ret = ats::system(buf);
	if(WIFEXITED(ret))
	{
		if (WEXITSTATUS(ret))
		{
			return true;
		}
	}
	return false;
}

bool h_obtain_ip_loop()
{
	ats::String buf;
	ats_sprintf(&buf, "metric=%d udhcpc -q -n -i %s", g_route_metric, APCLI_IFNAME);
	int i;
	for(i=0;i<5;i++)
	{
		int ret = ats::system(buf);
		ats_logf(ATSLOG(0), "Running udhcpc on %s. Return %d", APCLI_IFNAME, ret);
		if(WIFEXITED(ret))
		{
			ats_logf(ATSLOG(0), "udhcpc exited with code %d", WEXITSTATUS(ret));
			if(0 == WEXITSTATUS(ret))
			{
				return true;
			}
		}
		sleep(2);
	}


	return false;
}

void h_set_default_resolv()
{
	ats::system("/usr/bin/set-default-resolv-conf.sh");
}

void * h_assoc_loop(void *p)
{
	bool use_static_dns = false;
	{
		db_monitor::ConfigDB db;
		const ats::String& err = db.open_db_config();

		g_route_metric = db.GetInt("system", "apcli0RouteMetric", ats::toStr(g_default_route_metric));
		if(!err.empty())
		{
			ats_logf(ATSLOG(0), "%s,%d: ConfigDB returned\"%s\"", __FILE__, __LINE__, err.c_str());
		}

		if(db.GetInt("RedStone", "StaticDNS", "0"))
		{
			use_static_dns = true;
		}

	}
	if(!use_static_dns)
	{
		h_set_default_resolv();
	}

	MyData &md = *((MyData *)p);
	static int try_count = 0;
	static int assoc_count = 0;
	ats::String lastTryEssid;
	bool udhcpc_ret = false;
	bool deviceReady = false;
	while(1)
	{
		if(md.isDBempty())
		{
			sleep(15);
			continue;
		}

		if(deviceReady == false)
		{
			if(!md.getWirelessName().empty())
			{
				deviceReady = true;
			}
			else
			{
				sleep(5);
				continue;
			}
		}

		if((md.check_assoc()).empty() || !udhcpc_ret )
		{
			ats_logf(ATSLOG(0), "%s is not associated", APCLI_IFNAME);
			assoc_essid c = md.getassocEssid();
			if(!use_static_dns && assoc_count > 0)
			{
				h_set_default_resolv();
			}
			assoc_count = 0;
			if(c.tb)
			{

				if(lastTryEssid.compare(c.tb->ssid) == 0 && try_count == 3)
				{
					md.pushbackItem(c.tb->ssid);
					try_count = 0;
					lastTryEssid.clear();
					sleep(scan_interval);
					continue;
				}

				ats_logf(ATSLOG(0), "Found network, trying to associate (essid: %s, channel: %s, enc: %s, crypto: %s)", c.tb->ssid.c_str(), c.tb->channel.c_str(), c.tb->enc.c_str(), c.tb->crypto.c_str());
				md.wifi_start(ats::String(APCLI_IFNAME), c);
				udhcpc_ret = h_obtain_ip_loop();
				lastTryEssid = c.tb->ssid;
				try_count++;
			}
			else
			{
				ats_logf(ATSLOG(0), "No signal found to connect to");
				try_count = 0;
			}
		}
		else
		{
			if (assoc_count == 0)
			{
				ats_logf(ATSLOG(0), "%s is associated", APCLI_IFNAME);
			}

			assoc_count++;
			if(assoc_count > 5)//keep connect after 10 mins.
			{
				try_count = 0;
			}
			if((assoc_count % 4) == 0)
				ats_logf(ATSLOG(0), "%s is still associated", APCLI_IFNAME);
		}

		//2 mins interval for rescanning if no any ap associated, or check associated ap.
		sleep(scan_interval);
	}

	return 0;
}

// ************************************************************************
// Description: A local command server so that other applications or
//      developers can query this application.
//
//      One instance of this function is created for every local connection
//      made.
//
// Parameters:
// Return: NULL pointer
// ************************************************************************
static void* local_command_server( void* p)
{
	ClientData* cd = (ClientData*)p;
	MyData &md =* ((MyData*)(cd->m_data->m_hook));

	bool command_too_long = false;
	ats::String cmd;

	const size_t max_command_length = 1024;

	AdminCommandContext acc(md, *cd);

	ClientDataCache cdc;
	init_ClientDataCache(&cdc);

	for(;;)
	{
		char ebuf[1024];
		const int c = client_getc_cached(cd, ebuf, sizeof(ebuf), &cdc);

		if(c < 0)
		{
			if(c != -ENODATA) ats_logf(ATSLOG(0), "%s,%d: %s", __FILE__, __LINE__, ebuf);
			break;
		}

		if(c != '\r' && c != '\n')
		{
			if(cmd.length() >= max_command_length) command_too_long = true;
			else cmd += c;

			continue;
		}

		if(command_too_long)
		{
			ats_logf(ATSLOG(0), "%s,%d: command is too long", __FILE__, __LINE__);
			cmd.clear();
			command_too_long = false;
			send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "command is too long\n");
			continue;
		}

		CommandBuffer cb;
		init_CommandBuffer(&cb);
		const char* err;

		if((err = gen_arg_list(cmd.c_str(), cmd.length(), &cb)))
		{
			ats_logf(ATSLOG(0), "%s,%d: gen_arg_list failed (%s)", __FILE__, __LINE__, err);
			send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "gen_arg_list failed: %s\n", err);
			cmd.clear();
			continue;
		}

		ats_logf(ATSLOG(2), "%s,%d:%s: Command received: %s", __FILE__, __LINE__, __FUNCTION__, cmd.c_str());

		cmd.clear();

		if(cb.m_argc <= 0)
		{
			continue;
		}

		{
			const ats::String cmd(cb.m_argv[0]);
			AdminCommandMap::const_iterator i = md.m_command.find( cmd);

			if(i != md.m_command.end())
			{
				(i->second)(acc, cb.m_argc, cb.m_argv);
			}
			else
			{
				send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "Invalid command \"%s\"\n", cmd.c_str());
			}
		}
	}
	return 0;
}

void MyData::start_server()
{
	ServerData& sd = m_command_server;
	init_ServerData(&sd, 32);
	sd.m_hook = this;
	sd.m_cs =local_command_server;
	set_unix_domain_socket_user_group(&sd, "applet", "db-monitor");
	start_redstone_ud_server(&sd, "wifi-monitor", 1);
	signal_app_unix_socket_ready("wifi-monitor", "wifi-monitor");
	signal_app_ready("wifi-monitor");
}

void MyData::iwpriv(const ats::String& name, const ats::String& key, const ats::String& val)
{
	int socket_id;
	struct iwreq wrq;
	char data[64];
	snprintf(data, 64, "%s=%s", key.c_str(), val.c_str());
	socket_id = socket(AF_INET, SOCK_DGRAM, 0);
	strcpy(wrq.ifr_ifrn.ifrn_name, name.c_str());
	wrq.u.data.length = strlen(data);
	wrq.u.data.pointer = data;
	wrq.u.data.flags = 0;
	ioctl(socket_id, RTPRIV_IOCTL_SET, &wrq);
	close(socket_id);
}

ats::String trim(const ats::String& str)
{
	ats::String::size_type pos = str.find_first_not_of(' ');
	if (pos == ats::String::npos)
	{
		return str;
	}
	ats::String::size_type pos2 = str.find_last_not_of(' ');
	if (pos2 != ats::String::npos)
	{
		return str.substr(pos, pos2 - pos + 1);
	}
	return str.substr(pos);
}

int ac_unsetdb(AdminCommandContext &p_acc, int p_argc, char *p_argv[])
{
	ats::StringMap sm;
	ats::StringMap opt;
	sm.from_args(p_argc - 1, p_argv + 1, opt);
	const bool human_readable = opt.has_key("H");

	if(sm.empty())
	{
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "unsetdb username=\"name\" or all\n\r");
		return 0;
	}

	ats::String s;

	if(sm.has_key("all"))
	{
		s = "delete from t_security";
	}
	else if(sm.has_key("username"))
	{
		const ats::String& username = human_readable ? ats::from_hex(sm.get("username")) : sm.get("username");

		if(username.empty())
		{
			send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "No username specified\n");
			return 0;
		}

		s = "delete from t_security where v_username='" + username + "';";
	}

	ats::String ret;

	if(!s.empty())
	{
		const ats::String& err = ((p_acc.my_data()).DB()).query(s);

		if(!err.empty())
			ret = err;
		else
			ret = "OK";
	}
	else
	{
		ret = "ERROR: wrong format";
	}

	send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s\n\r", ret.c_str());
	return 0;
}

int ac_updatedb(AdminCommandContext &p_acc, int p_argc, char *p_argv[])
{
	ats::StringMap sm;
	ats::StringMap opt;
	sm.from_args(p_argc - 1, p_argv + 1, opt);
	const bool human_readable = opt.has_key("H");

	if(sm.empty())
	{
		const ats::String& err = ((p_acc.my_data()).DB()).query("select v_username, v_password from t_security;");

		if(!err.empty())
			send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s\n", err.c_str());

		for(int i = 0; i < ((p_acc.my_data()).DB()).Table().size(); ++i)
		{
			const db_monitor::ResultRow& row = ((p_acc.my_data()).DB()).Table()[i];
			size_t i;

			if(row.size() >= 2)
			{

				if(human_readable)
				{
					send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "username='%s' password='%s'\n", ats::from_hex(row[0]).c_str(), ats::from_hex(row[1]).c_str());
				}
				else
				{
					send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "username=%s password=%s\n", row[0].c_str(), row[1].c_str());
				}

			}
		}

		return 0;
	}

	const ats::String& username = human_readable ? ats::to_hex(sm.get("username")) : sm.get("username");
	const ats::String& password = human_readable ? ats::to_hex(sm.get("password")) : sm.get("password");
	ats::String ret;

	if(username.empty())
	{
		ret = p_argv[0] + ats::String(": error\nusername is empty\n\r");
		return 0;
	}

	const ats::String& s = "insert or replace into t_security (v_username, v_password) VALUES ('" + username + "','" + password +  "');";

	if(!s.empty())
	{
		const ats::String& err = ((p_acc.my_data()).DB()).query(s);

		if(!err.empty())
			ret = err;
		else
			ret = "OK";
	}

	send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s\n\r", ret.c_str());
	return 0;
}

bool MyData::openWifiDB()
{
	m_db = new db_monitor::DBMonitorContext(m_db_name,"/mnt/nvram/config/wifi.db");
	{
		const ats::String createdb("CREATE TABLE IF NOT EXISTS t_security "
			"(mid INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,v_username TEXT UNIQUE,v_password TEXT);");

		const ats::String& err = m_db->query(createdb);

		if(!err.empty())
		{
			ats_logf(ATSLOG(0), "%s: Error:%s when create t_security table", __PRETTY_FUNCTION__, err.c_str());
			return false;
		}

	}

	return true;
}

bool MyData::loadConfigList()
{
	{
		//only keep the lastest 20 rows.
		ats::String buf;
		ats_sprintf(&buf, "DELETE FROM t_security WHERE mid NOT IN ( SELECT mid FROM t_security ORDER BY mid DESC LIMIT 20);");
		sendsql(m_db_name);
	}

	{
		ats::String buf;
		ats_sprintf(&buf, "SELECT * FROM t_security");
		sendsql(m_db_name);
	}

	if(m_db->Table().size() <= 0)
	{
		ats_logf(ATSLOG(0), "%s: Database table size is zero ",__PRETTY_FUNCTION__);
		m_db_empty = true;
		return false;
	}

	//clean up.
	m_wsl.clear();

	for(size_t i = 0; i < m_db->Table().size(); ++i)
	{
		db_monitor::ResultRow& row = m_db->Table()[i];
		size_t i;

		if(row.size() >= 3 )
		{
			m_wsl.push_back(wifiSecurityPair(ats::from_hex(row[1]).c_str(), ats::from_hex(row[2]).c_str()));
			m_db_empty = false;
		}
		else
		{
			ats_logf(ATSLOG(0), "%s:database wrong",__PRETTY_FUNCTION__);
			return false;
		}
	}
	return true;
}

void MyData::pushbackItem(const ats::String& name)
{
	//if item is the last one, no move, if list is empty, return.
	if(m_wsl.size() == 0) return;

	wifiSecurityList::iterator result = m_wsl.begin();
	result = std::find_if( result, m_wsl.end(), std::bind2nd(wsfind(), name ) );
	if(result == m_wsl.end())
	{
		ats_logf(ATSLOG(0), "%s: no %s found",__PRETTY_FUNCTION__, name.c_str());
		return;
	}

	wifiSecurityList::iterator end = m_wsl.end();
	end--;

	if(result  == end)
		return;

	wifiSecurityPair p = (*result);
	m_wsl.erase(result);
	m_wsl.push_back(p);
}

ats::String MyData::getWirelessName()
{
	int socket_id;
	char name[255];
	struct iwreq wrq;
	char data[4096];

	socket_id = socket(AF_INET, SOCK_DGRAM, 0);
	if( socket_id < 0 )
	{
		ats_logf(ATSLOG(0), "%s:Error: open socket error",__PRETTY_FUNCTION__);
		return "";
	}

	sprintf(name, APCLI_IFNAME);
	memset(data, 0x00, 255);

	strcpy(wrq.ifr_name, name);
	wrq.u.data.length = 255;
	wrq.u.data.pointer = data;
	wrq.u.data.flags = 0;

	int ret = ioctl(socket_id, SIOCGIWNAME, &wrq);
	if( ret != 0 )
	{
		ats_logf(ATSLOG(0), "%s:Error: get wireless name",__PRETTY_FUNCTION__);
		return "";
	}

	ats_logf(ATSLOG(5), "%s:Get wireless name: %s",__PRETTY_FUNCTION__, wrq.u.name);
	return ats::String( wrq.u.name );
}

ats::String MyData::check_assoc()
{
	int socket_id;
	socket_id = socket(AF_INET, SOCK_DGRAM, 0);

	struct iwreq wrq;
	char essid[32]={0};
	memset(&wrq, 0, sizeof(struct iwreq));
	strcpy(wrq.ifr_ifrn.ifrn_name, APCLI_IFNAME);
	wrq.u.data.length = 32;
	wrq.u.data.pointer = essid;
	wrq.u.data.flags = 0;

	if(ioctl(socket_id, SIOCGIWESSID, &wrq)<0)
	{
		ats_logf(ATSLOG(0), "%s:ioctl[SIOCGIWESSID] error",__PRETTY_FUNCTION__);
		close(socket_id);
		return "";
	}

	close(socket_id);
	ats::String s = (char *)wrq.u.data.pointer;
	return s;

}

void MyData::wifi_start(const ats::String& ifname, const assoc_essid& ae)
{
	const bool using_sta_driver = ats::file_exists("/tmp/flags/ralink-sta-driver");
	ats_logf(ATSLOG(0), "Configuring WiFi for RaLink %s driver...", using_sta_driver ? "STA" : "AP");
	const int error_code = using_sta_driver ? wifi_start_sta(ifname, ae) : wifi_start_ap(ifname, ae);

	if(error_code)
	{
		ats_logf(ATSLOG(0), "%s: Failed with error code %d", __FUNCTION__, error_code);
	}

}

int MyData::wifi_start_ap(const ats::String& ifname, const assoc_essid& ae)
{
	int enctype = 0;

	iwpriv(APCLI_IFNAME, "Channel", ae.tb->channel);
	iwpriv(ifname, "ApCliEnable", "0");

	if((strstr(ae.tb->enc.c_str(), "WPA2PSK") || strstr(ae.tb->enc.c_str(), "WPAPSKWPA2PSK")) && ae.password.size() > 0)
	{
		enctype = 1;
		iwpriv(ifname, "ApCliAuthMode", "WPA2PSK");
	}
	else if(strstr(ae.tb->enc.c_str(), "WPAPSK") && ae.password.size() > 0)
	{
		enctype = 1;
		iwpriv(ifname, "ApCliAuthMode", "WPAPSK");
	}
	else if(strstr(ae.tb->enc.c_str(), "WEP") && ae.password.size() > 0)
	{
		iwpriv(ifname, "ApCliAuthMode", "AUTOWEP");
		iwpriv(ifname, "ApCliEncrypType", "WEP");
		iwpriv(ifname, "ApCliDefaultKeyID", "1");
		iwpriv(ifname, "ApCliKey1", ae.password);
		iwpriv(ifname, "ApCliSsid", ae.tb->ssid);
	}
	else if(ae.password.size() == 0 )
	{
		iwpriv(ifname, "ApCliAuthMode", "NONE");
		iwpriv(ifname, "ApCliSsid", ae.tb->ssid);
	}
	else
	{
		return -1;
	}

	if(enctype)
	{
		if(strstr(ae.tb->crypto.c_str(), "AES") || strstr(ae.tb->crypto.c_str(), "TKIPAES"))
			iwpriv(ifname, "ApCliEncrypType", "AES");
		else
			iwpriv(ifname, "ApCliEncrypType", "TKIP");
		iwpriv(ifname, "ApCliSsid", ae.tb->ssid);
		iwpriv(ifname, "ApCliWPAPSK", ae.password);
	}

	iwpriv(ifname, "ApCliEnable", "1");
	return 0;
}

int MyData::wifi_start_sta(const ats::String& ifname, const assoc_essid& ae)
{
	int enctype = 0;

	iwpriv(APCLI_IFNAME, "Channel", ae.tb->channel);

	if((strstr(ae.tb->enc.c_str(), "WPA2PSK") || strstr(ae.tb->enc.c_str(), "WPAPSKWPA2PSK")) && ae.password.size() > 0)
	{
		enctype = 1;
		iwpriv(ifname, "AuthMode", "WPA2PSK");
	}
	else if(strstr(ae.tb->enc.c_str(), "WPAPSK") && ae.password.size() > 0)
	{
		enctype = 1;
		iwpriv(ifname, "AuthMode", "WPAPSK");
	}
	else if(strstr(ae.tb->enc.c_str(), "WEP") && ae.password.size() > 0)
	{
		iwpriv(ifname, "AuthMode", "AUTOWEP");
		iwpriv(ifname, "EncrypType", "WEP");
		iwpriv(ifname, "DefaultKeyID", "1");
		iwpriv(ifname, "Key1", ae.password);
		iwpriv(ifname, "SSID", ae.tb->ssid);
	}
	else if(ae.password.size() == 0 )
	{
		iwpriv(ifname, "AuthMode", "NONE");
		iwpriv(ifname, "SSID", ae.tb->ssid);
	}
	else
	{
		return -1;
	}

	if(enctype)
	{
		if(strstr(ae.tb->crypto.c_str(), "AES") || strstr(ae.tb->crypto.c_str(), "TKIPAES"))
			iwpriv(ifname, "EncrypType", "AES");
		else
			iwpriv(ifname, "EncrypType", "TKIP");
		iwpriv(ifname, "SSID", ae.tb->ssid);
		iwpriv(ifname, "WPAPSK", ae.password);
	}

	return 0;
}

survey_table * MyData::wifi_find_ap(const ats::String& name)
{
	stableList::iterator it = m_stable.begin();
	while( it != m_stable.end())
	{
		ats_logf(ATSLOG(2), "check %s %s", name.c_str(), (*it).ssid.c_str());
		if(name == (*it).ssid)
			return &(*it);

		++it;
	}

	return 0;
}

bool MyData::get_site_survey()
{
	char data[IW_SCAN_MAX_DATA];

	int socket_id;
	struct iwreq wrq;
	int ret;

	iwpriv(APCLI_IFNAME, "SiteSurvey","1");
	sleep(3);

	socket_id = socket(AF_INET, SOCK_DGRAM, 0);
	if(socket_id < 0)
	{
		ats_logf(ATSLOG(0), "error::Open socket error!");
		return false;
	}

	memset(data, 0x00,IW_SCAN_MAX_DATA);
	strcpy(data, "");

	strcpy(wrq.ifr_name, APCLI_IFNAME);
	wrq.u.data.length = IW_SCAN_MAX_DATA;
	wrq.u.data.pointer = data;
	wrq.u.data.flags = 0;

	ret = ioctl(socket_id, RTPRIV_IOCTL_GSITESURVEY, &wrq);
	if(ret != 0)
	{
		ats_logf(ATSLOG(0), "error::get site survey");
		return false;
	}

	char * start = (char*)wrq.u.data.pointer;

	ats::String s(start, wrq.u.data.length);
	std::stringstream output(s);
	ats::String line;
	std::set<ats::String> apset;

	while(std::getline(output,line))
	{
		apset.insert(line);
	}

	//clean up.
	m_stable.clear();

	std::set<ats::String>::iterator it;
	for(it = apset.begin(); it != apset.end(); ++it)
	{
		if((*it).empty())
			continue;

		//skip the first title line.
		if((*it).find("BSSID") != ats::String::npos && (*it).find("Security") != ats::String::npos)
			continue;

		const ats::String line = (*it);
		const ats::String ch = trim(line.substr(0,2));
		const ats::String essid = trim(line.substr(4,32));
		const ats::String mac = trim(line.substr(36,18));

		ats_logf(ATSLOG(2), "%s", line.c_str());
		ats::String left = trim(line.substr(54));
		ats::String::size_type pos = left.find(' ');
		left = left.substr(0, pos);

		pos = left.find('/');
		const ats::String enc_mode = left.substr(0, pos);
		const ats::String enc_type = left.substr(pos+1);

		survey_table tb;
		tb.channel = ch;
		tb.bssid = mac;
		tb.ssid = essid;
		tb.enc = enc_mode;
		tb.crypto = enc_type;
		m_stable.push_back(tb);

		ats_logf(ATSLOG(2),"ch:%s, mac:%s, essid:%s, enc:%s,type:%s", ch.c_str(), mac.c_str(), essid.c_str(), enc_mode.c_str(), enc_type.c_str());
	}

	close(socket_id);
	return true;
}

void* LED_thread(void* p)
{
	MyData &md = *((MyData*)p);

	int prev_tx_bytes = 0;
	int prev_blink_line = 0;
	int blink_line = 0;

	for(;;)
	{
		ats::String s;
		const int ret = ats::read_file("/sys/devices/virtual/net/" APCLI_IFNAME "/statistics/tx_bytes", s);

		if(ret)
		{
			blink_line = __LINE__;

			if(blink_line != prev_blink_line)
			{
				prev_blink_line = blink_line;
				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink kick name=wifi led=wifi script=\"0,250000\"\r");
			}

		}
		else
		{
			const int tx_bytes = strtol(s.c_str(), 0, 0);

			if(tx_bytes != prev_tx_bytes)
			{
				prev_tx_bytes = tx_bytes;
				blink_line = __LINE__;

				if(blink_line != prev_blink_line)
				{
					prev_blink_line = blink_line;
					send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink kick name=wifi led=wifi script=\"1,250000;0,250000\"\r");
				}
			}
			else
			{
				blink_line = __LINE__;

				if(blink_line != prev_blink_line)
				{
					prev_blink_line = blink_line;
					send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink kick name=wifi led=wifi script=\"1,250000\"\r");
				}

			}

		}

		sleep(2);
	}

}

void MyData::start_LED_monitor()
{
	const int retval = pthread_create(
		&(m_LED_thread),
		(pthread_attr_t *)0,
		LED_thread,
		this);
}

int main(int argc, char* argv[])
{
	g_log.set_global_logger(&g_log);

	MyData& md = *(new MyData());
	ats::StringMap &config = md.m_config;
	config.set("debug", "1");
	config.from_args(argc - 1, argv + 1);

	g_dbg = config.get_int("debug");
	g_log.set_level(g_dbg);

	ats::String version;
	ats::get_file_line(version, "/version", 1);
	md.setversion(version);

	const ats::String& ss = "wifi-monitor-" + version;
	openlog(ss.c_str(), LOG_PID, LOG_USER);

	g_log.open_testdata("wifi-monitor");
	ats_logf(ATSLOG(0), "Wifi Monitor Started");

	if(ats::testmode())
	{
		ats_logf(ATSLOG(0), "Device Test Mode detected, disabling WiFi-Monitor for this power-up/runtime");
		return 0;
	}

	//md.start_LED_monitor();
	md.openWifiDB();
	md.loadConfigList();

	// look for argument of form: 'wifi-monitor show=list' to dump the database

	if (argc != 1)
	{
		ats::StringMap arg;
		ats::StringMap opt;
		arg.from_args(argc - 1, argv + 1, opt);

		if (opt.has_key("h"))
		{
			printf("Usage: wifi-monitor [--help] [--list] [--show]\r\n");
			printf("       [--list] - human readable listing of available networks\r\n");
			printf("       [--show] - hex listing of available networks\r\n");
			return 0;
		}

		if(arg.has_key("--show"))
		{
			const ats::String& err = md.DB().query("select v_username, v_password from t_security;");

			if(!err.empty())
			{
				return 0;
			}

			for(int i = 0; i < ((md).DB()).Table().size(); ++i)
			{
				const db_monitor::ResultRow& row = ((md).DB()).Table()[i];
				size_t i;

				if(row.size() >= 2)
				{
					printf("username=%s password=%s\n", row[0].c_str(), row[1].c_str());
				}

			}

			return 0;
		}

		if(arg.has_key("--list"))
		{
			const ats::String& err = md.DB().query("select v_username, v_password from t_security;");

			if(!err.empty())
			{
				return 0;
			}

			for(int i = 0; i < ((md).DB()).Table().size(); ++i)
			{
				const db_monitor::ResultRow& row = ((md).DB()).Table()[i];
				size_t i;

				if(row.size() >= 2)
				{
					printf("username='%s' password='%s'\n", ats::from_hex(row[0]).c_str(), ats::from_hex(row[1]).c_str());
				}

			}

			return 0;
		}

		return 0;
	}

	pthread_t m_reader_thread;
	const int retval = pthread_create(
			&(m_reader_thread),
			(pthread_attr_t *)0,
			h_assoc_loop,
			&md);

	if( retval)
	{
		syslog( LOG_ERR, "%s,%d: Failed to create assoc_loop. (%d) %s", __FILE__, __LINE__, errno, strerror(errno));
		ats_logf(ATSLOG(0), "%s,%d: Failed to create assoc_loop. (%d) %s", __FILE__, __LINE__, errno, strerror(errno));
		return -1;
	}


	md.start_server();
	ats::infinite_sleep();
	return 0;
}
