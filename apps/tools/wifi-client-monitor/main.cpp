// Description: Monitors the WiFi access point for client connection activity. When
//	client connection activity ends, the program is terminated. So this program
//	returning means that there is no "detectable" client WiFi activity (regardless
//	if the reason is an error, or no WiFi activity).
//
//	The return status is zero on success, and non-zero on error.
//
// Author: Amour Hassan (Amour.Hassan@gps1.com)
// Date: May 15, 2014
// Copyright 2014 AbsoluteGemini
//
// Usage: ./wifi-client-monitor
#include <fstream>

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
#include <sys/inotify.h>

#include "pcap.h"

#include "SocketQuery.h"
#include "ats-common.h"
#include "atslogger.h"
#include "ConfigDB.h"
#include "timer-event.h"

// Structures and some code taken from: http://www.tcpdump.org/pcap.html
//
//This document is Copyright 2002 Tim Carstens. All rights reserved. Redistribution and use, with or without modification, are permitted provided that the following conditions are met:
//
//    Redistribution must retain the above copyright notice and this list of conditions.
//    The name of Tim Carstens may not be used to endorse or promote products derived from this document without specific prior written permission.
//
// /* Insert 'wh00t' for the BSD license here */
//
// ethernet headers are always exactly 14 bytes
#define SIZE_ETHERNET 14

// Ethernet addresses are 6 bytes
#define ETHER_ADDR_LEN	6

#define IP_HL(ip) (((ip)->ip_vhl) & 0x0f)

// XXX: The structures defined here ("sniff_ethernet" and "sniff_ip") must not contain
//	any padding.
#pragma pack(push, 1)
// Ethernet header
struct sniff_ethernet
{
	u_char ether_dhost[ETHER_ADDR_LEN]; // Destination host address
	u_char ether_shost[ETHER_ADDR_LEN]; // Source host address
	u_short ether_type; // IP? ARP? RARP? etc
};

// IP header
struct sniff_ip
{
	u_char ip_vhl;            // version << 4 | header length >> 2
	u_char ip_tos;            // type of service
	u_short ip_len;           // total length
	u_short ip_id;            // identification
	u_short ip_off;           // fragment offset field
	#define IP_RF 0x8000      // reserved fragment flag
	#define IP_DF 0x4000      // dont fragment flag
	#define IP_MF 0x2000      // more fragments flag
	#define IP_OFFMASK 0x1fff // mask for fragmenting bits
	u_char ip_ttl;            // time to live
	u_char ip_p;              // protocol
	u_short ip_sum;           // checksum
	struct in_addr ip_src;    // source IP address
	struct in_addr ip_dst;    // destination IP address
};
#pragma pack(pop)

static ATSLogger g_log;
static ats::StringMap g_arg;
static int g_debug = 0;
static const ats::String g_app_name("wifi-client-monitor");

static pthread_t g_wifi_client_max_keep_alive_thread;
static int g_keep_alive_seconds;

// Description: Watchdog that monitors the total running time for WiFi clients. Once the timeout
//	expires, the program is terminated.
//
// Return: Does not return as "exit" is called.
static void* wifi_client_max_keep_alive_monitor(void*)
{
	ats_logf(ATSLOG(0), "%s: Hard cutoff of %d seconds starts now", __FUNCTION__, g_keep_alive_seconds);
	sleep(g_keep_alive_seconds);
	ats_logf(ATSLOG(0), "%s: Timeout of %d seconds has expired, hard cutoff active, shutting down", __FUNCTION__, g_keep_alive_seconds);
	exit(0);
	return 0;
}

class WiFiClientAliveTimeoutMonitor
{
public:
	pthread_t m_wifi_client_alive_timeout_thread;
	int m_wifi_client_alive_timeout_seconds;
	ats::TimerEvent m_t;
};

// Description: Watchdog that monitors WiFi client activity. The program is terminated when there is
//	no WiFi client activity for the specified timeout.
//
// Return: Does not return as "exit" is called.
static void* wifi_client_alive_timeout_monitor(void* p)
{
	WiFiClientAliveTimeoutMonitor& w = *((WiFiClientAliveTimeoutMonitor*)p);

	for(;;)
	{
		w.m_t.start_timer_and_wait(w.m_wifi_client_alive_timeout_seconds, 0);

		if(w.m_t.is_cancelled())
		{
			continue;
		}

		break;
	}

	ats_logf(ATSLOG(0), "%s: No client activity for %d seconds", __FUNCTION__, w.m_wifi_client_alive_timeout_seconds);
	exit(0);
	return 0;
}

static pthread_t g_leases_thread;
static const char* g_leases_file = "/tmp/vstate/dhcp/dhcpd.leases";
static ats::StringMap g_leases;
static pthread_mutex_t g_leases_mutex;

static void lock_leases()
{
	pthread_mutex_lock(&g_leases_mutex);
}

