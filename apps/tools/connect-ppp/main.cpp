// Description: Creates a Point-to-Point-Protocol (PPP) connection using the given "p_network".
//
//	The return status is zero on success, and non-zero on error.
//
// Usage: connect-ppp <Network> [options=value]
#include <fstream>
#include <iostream>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>

#include <netdb.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ats-common.h"
#include "atslogger.h"
#include "ConfigDB.h"
#include <RedStone_IPC.h>
#include <SingletonProcess.h>


REDSTONE_IPC g_RedStone;

static ATSLogger g_log;
static ats::StringMap g_arg;
static int g_debug = 0;

static void get_all_carriers(db_monitor::ConfigDB& p_apn_db)
{
	const char* apn_db_fname = "/etc/redstone/apn.db";
	p_apn_db.open_db("apn", apn_db_fname);
	p_apn_db.query("apn", "select v_APN,v_Network from t_APN");
}

static size_t max_carriers(const db_monitor::ConfigDB& p_apn_db)
{
	return p_apn_db.Table().size();
}

static void show_carriers(const db_monitor::ConfigDB& p_apn_db)
{

	for(size_t i = 0; i < max_carriers(p_apn_db); ++i)
	{
		const db_monitor::ResultRow& r = p_apn_db.Table()[i];

		if(r.size() >= 2)
		{
			ats_logf(ATSLOG_ERROR, "carrier[%zu] %s,%s\n", i, r[0].c_str(), r[1].c_str());
		}

	}

}

static const ats::String& get_apn_by_network(const db_monitor::ConfigDB& p_apn_db, const ats::String& p_network, size_t p_index)
{

	for(size_t i = 0; i < max_carriers(p_apn_db); ++i)
	{
		const db_monitor::ResultRow& r = p_apn_db.Table()[i];

		if((r.size() >= 2) && (0 == strcasecmp(p_network.c_str(), r[1].c_str())))
		{

			if(!p_index)
			{
				return r[0];
			}
			else
			{
				--p_index;
			}

		}

	}

	return ats::g_empty;
}

static bool get_ip(int p_fd, struct ifreq& p_ifr)
{

	if(ioctl(p_fd, SIOCGIFADDR, &p_ifr) < 0)
	{
		ats_logf(ATSLOG_ERROR, "ioctl: (%d) %s", errno, strerror(errno));
		return false;
	}

	return true;
}

static ats::String get_valid_ip(struct ifreq& p_ifr)
{
	const ats::String ip(inet_ntoa(((struct sockaddr_in *)&(p_ifr.ifr_addr))->sin_addr));
	bool octet_good[4] = {false, false, false, false};

	{
		size_t octet = 0;

		for(size_t i = 0; i < ip.size(); ++i)
		{
			const char c = ip[i];

			if('.' == c)
			{

				if(octet++ > 2)
				{
					return ats::String();
				}

			}
			else
			{

				if(!isdigit(c))
				{
					return ats::String();
				}

				octet_good[octet] = true;
			}

		}

	}

	return (octet_good[0] && octet_good[1] && octet_good[2] && octet_good[3]) ? ip : ats::String();
}

static bool add_low_priority_route()
{
	const int pid = fork();

	if(!pid)
	{
		const char* route = "/sbin/route";
		execl(route, route, "-n", "add", "default", "metric", g_arg.get("metric", "10").c_str(), "ppp0", NULL);
		exit(1);
	}

	int status;
	const int ret = waitpid(pid, &status, 0);

	if(ret != pid)
	{
		ats_logf(ATSLOG_ERROR, "%s: waitpid(%d) returned %d", __FUNCTION__, pid, ret);
		exit(1);
	}

	return (WIFEXITED(status) && (0 == WEXITSTATUS(status)));
}

static void wait_for_ppp0_ip_address()
{
	const int ip_poll_period = g_arg.get_int("ip_poll_period", 250000);

	for(;;)
	{
		const int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
		struct ifreq ifr;
		strncpy(ifr.ifr_name, "ppp0", IFNAMSIZ - 1);
		ifr.ifr_name[IFNAMSIZ - 1] = '\0';

		if(!get_ip(fd, ifr))
		{
			g_RedStone.pppState(APS_UNCONNECTED);    // ppp is not established!
			exit(1);
		}

		const ats::String& ip = get_valid_ip(ifr);

		if(!(ip.empty()))
		{
			ats_logf(ATSLOG_ERROR, "ppp0 IP address set to %s", ip.c_str());

			if(!add_low_priority_route())
			{
				ats_logf(ATSLOG_ERROR, "Failed to add low priority route");
			}

			break;
		}

		usleep(ip_poll_period);
	}

}

