#include <stdio.h>
#include <pthread.h>

#include "skybase_roadinfo.h"

#include "NMEA_DATA.h"
#include "NMEA.h"
#include "NMEA_Client.h"
#include "ats-common.h"
#include "atslogger.h"
#include "socket_interface.h"
#include "command_line_parser.h"

static pthread_mutex_t g_mutex;

static ATSLogger g_log;

static SkyBase::RoadInfo g_ri;
static SkyBase::RoadInfoStatus g_ris = SkyBase::RoadInfoBadCoords;
static SkyBase::RoadInfoFinder* g_rif = 0;
static int g_stable_readings = 0;
static double g_lat = 0.0;
static double g_lon = 0.0;

static double g_dbg_lat;
static double g_dbg_lon;
static bool g_dbg_coords = false;
static bool g_trace = false;

static void display_road_info(ats::String& p_o, const SkyBase::RoadInfoStatus& p_ris, const SkyBase::RoadInfo& p_ri, double p_lat, double p_lon, int p_stable_readings)
{
	ats_sprintf(&p_o, "lat=%lf lon=%lf status=%d name=\"%s\" type=\"%s\" speed=\"%s\" stable_readings=%d",
		p_lat, p_lon, p_ris, p_ri.m_road_name.c_str(), p_ri.m_road_type.c_str(), p_ri.m_road_speed_limit.c_str(), p_stable_readings);
}

static void display_road_info(const SkyBase::RoadInfoStatus& p_ris, const SkyBase::RoadInfo& p_ri, double p_lat, double p_lon, int p_stable_readings)
{
	ats::String s;
	display_road_info(s, p_ris, p_ri, p_lat, p_lon, p_stable_readings);
	printf("%s\n", s.c_str());
}

static SkyBase::RoadInfoStatus update_road_info(SkyBase::RoadInfo& p_ri, double p_lat, double p_lon)
{
	const SkyBase::RoadInfoStatus& ris = g_rif->GetRoadInfo(p_ri, p_lon, p_lat);

	pthread_mutex_lock(&g_mutex);

	if((g_ris != ris) || (g_ri.m_road_speed_limit != p_ri.m_road_speed_limit))
	{
		g_stable_readings = 1;
	}
	else
	{
		++g_stable_readings;
	}

	g_ris = ris;
	g_ri = p_ri;
	g_lat = p_lat;
	g_lon = p_lon;
	pthread_mutex_unlock(&g_mutex);

	return ris;
}

static void parse_lat_lon(double& p_lat, double& p_lon, const ats::StringMap& p_arg)
{
	p_lat = 0.0;
	p_lon = 0.0;

	if(p_arg.has_key("lat") || p_arg.has_key("lon"))
	{
		p_lat = strtod(p_arg.get("lat").c_str(), 0);
		p_lon = strtod(p_arg.get("lon").c_str(), 0);
	}
	else
	{

		if(!p_arg.empty())
		{
			ats::StringList l;
			ats::split(l, ((p_arg.begin())->first).c_str(), ",");

			if(l.size() >= 2)
			{
				p_lat = strtod(l[0].c_str(), 0);
				p_lon = strtod(l[1].c_str(), 0);
			}

		}

	}

}

