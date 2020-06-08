#pragma once
#include <list>

#include "AdminCommandContext.h"
#include "db-monitor.h"

extern int g_dbg;

#define APCLI_IFNAME "apcli0"
#define AP_IFNAME "apcli0"
#define MAX_SURVEY_TABLE 64
#ifndef SIOCDEVPRIVATE
#define SIOCDEVPRIVATE 0x8BE0
#define SIOCIWFIRSTPRIV SIOCDEVPRIVATE
#endif

#define RT_PRIV_IOCTL (SIOCIWFIRSTPRIV + 0x01)
#define RTPRIV_IOCTL_SET (SIOCIWFIRSTPRIV + 0x02)
#define RTPRIV_IOCTL_BBP (SIOCIWFIRSTPRIV + 0x03)
#define RTPRIV_IOCTL_MAC (SIOCIWFIRSTPRIV + 0x05)
#define RTPRIV_IOCTL_E2P (SIOCIWFIRSTPRIV + 0x07)
#define RTPRIV_IOCTL_STATISTICS (SIOCIWFIRSTPRIV + 0x09)
#define RTPRIV_IOCTL_ADD_PMKID_CACHE (SIOCIWFIRSTPRIV + 0x0A)
#define RTPRIV_IOCTL_RADIUS_DATA (SIOCIWFIRSTPRIV + 0x0C)
#define RTPRIV_IOCTL_GSITESURVEY (SIOCIWFIRSTPRIV + 0x0D)
#define RTPRIV_IOCTL_ADD_WPA_KEY (SIOCIWFIRSTPRIV + 0x0E)
#define RTPRIV_IOCTL_GET_MAC_TABLE (SIOCIWFIRSTPRIV + 0x0F)

#define m_db_name "wifi_db"
#define sendsql(dbname) \
const ats::String& err = \
	m_db->query(dbname, buf);\
	if(!err.empty())\
	{\
		ats_logf(ATSLOG(0), "%s,%d: Error: %s", __FILE__, __LINE__,err.c_str());\
		return false;\
	}

typedef struct _survey_table
{
	ats::String channel;
	ats::String ssid;
	ats::String bssid;
	ats::String enc;
	ats::String crypto;
} survey_table;

typedef struct
{
	survey_table * tb;
	ats::String password;
} assoc_essid;

class MyData;

// Description: A "wifiSecurityPair" is a <SSID, Password> pair.
typedef std::pair <ats::String, ats::String> wifiSecurityPair;

typedef std::list <wifiSecurityPair> wifiSecurityList;
typedef std::vector <survey_table> stableList;

struct wsfind: public std::binary_function< wifiSecurityPair, ats::String, bool >
{
	bool operator () (const wifiSecurityPair &pair, const ats::String &name) const
	{
		return !pair.first.compare(name);
	}
};

class MyData
{
public:
	AdminCommandMap m_command;
	ats::StringMap m_config;

	MyData();

	void start_server();
	bool get_site_survey();

	//load presaved username and password from database.
	bool loadConfigList();
	bool openWifiDB();
	ats::String check_assoc();
	void wifi_start(const ats::String& ifname, const assoc_essid& ae);
	assoc_essid getassocEssid();
	bool isDBempty(){return m_db_empty;}
	void pushbackItem(const ats::String& name);

	void setversion(const ats::String& version){m_version = version;}

	db_monitor::DBMonitorContext& DB()
	{
		return *m_db;
	}

	const db_monitor::DBMonitorContext& DB() const
	{
		return *m_db;
	}

	void start_LED_monitor();
	ats::String getWirelessName();

protected:
	void iwpriv(const ats::String& name, const ats::String& key, const ats::String& val);
	survey_table * wifi_find_ap(const ats::String& name);

private:
	ServerData m_command_server;
	ats::String m_version;
	stableList m_stable;

	db_monitor::DBMonitorContext* m_db;
	wifiSecurityList m_wsl;
	bool m_db_empty;

	friend void* LED_thread(void*);
	pthread_t m_LED_thread;

	// Description: Helper function for "wifi_start" that sends RaLink AP driver compatible commands.
	//	The AP driver supports Access-Point-Mode and Client-Mode at the same time.
	//
	// Return: 0 is returned on success, and all other values are error codes (not currently specified).
	int wifi_start_ap(const ats::String& ifname, const assoc_essid& ae);

	// Description: Helper function for "wifi_start" that sends RaLink STA driver compatible commands.
	//	The STA driver only supports Client-Mode.
	//
	// Return: 0 is returned on success, and all other values are error codes (not currently specified).
	int wifi_start_sta(const ats::String& ifname, const assoc_essid& ae);
};