static void prepare_user_credentials(db_monitor::ConfigDB& p_db)
{
	const char* options = "/tmp/config/options.ttyModem";
	const char* secrets = "/tmp/config/pap-secrets";
	const ats::String& user(p_db.GetValue("Cellular", "UserName"));

	if(user.empty())
	{
		unlink(options);
		unlink(secrets);
		return;
	}

	const ats::String& pass(p_db.GetValue("Cellular", "Password"));

	{
		FILE* f = fopen(options, "w");

		if(f)
		{
			fchmod(fileno(f), 0600);
			std::stringstream s;
			s << "name " << user << '\n';
			fwrite(s.str().c_str(), 1, s.str().size(), f);
			fclose(f);
		}

	}

	{
		FILE* f = fopen(secrets, "w");

		if(f)
		{
			fchmod(fileno(f), 0600);
			std::stringstream s;
			s << user << " * " << pass << '\n';
			fwrite(s.str().c_str(), 1, s.str().size(), f);
			fclose(f);
		}

	}

}

// Description: Gets a Point-to-Point-Protocol (PPP) connection using the given "p_network".
//
//	The Access Point Name (APN) for network "p_network" is read from the "p_an_db" database.
//	The default APN used when all others fail is "inet.bell.ca".
//
// Algorithm:
//
//	1. Set sequence = 0 (Access Point Name (APN) list is zero-based).
//	2. Select the nth Access Point Name (APN) that matches "p_network", where "n == sequence".
//	3. Create a Point-to-Point-Protocol (PPP) connection
//	4. If there is an internal PPP system failure (not due to APN/Network/Carrier settings) then exit the program
//	5. If the PPP connection was successful, then return
//	6. If this is the last APN/Network/Carrier setting to try, then exit the program
//	7. Increment the sequence number and continue at step 2.
static void get_ppp0_connection(db_monitor::ConfigDB& p_db, const db_monitor::ConfigDB& p_apn_db, const ats::String& p_network)
{
	const ats::String default_apn("inet.bell.ca");
	ats::String apn(p_db.GetValue("Cellular", "carrier"));

	size_t sequence = 0;
	bool auto_apn = false;
	ats::String previous_apn;
	int tryCount = 0;

	if(apn.empty() || ("auto" == apn))
	{
		auto_apn = true;
	}

	bool final_choice = false;

	for(;;)
	{

		if(auto_apn)
		{
			apn = get_apn_by_network(p_apn_db, p_network, sequence);

			if(apn.empty())
			{
				apn = default_apn;
			}

		}

		if(previous_apn == apn)
		{
			final_choice = true;

			if(auto_apn)
			{
				apn = default_apn;
			}
			else
			{
				ats_logf(ATSLOG_ERROR, "User specified APN (%s) failed, will try %d more time...", apn.c_str(), 5 - tryCount);
				tryCount++;
				sleep(10);
			}

		}

		const int pid = fork();

		if(!pid)
		{
			const char* pppd = "/usr/bin/pppd";
			const char* modem_port = "/dev/ttyModem";
			const char* baud_rate = "921600";

			prepare_user_credentials(p_db);

			char chat[256];
			snprintf(chat, sizeof(chat) - 1, "chat -t 6 -s -v \"\" ATZ OK AT+CGDCONT=1,\\\"IP\\\",\\\"%s\\\",\\\"0.0.0.0\\\",0,0 OK ATDT*99***1# CONNECT \\d\\c", apn.c_str());
			chat[sizeof(chat) - 1] = '\0';

			if(auto_apn)
			{
				ats_logf(ATSLOG_ERROR, "Calling pppd(%s), network=%s, %s", apn.c_str(), p_network.c_str(), final_choice ? "final choice" : ("choice " + ats::toStr(sequence + 1)).c_str());
			}
			else
			{
				ats_logf(ATSLOG_ERROR, "Calling pppd(%s), user specified APN", apn.c_str());
			}

			execl(pppd, pppd,
				modem_port,
				baud_rate,
				"crtscts",
				"updetach",
				"usepeerdns",
				"lcp-echo-failure",
				"6",
				"lcp-echo-interval",
				"10",
				"connect",
				chat,
				NULL);
			ats_logf(ATSLOG_ERROR, "Failed to run pppd. (%d) %s", errno, strerror(errno));
			exit(1);
		}

		int status;
		const int ret = waitpid(pid, &status, 0);

		if(ret != pid)
		{
			ats_logf(ATSLOG_ERROR, "waitpid(%d) returned %d", pid, ret);
			g_RedStone.pppState(APS_UNCONNECTED);    // ppp is not established!
			exit(1);
		}

		if(WIFEXITED(status))
		{

			if(0 == WEXITSTATUS(status))
			{
				ats_logf(ATSLOG_ERROR, "pppd connected");
				break;
			}

			ats_logf(ATSLOG_ERROR, "pppd returned %d", WEXITSTATUS(status));
		}
		else
		{

			if(WIFSIGNALED(status))
			{
				ats_logf(ATSLOG_ERROR, "pppd exited on signal %d", WTERMSIG(status));
			}
			else
			{
				ats_logf(ATSLOG_ERROR, "pppd exited abnormally status=0x%08X", status);
			}

		}

		if(final_choice)
			g_RedStone.ModemState(AMS_NO_CARRIER);  //indicates there may be some issue with the APN
			
		if (tryCount >= 5)
		{
			g_RedStone.pppState(APS_UNCONNECTED);    // ppp is not established!
			exit(1);
		}

		previous_apn = apn;
		++sequence;
	}

}