static void* client_command_server(void* p)
{
	ClientData* cd = (ClientData*)p;

	bool command_too_long = false;
	ats::String cmd;

	const size_t max_command_length = 1024;

	ClientDataCache cdc;
	init_ClientDataCache(&cdc);

	for(;;)
	{
		char ebuf[1024];
		const int c = client_getc_cached(cd, ebuf, sizeof(ebuf), &cdc);

		if(c < 0)
		{
			if(c != -ENODATA) ats_logf(ATSLOG(3), "%s,%d: %s", __FILE__, __LINE__, ebuf);
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
			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "xml <devResp error=\"command is too long\"></devResp>\r");
			continue;
		}

		CommandBuffer cb;
		init_CommandBuffer(&cb);
		const char *err;

		if((err = gen_arg_list(cmd.c_str(), cmd.length(), &cb)))
		{
			ats_logf(ATSLOG(0), "%s,%d: gen_arg_list failed (%s)", __FILE__, __LINE__, err);
			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "xml <devResp error=\"gen_arg_list failed\">%s</devResp>\r", err);
			cmd.clear();
			continue;
		}
		cmd.clear();

		if(cb.m_argc <= 0)
		{
			continue;
		}

		const ats::String cmd(cb.m_argv[0]);
		ats::StringMap args;
		args.from_args(cb.m_argc - 1, cb.m_argv + 1);

		if("get" == cmd)
		{
			pthread_mutex_lock(&g_mutex);
			const int stable_readings = g_stable_readings;
			const SkyBase::RoadInfo ri(g_ri);
			const SkyBase::RoadInfoStatus ris(g_ris);
			const double lat = g_lat;
			const double lon = g_lon;
			pthread_mutex_unlock(&g_mutex);

			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "%d %d %ld\n\r", (ris == SkyBase::RoadInfoValid) ? 1 : 0, stable_readings, strtol(ri.m_road_speed_limit.c_str(), 0, 0));

			if(g_trace)
			{
				ats::String s;
				display_road_info(s, ris, ri, lat, lon, stable_readings);
				ats_logf(ATSLOG(0), "%s", s.c_str());
			}

		}
		else if("get_all" == cmd)
		{
			pthread_mutex_lock(&g_mutex);
			const int stable_readings = g_stable_readings;
			const SkyBase::RoadInfo ri(g_ri);
			const SkyBase::RoadInfoStatus ris(g_ris);
			const double lat = g_lat;
			const double lon = g_lon;
			pthread_mutex_unlock(&g_mutex);

			ats::String s;
			display_road_info(s, ris, ri, lat, lon, stable_readings);
			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "get: %s\n\r", s.c_str());
		}
		else if("get_at" == cmd)
		{
			SkyBase::RoadInfo ri;
			double lat;
			double lon;
			parse_lat_lon(lat, lon, args);
			const SkyBase::RoadInfoStatus& ris = g_rif->GetRoadInfo(ri, lon, lat);
			ats::String s;
			display_road_info(s, ris, ri, lat, lon, 1);
			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "get: %s\n\r", s.c_str());
		}
		else if("set_dbg_coords" == cmd)
		{
			double lat;
			double lon;
			parse_lat_lon(lat, lon, args);
			pthread_mutex_lock(&g_mutex);
			g_dbg_coords = true;
			g_dbg_lat = lat;
			g_dbg_lon = lon;
			pthread_mutex_unlock(&g_mutex);
			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "%s: ok\n\tlat=%lf, lon=%lf\n\r", cmd.c_str(), lat, lon);
		}
		else if("unset_dbg_coords" == cmd)
		{
			pthread_mutex_lock(&g_mutex);
			g_dbg_coords = false;
			pthread_mutex_unlock(&g_mutex);
			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "%s: ok\n\r", cmd.c_str());
		}
		else if("trace" == cmd)
		{

			if(cb.m_argc >= 2)
			{
				g_trace = ats::get_bool(cb.m_argv[1]);
			}

			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "g_trace=%s\n\r", g_trace ? "ON" : "OFF");
		}
		else
		{
			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "invalid command \"%s\"\n\r", cmd.c_str());
		}

	}
	return 0;
}

int main(int argc, char* argv[])
{
	NMEA_Client nmea_client;

	ats::StringMap config;
	config.set("appname", "SkyBase");
	config.set("run-as-user", "applet");
	config.from_args(argc - 1, argv + 1);

	if(ats::su(config.get("run-as-user")))
	{
		ats_logf(ATSLOG(0), "%s,%d: Could not become user \"%s\": ERR (%d): %s", __FILE__, __LINE__, config.get("run-as-user").c_str(), errno, strerror(errno));
		return 1;
	}

	const ats::String appname(config.get("appname"));
	pthread_mutex_init(&g_mutex, 0);
	g_log.open_testdata(appname);
	g_log.set_global_logger(&g_log);

	if((config.get("roadfile")).empty())
	{
		config.set("roadfile", "/home/applet/.skybase/mnt/roadspeeds.ctm1");
	}

	const ats::String& road_info_file = config.get("roadfile");

	if(!ats::file_exists(road_info_file))
	{
		ats_logf(ATSLOG(0), "%s,%d: Road info file \"%s\" does not exist", __FILE__, __LINE__, road_info_file.c_str());
		return 1;
	}

	SkyBase::RoadInfo ri;
	SkyBase::RoadInfoFinder rif(road_info_file.c_str());
	g_rif = &rif;

	if(config.has_key("lat") && config.has_key("lon"))
	{
		const double lat = strtod(config.get("lat").c_str(), 0);
		const double lon = strtod(config.get("lon").c_str(), 0);
		const SkyBase::RoadInfoStatus ris(update_road_info(ri, lat, lon));
		display_road_info(ris, ri, lat, lon, 1);
		return 0;
	}

	g_trace = config.get_bool("trace");

	ServerData sd;
	init_ServerData(&sd, 8);
	sd.m_cs = client_command_server;
	::start_redstone_ud_server(&sd, appname.c_str(), 1);
	signal_app_ready(appname.c_str());

	const int update_delay = config.has_key("update_delay") ? strtol(config.get("update_delay").c_str(), 0, 0) : 1;

	for(;;)
	{
		double lat;
		double lon;
		pthread_mutex_lock(&g_mutex);

		if(g_dbg_coords)
		{
			lon = g_dbg_lon;
			lat = g_dbg_lat;
			pthread_mutex_unlock(&g_mutex);
		}
		else
		{
			pthread_mutex_unlock(&g_mutex);
			lon = nmea_client.Lon();
			lat = nmea_client.Lat();
		}

		update_road_info(ri, lat, lon);
		sleep(update_delay);
	}

	return 0;
}
