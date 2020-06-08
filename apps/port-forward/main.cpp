#include <iostream>
#include <list>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/if_link.h>

#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

#include "ConfigDB.h"
#include "ats-common.h"
#include "ats-string.h"

#define MSG_INFO "\x1b[1;44;33m"
#define MSG_INFO2 "\x1b[1;43;30m"
#define MSG_ERR "\x1b[1;41;37m"
#define MSG_END "\x1b[0m"

class RuleSpec
{
public:
	ats::StringList m_protocol;
	ats::String m_name;
	ats::String m_ip;
	ats::String m_mac;
	int m_port[2];
	int m_sPort[2];
	int m_rule_num;

	RuleSpec()
	{
		m_port[0] = 0;
		m_port[1] = 0;
		m_sPort[0] = 0;
		m_sPort[1] = 0;
		m_rule_num = 0;
	}

	RuleSpec(int p_rule_num, const ats::String& p_sec)
	{
		m_rule_num = p_rule_num;
		from_spec(p_sec);
	}

	RuleSpec& from_spec(const ats::String& p_spec)
	{
		ats::StringList r;
		ats::split(r, p_spec, ",");
		m_name = (r.size() >= 1) ? r[0] : "";
		m_ip = (r.size() >= 2) ? r[1] : "";

		if(r.size() >= 3)
		{
			ats::StringList s;
			ats::split(s, r[2], "-");

			if(s.size())
			{
				m_port[0] = atoi(s[0].c_str());
				m_port[1] = (s.size() >= 2) ? atoi(s[1].c_str()) : m_port[0];
			}

		}
		else
		{
			m_port[0] = 0;
			m_port[1] = 0;
		}

		if(r.size() >= 4)
		{
			ats::split(m_protocol, r[3], "/");
		}

		m_mac = (r.size() >= 5) ? r[4] : "";

		if(r.size() >= 6)
		{
			ats::StringList s;
			ats::split(s, r[5], "-");

			if(s.size())
			{
				m_sPort[0] = atoi(s[0].c_str());
				m_sPort[1] = (s.size() >= 2) ? atoi(s[1].c_str()) : m_sPort[0];
			}

		}
		else
		{
			m_sPort[0] = 80;
			m_sPort[1] = 80;
		}


		return *this;
	}

	friend std::ostream& operator<<(std::ostream& o, const RuleSpec& p_spec)
	{
		o << '(' << p_spec.m_rule_num << ") " << p_spec.m_name << ',' << p_spec.m_ip << ',';
		o << p_spec.m_port[0] << '-' << p_spec.m_port[1] << ',';

		for(size_t i = 0; i < p_spec.m_protocol.size(); ++i)
		{

			if(i)
			{
				o << '/';
			}

			o << p_spec.m_protocol[i] << ',';
		}

		return o << p_spec.m_mac;
	}

};

class ReservationSpec
{
public:
	ats::String m_name;
	ats::String m_ip;
	ats::String m_mac;
	int m_rule_num;

	ReservationSpec()
	{
		m_rule_num = 0;
	}

	ReservationSpec(int p_rule_num, const ats::String& p_sec)
	{
		m_rule_num = p_rule_num;
		from_spec(p_sec);
	}

	ReservationSpec& from_spec(const ats::String& p_spec)
	{
		ats::StringList r;
		ats::split(r, p_spec, ",");
		m_name = (r.size() >= 1) ? r[0] : "";
		m_ip = (r.size() >= 2) ? r[1] : "";
		m_mac = (r.size() >= 3) ? r[2] : "";
		return *this;
	}

	friend std::ostream& operator<<(std::ostream& o, const ReservationSpec& p_spec)
	{
		o << '(' << p_spec.m_rule_num << ") " << p_spec.m_name << ',' << p_spec.m_ip << ',';
		return o << p_spec.m_mac;
	}

};

typedef std::list <RuleSpec> RuleList;
typedef std::map <int, RuleSpec> RuleMap;
typedef std::pair <int, RuleSpec> RulePair;