static void load_ppp_module()
{
	const int pid = fork();

	if(!pid)
	{
		ats::ignore_return<FILE*>( freopen("/dev/null", "r", stdin));
		ats::ignore_return<FILE*>( freopen("/dev/null", "w", stdout));
		ats::ignore_return<FILE*>( freopen("/dev/null", "w", stderr));
		const char* modprobe = "/sbin/modprobe";
		execl(modprobe, modprobe, "ppp_generic", NULL);
		exit(1);
	}

	int status;
	const int ret = waitpid(pid, &status, 0);

	if(ret != pid)
	{
		ats_logf(ATSLOG_ERROR, "%s: Failed to run modprobe(%d): (%d) %s", __FUNCTION__, pid, errno, strerror(errno));
	}
	else if(WIFEXITED(status))
	{

		if(WEXITSTATUS(status))
		{
			ats_logf(ATSLOG_ERROR, "%s: modprobe returned %d", __FUNCTION__, WEXITSTATUS(status));
		}

	}
	else
	{
		ats_logf(ATSLOG_ERROR, "%s: modprobe status=0x%08X", __FUNCTION__, status);
	}

}

int main(int argc, char* argv[])
{
	if(argc >= 3)
	{
		g_arg.from_args(argc - 2, argv + 2);
	}

	db_monitor::ConfigDB db;

	g_debug = db.GetInt("RedStone", "LogLevel", 0);
	g_log.set_global_logger(&g_log);
  g_log.open_testdata("connect-ppp");
	g_log.set_level(g_debug);
	db.open_db_config();
	ats_logf(ATSLOG_ERROR, "----------- starting connect-ppp");
	ats_logf(ATSLOG_ERROR, "pppState is %d", g_RedStone.pppState());

	if (g_RedStone.pppState() == APS_CONNECTING)
	{
		ats_logf(ATSLOG_ERROR, "Singleton alert - process is running already (pppState is APS_CONNECTING).");
		return 1;
	}

	g_RedStone.pppState(APS_CONNECTING);  // ppp is not established but we are trying
	ats_logf(ATSLOG_ERROR, "pppState is now %d (should be 1)", g_RedStone.pppState());
	
	db_monitor::ConfigDB apn_db;
	get_all_carriers(apn_db);

	if(g_debug >= 5)
	{
		show_carriers(apn_db);
	}

	load_ppp_module();

	const ats::String network((argc >= 2) ? argv[1] : "");

	get_ppp0_connection(db, apn_db, network);

	wait_for_ppp0_ip_address();
	g_RedStone.pppState(APS_CONNECTED);    // ppp is established!
	return 0;
}