static void unlock_leases()
{
	pthread_mutex_unlock(&g_leases_mutex);
}

// Description: Returns true if "p_mac" passes the "rough" test for a valid
//	Media Access Control (MAC) address format.
//
// Return: True is returned if "p_mac" is valid, and false is returned otherwise.
static bool mac_address_format_ok(const ats::String& p_mac)
{
	// MAC Address Format (table showing positions of colons):
	//                     1 1 1 1 1 1 1 |
	// 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 | Byte position
	// ----------------------------------+---------------
	// A A : B B : C C : D D : E E : F F | MAC Address
	const size_t valid_mac_length = 17;
	return (valid_mac_length == p_mac.length()) &&
		(':' == p_mac[2]) &&
		(':' == p_mac[5]) &&
		(':' == p_mac[8]) &&
		(':' == p_mac[11]) &&
		(':' == p_mac[14]);
}

// Description: Reads the current leases in the "dhcpd" leases file (for all interfaces) and stores them
//	in "g_leases". If there are no leases in the "dhcpd" leases file or some error occurred, then
//	"g_leases" will be empty (or will contain a short count of leases).
//
// ATS FIXME: Limit leases stored to just those for the WiFi interface.
//
// XXX: The "dhcpd.leases" file is not locked, so it is possible for this function to read a partial
//	lease (there is a race with "dhcpd" writing the lease and this function reading it). Since there
//	is no other solution at present, this function doesn't care if it reads a partial lease. This
//	function will make a "best effort" attempt to detect a partial lease, filter it out, and wait
//	for the next "inotify" event when the file contains more data.
//
//	Reading partial leases will not adversely affect the processing of this function, nor its ability
//	to detect new leases.
static void update_leases()
{
	lock_leases();
	ats_logf(ATSLOG(0), "Updating leases");
	g_leases.clear();

	FILE* f = fopen("/tmp/vstate/dhcp/dhcpd.leases", "r");

	if(!f)
	{
		unlock_leases();
		return;
	}

	ats::ReadDataCache_FILE rdc(f);

	const ats::String lease("lease ");
	const ats::String hw_eth("hardware ethernet ");

	for(;;)
	{
		ats::String line;

		if(ats::get_file_line(line, rdc, 1) < 0)
		{
			break;
		}

		if(0 == lease.compare(0, lease.length(), line, 0, lease.length()))
		{
			size_t i = line.rfind(" {");
			ats::String ip;
			ats::String mac;

			if(i != ats::String::npos)
			{
				ip = line.substr(lease.length(), i - lease.length());
			}

			for(;;)
			{

				if(ats::get_file_line(line, rdc, 1) < 0)
				{
					break;
				}

				if("}" == line)
				{

					if(mac_address_format_ok(mac))
					{
						g_leases.set(mac, ip);
					}

					break;
				}

				size_t i = line.find(hw_eth);

				if(i != ats::String::npos)
				{
					const int semi_colon = 1;
					mac = line.substr(i + hw_eth.length(), line.length() - (i + hw_eth.length() + semi_colon));
				}

			}

		}

	}

	fclose(f);
	unlock_leases();
}

// Description: Watches the "dhcpd" leases file for changes. When a change occurs, the leases
//	are re-read.
//
// Return: 0 is returned on error and monitoring of the lease file ends. This function does not
//	return if there are no errors.
static void* leases_monitor(void*)
{
	const int fd = inotify_init();

	if(fd < 0)
	{
		ats_logf(ATSLOG(0), "%s: inotify_init failed, not watching for lease changes: (%d) %s", __FUNCTION__, errno, strerror(errno));
		return 0;
	}

	// XXX: According to the "dhcpd.leases" manual, "dhcpd" will open but not close the leases file
	//	on a regular/timely basis (will only close if file gets "too big"). Therefore the file must
	//	be watched for modifications (in order to pick up small changes) in addition to any
	//	other watch condition.
	const int wd = inotify_add_watch(fd, g_leases_file, IN_CLOSE_WRITE | IN_MODIFY);

	if(wd < 0)
	{
		ats_logf(ATSLOG(0), "%s: inotify_add_watch failed: (%d) %s", __FUNCTION__, errno, strerror(errno));
		return 0;
	}

	for(;;)
	{
		char buf[sizeof(struct inotify_event)];
		const ssize_t nread = read(fd, buf, sizeof(buf));

		if(nread <= 0)
		{

			if(!nread)
			{
				ats_logf(ATSLOG(0), "%s: read ended", __FUNCTION__);
			}
			else if(EINTR == errno)
			{
				continue;
			}
			else
			{
				ats_logf(ATSLOG(0), "%s: read failed: (%d) %s", __FUNCTION__, errno, strerror(errno));
			}

			break;
		}

		update_leases();
	}

	return 0;
}