typedef std::list <ReservationSpec> ReservationList;
typedef std::map <int, ReservationSpec> ReservationMap;
typedef std::pair <int, ReservationSpec> ReservationPair;

static void get_ip(ats::StringMap& p_ip)
{
	p_ip.clear();
	db_monitor::ConfigDB db;
	db.open_db_config();
	const ats::String& ra0 = db.GetValue("system", "ra0addr");
	const ats::String& eth0 = db.GetValue("system", "eth0addr");

	if(!ra0.empty())
	{
		p_ip.set("ra0", ra0);
	}

	if(!eth0.empty())
	{
		p_ip.set("eth0", eth0);
	}

}

static bool is_a_rule(const ats::String& p_key)
{
	return ((p_key.length() >= 5) && (!strncasecmp("rule", p_key.c_str(), 4)));
}

static void load_reservations(const ats::String& p_app_name, ReservationList& p_res)
{
	db_monitor::ConfigDB db;
	db.open_db_config();
	const ats::String& err = db.Get("IPReservation");

	if(!err.empty())
	{
		fprintf(stderr, "%s%s: ConfigDB.Get() returned \"%s\"%s\n", MSG_ERR, p_app_name.c_str(), err.c_str(), MSG_END);
		return;
	}

	const db_monitor::ResultTable& t = db.Table();
	db_monitor::ResultTable::const_iterator i = t.begin();

	while(i != t.end())
	{
		const db_monitor::ResultRow& r = *i;
		++i;

		if(r.size() < 2)
		{
			continue;
		}

		const ats::String& key = r[3];

		if(is_a_rule(key))
		{
			const int rule_num = atoi(key.c_str() + 4);
			const ats::String& rule_def = r[1];
			p_res.push_back(ReservationSpec(rule_num, rule_def));
		}

	}

}


static void load_rules(const ats::String& p_app_name, RuleList& p_rule)
{
	db_monitor::ConfigDB db;
	db.open_db_config();
	const ats::String& err = db.Get("Forwarding");

	if(!err.empty())
	{
		fprintf(stderr, "%s%s: ConfigDB.Get() returned \"%s\"%s\n", MSG_ERR, p_app_name.c_str(), err.c_str(), MSG_END);
		return;
	}

	const db_monitor::ResultTable& t = db.Table();
	db_monitor::ResultTable::const_iterator i = t.begin();

	while(i != t.end())
	{
		const db_monitor::ResultRow& r = *i;
		++i;

		if(r.size() < 2)
		{
			continue;
		}

		const ats::String& key = r[3];

		if(is_a_rule(key))
		{
			const int rule_num = atoi(key.c_str() + 4);
			const ats::String& rule_def = r[1];
			p_rule.push_back(RuleSpec(rule_num, rule_def));
		}

	}

}

static ats::String get_3_octet(const ats::String& p_ip)
{
	ats::StringList s;
	ats::split(s, p_ip, ".");

	if(s.size() < 3)
	{
		return "";
	}

	return (s[0] + '.' + s[1] + '.' + s[2]);
}

static void simplify_protocols(const char* p_app_name, RuleList& p_rule)
{
	RuleList s;
	RuleList::const_iterator i = p_rule.begin();

	while(i != p_rule.end())
	{
		const RuleSpec& r = *i;

		if(1 == r.m_protocol.size())
		{
			s.push_back(r);
		}
		else if(r.m_protocol.size() > 1)
		{
			ats::StringList::const_iterator i = r.m_protocol.begin();
	
			while(i != r.m_protocol.end())
			{
				RuleSpec rs(r);
				rs.m_protocol.clear();
				rs.m_protocol.push_back(*i);
				s.push_back(rs);
				++i;
			}

		}

		++i;
	}

	p_rule = s;
}

static void simplify_rules(const char* p_app_name, RuleList& p_rule, const ats::StringMap& p_iface)
{
	simplify_protocols(p_app_name, p_rule);
	RuleList s;
	RuleList::const_iterator i = p_rule.begin();

	while(i != p_rule.end())
	{
		const RuleSpec& r = *i;

		if('.' == r.m_ip[0])
		{
			ats::StringMap::const_iterator i = p_iface.begin();

			while(i != p_iface.end())
			{
				const ats::String& subnet = get_3_octet(i->second);

				if(!subnet.empty())
				{
					RuleSpec rs(r);
					rs.m_ip = subnet + r.m_ip;
					s.push_back(rs);
				}

				++i;			
			}

		}
		else
		{
			ats::StringList sl;
			ats::split(sl, r.m_ip, ".");

			if(2 == sl.size())
			{
				RuleSpec rs(r);
				rs.m_ip = get_3_octet(p_iface.get(sl[0]));

				if(!(rs.m_ip.empty()))
				{
					rs.m_ip += '.' + sl[1];
					s.push_back(rs);
				}

			}

		}

		++i;
	}

	p_rule = s;
}

static void simplify_reservations(const char* p_app_name, ReservationList& p_res, const ats::StringMap& p_iface)
{
	ReservationList s;
	ReservationList::const_iterator i = p_res.begin();

	while(i != p_res.end())
	{
		const ReservationSpec& r = *i;

		if('.' == r.m_ip[0])
		{
			ats::StringMap::const_iterator i = p_iface.begin();

			while(i != p_iface.end())
			{
				const ats::String& subnet = get_3_octet(i->second);

				if(!subnet.empty())
				{
					ReservationSpec rs(r);
					rs.m_ip = subnet + r.m_ip;
					s.push_back(rs);
				}

				++i;			
			}

		}
		else
		{
			ats::StringList sl;
			ats::split(sl, r.m_ip, ".");

			if(2 == sl.size())
			{
				ReservationSpec rs(r);
				rs.m_ip = get_3_octet(p_iface.get(sl[0]));

				if(!(rs.m_ip.empty()))
				{
					rs.m_ip += '.' + sl[1];
					s.push_back(rs);
				}

			}

		}

		++i;
	}

	p_res = s;
}

static void to_ReservationMap(ReservationMap& p_rm, const ReservationList& p_rl)
{
	int n = 0;
	p_rm.clear();

	ReservationList::const_iterator i = p_rl.begin();

	while(i != p_rl.end())
	{
		p_rm.insert(ReservationPair(++n, *i));
		++i;
	}

}

static void to_RuleMap(RuleMap& p_rm, const RuleList& p_rl)
{
	int n = 0;
	p_rm.clear();

	RuleList::const_iterator i = p_rl.begin();

	while(i != p_rl.end())
	{
		p_rm.insert(RulePair(++n, *i));
		++i;
	}

}

static bool rule_is_valid(const RuleSpec& p_rs)
{
	return (
		(!(p_rs.m_protocol.empty())) &&
		(!(p_rs.m_protocol[0].empty()))
		);
}

static bool reservation_is_valid(const ReservationSpec& p_rs)
{
	return true;
}

static void apply_reservations(const char* p_app_name, const ReservationMap& p_res)
{
	ReservationMap::const_iterator i = p_res.begin();

	while(i != p_res.end())
	{
		std::stringstream s;
		s << "Reservation[" << i->first << "]: " << ReservationSpec(i->second);
		printf("%s%s: %s%s\n", MSG_INFO2, p_app_name, s.str().c_str(), MSG_END);
		++i;
	}

	ats::String dhcpd_conf;
	i = p_res.begin();

	while(i != p_res.end())
	{
		const ReservationSpec& rs = i->second;

		if(reservation_is_valid(rs))
		{
			ats::String s;
			ats_sprintf(&s,
				"host ReserveRES%d {\n"
				"hardware ethernet %s;\n"
				"fixed-address %s;\n"
				"}\n",
				i->first,
				rs.m_mac.c_str(),
				rs.m_ip.c_str());

			dhcpd_conf += s;
		}

		++i;
	}

	if(!dhcpd_conf.empty())
	{
		const char* fname = "/tmp/config/dhcpd.conf.reservation";
		FILE* f = fopen(fname, "w");

		if(f)
		{
			fwrite(dhcpd_conf.c_str(), 1, dhcpd_conf.size(), f);
			fclose(f);
			fprintf(stderr, "%s%s: IP reservations written to \"%s\"%s\n", MSG_INFO, p_app_name, fname, MSG_END);
		}
		else
		{
			fprintf(stderr, "%s%s: Could not write \"%s\"%s\n", MSG_ERR, p_app_name, fname, MSG_END);
		}

	}

}