int main(int argc, char* argv[])
{
	// Only one instance (per boot) of this application is allowed. Once it has
	// been ran, it cannot be ran again.
	{
		const char* flag = "/tmp/flags/.wifi-client-monitor-started";

		if(mkdir(flag, 0755))
		{
			return 1;
		}

	}

	const int pid = fork();

	if(pid)
	{
		// Tell power-monitor to stay awake for as long as the child process is alive.

		// ATS FIXME: Use a TRULink Unix Domain Socket (more resource/time efficient) for Power-Monitor.
		const int power_monitor_port = 41009;
		ats::SocketQuery sq("127.0.0.1", power_monitor_port);
		sq.query("set_work key=\"" + g_app_name + "\"\r");
		waitpid(pid, 0, 0);
		sq.query("unset_work key=\"" + g_app_name + "\"\r");
		return 0;
	}

	g_log.set_global_logger(&g_log);

	if(argc >= 3)
	{
		g_arg.from_args(argc - 2, argv + 2);
	}

	g_debug = g_arg.get_int("debug");
	g_log.set_level(g_debug);
        g_log.open_testdata(g_app_name);

	db_monitor::ConfigDB db;
	db.open_db_config();

	pthread_mutex_init(&g_leases_mutex, 0);
	update_leases();
	pthread_create(&g_leases_thread, 0, leases_monitor, 0);

	const int two_hours_in_minutes = 120;
	g_keep_alive_seconds = db.GetInt("system", "WiFiClientMaxKeepAliveMinutes", two_hours_in_minutes) * 60;

	if(g_keep_alive_seconds > 0)
	{
		pthread_create(&g_wifi_client_max_keep_alive_thread, 0, wifi_client_max_keep_alive_monitor, 0);
	}

	static WiFiClientAliveTimeoutMonitor client_alive_monitor;
	const int five_minutes = 5;
	client_alive_monitor.m_wifi_client_alive_timeout_seconds = db.GetInt("system", "WiFiClientAliveTimeoutMinutes", five_minutes) * 60;

	if(client_alive_monitor.m_wifi_client_alive_timeout_seconds == 0 || g_keep_alive_seconds == 0)
	{
		ats_logf(ATSLOG(0), "Timeout is OFF -  WiFi does not keep the system awake ");
		return 0;
	}

	ats_logf(ATSLOG(0), "clients considered dead after %d seconds of no recv packets", client_alive_monitor.m_wifi_client_alive_timeout_seconds);
	pthread_create(&(client_alive_monitor.m_wifi_client_alive_timeout_thread), 0, wifi_client_alive_timeout_monitor, &client_alive_monitor);

	char ebuf[PCAP_ERRBUF_SIZE];
	const char* device = "ra0";
	const int non_promiscuous_mode = 0;
	pcap_t* handle = pcap_open_live(device, BUFSIZ, non_promiscuous_mode, client_alive_monitor.m_wifi_client_alive_timeout_seconds * 1000, ebuf);

	if(handle == NULL)
	{
		 fprintf(stderr, "Couldn't open device %s: %s\n", device, ebuf);
		 return 1;
	}

	struct bpf_program fp;
	const char* filter = "";

	if(-1 == pcap_compile(handle, &fp, filter, 1, 0xFFFFFF00))
	{
		fprintf(stderr, "pcap_compile(%s) failed: %s", filter, pcap_geterr(handle));
		return 1;
	}

	if(-1 == pcap_setfilter(handle, &fp))
	{
		fprintf(stderr, "pcap_setfilter(%s) failed: %s", filter, pcap_geterr(handle));
		return 1;
	}

	for(;;)
	{
		struct pcap_pkthdr* header;
		const u_char* packet;
		const int ret = pcap_next_ex(handle, &header, &packet);

		if(ret < 0)
		{
			fprintf(stderr, "pcap_next failed: %s\n", pcap_geterr(handle));
			return 1;
		}

		const struct sniff_ethernet* eth = (struct sniff_ethernet*)packet;
		const struct sniff_ip* ip = (struct sniff_ip*)(packet + SIZE_ETHERNET);
		const u_int size_ip = IP_HL(ip) * 4;

		if(size_ip < 20)
		{
			continue;
		}

		char mac[] = "11:22:33:44:55:66";
		snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
			eth->ether_shost[0],
			eth->ether_shost[1],
			eth->ether_shost[2],
			eth->ether_shost[3],
			eth->ether_shost[4],
			eth->ether_shost[5]
		);
		mac[sizeof(mac) - 1] = '\0';

		lock_leases();

		if(g_leases.find(mac) != g_leases.end())
		{
			unlock_leases();
			const bool cancel = true;
			client_alive_monitor.m_t.stop_timer(cancel);
		}
		else
		{
			unlock_leases();
		}

	}

	return 1; // Never reached
}