static void apply_rules(const char* p_app_name, const RuleMap& p_rule)
{
	RuleMap::const_iterator i = p_rule.begin();

	while(i != p_rule.end())
	{
		std::stringstream s;
		s << "Rule[" << i->first << "]: " << RuleSpec(i->second);
		printf("%s%s: %s%s\n", MSG_INFO2, p_app_name, s.str().c_str(), MSG_END);
		++i;
	}

	i = p_rule.begin();

	while(i != p_rule.end())
	{
		const RuleSpec& rs = i->second;

		if(rule_is_valid(rs))
		{
			int pid = fork();

			if(!pid)
			{
				// nat table routing rule
				execl("/sbin/iptables",
					"/sbin/iptables",
					"-t", "nat",
					"-A", "PREROUTING",
					"-i", "ppp0",
					"-p", rs.m_protocol[0].c_str(),
					"--dport", ((ats::toStr(rs.m_sPort[0]) + ':') + ats::toStr(rs.m_sPort[1])).c_str(),
					"-j", "DNAT",
					"--to-destination", ((ats::toStr(rs.m_ip.c_str()) + ':') + ats::toStr(rs.m_port[0])).c_str(),
					(const char*)NULL);
				exit(1);
			}
			
			pid = fork();

			if(!pid)
			{
				// now the input rule
				execl("/sbin/iptables",
					"/sbin/iptables",
					"-A", "INPUT",
					"-i", "ppp0",
					"-p", rs.m_protocol[0].c_str(),
					"--dport", ((ats::toStr(rs.m_sPort[0]) + ':') + ats::toStr(rs.m_sPort[1])).c_str(),
					"-j", "ACCEPT",
					(const char*)NULL);
				exit(1);
			}

			{
				int status;
				const int ret = waitpid(pid, &status, 0);

				if(ret < 0)
				{
					fprintf(stderr, "%s%s: failed to apply rule <%s>. (%d) %s%s\n", MSG_ERR, p_app_name, ats::toStr(rs).c_str(), errno, strerror(errno), MSG_END);
				}
				else
				{

					if(WIFEXITED(status))
					{

						if(WEXITSTATUS(status))
						{
							fprintf(stderr, "%s%s: iptables exited with %d. failed to apply rule <%s>%s\n",
								MSG_ERR, p_app_name, WEXITSTATUS(status), ats::toStr(rs).c_str(), MSG_END);
						}
						else
						{
							printf("%s%s: Applied rule <%s>%s\n", MSG_INFO, p_app_name, ats::toStr(rs).c_str(), MSG_END);
						}

					}
					else if(WIFSIGNALED(status))
					{
						fprintf(stderr, "%s%s: iptables got signal %d. failed to apply rule <%s>%s\n",
							MSG_ERR, p_app_name, WTERMSIG(status), ats::toStr(rs).c_str(), MSG_END);
					}
					else
					{
						fprintf(stderr, "%s%s: iptables returned status 0x%08X. failed to apply rule <%s>%s\n",
							MSG_ERR, p_app_name, status, ats::toStr(rs).c_str(), MSG_END);
					}

				}

			}

		}

		++i;
	}
}

int main(int argc, char* argv[])
{
	RuleMap rm;
	ReservationMap rl;

	{
		RuleList rule;
		ReservationList res;

		load_rules(argv[0], rule);
		load_reservations(argv[0], res);

		ats::StringMap iface;
		get_ip(iface);

		simplify_rules(argv[0], rule, iface);
		simplify_reservations(argv[0], res, iface);

		to_RuleMap(rm, rule);
		to_ReservationMap(rl, res);
	}

	apply_rules(argv[0], rm);
	apply_reservations(argv[0], rl);
	return 0;
}
