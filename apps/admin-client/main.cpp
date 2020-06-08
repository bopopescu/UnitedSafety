#include <iostream>
#include <algorithm>
#include <set>

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <pwd.h>
#include <sys/wait.h>

#include "ConfigDB.h"
#include "ats-common.h"
#include "atslogger.h"
#include "socket_interface.h"
#include "SocketInterfaceResponse.h"
#include "command_line_parser.h"
#include "NMEA_Client.h"
#include "RedStone_IPC.h"
#include "geoconst.h"
#include "AFS_Timer.h"

#define to_base64(P_s) ats::base64_encode(P_s)
#define from_base64(P_s) ats::base64_decode(P_s)

ATSLogger g_log;

static const ats::String g_app_name("admin-client");
int g_dbg = 0;

class MyData;

class AdminCommandContext
{
public:
	AdminCommandContext(MyData& p_data, ClientSocket& p_socket)
	{
		m_data = &p_data;
		m_socket = &p_socket;
		m_cd = 0;
	}

	AdminCommandContext(MyData& p_data, ClientData& p_cd)
	{
		m_data = &p_data;
		m_socket = 0;
		m_cd = &p_cd;
	}

	int get_sockfd() const
	{
		return (m_cd ? m_cd->m_sockfd : m_socket->m_fd);
	}

	MyData* m_data;

private:
	ClientSocket* m_socket;
	ClientData* m_cd;
};

typedef int (*AdminCommand)(AdminCommandContext&, int p_argc, char* p_argv[]);
typedef std::map <const ats::String, AdminCommand> AdminCommandMap;
typedef std::pair <const ats::String, AdminCommand> AdminCommandPair;

class MyData
{
public:
	ats::StringMap m_config;

	// Description: General RedStone commands.
	AdminCommandMap m_command;

	// Description: RedStone commands for system administrators.
	AdminCommandMap m_admin_cmd;

	// Description: Stores commands only available through Unix Domain sockets.
	//	Since Unix Domain sockets cannot be accessed from outside of the local system,
	//	and user/group access is enforced, the commands contained in here are more privileged.
	//
	//	Any process in the "wheel" group will have access to these commands.
	AdminCommandMap m_wheel_cmd;

	// Description: RedStone commands for developers.
	AdminCommandMap m_dev_cmd;

	MyData();

	void init_wheel_data();
	void init_data();

	void lock() const
	{
		pthread_mutex_lock(m_mutex);
	}

	void unlock() const
	{
		pthread_mutex_unlock(m_mutex);
	}

	sem_t* m_gsm_sem;

	NMEA_Client& getNmeaClient() { return nmea_client;}

	// Description: Notifies the user of Local Configuration Manager activity.
	//
	// Operation:
	//	Blink the activity LED (which will blink for 5 seconds) and do not request another
	//	blink until 5 seconds has passed. This means that an action/event occuring just before
	//	the 5 seconds expire will not cause an aditional 5 seconds from that point.
	//
	//	The system is also kept alive for 2 minutes every time the activity LED blink command
	//	is issued.
	void refresh_keep_awake();

private:
	pthread_mutex_t* m_mutex;
	NMEA_Client	nmea_client;

	AFS_Timer m_resetTimer;
	int m_blink_refresh_seconds;
	int m_set_work_expire_seconds;
	bool m_first_time;
};


int ac_speedlimit(AdminCommandContext& p_acc, int p_argc, char* p_argv[])
{
	ats_logf(ATSLOG_INFO, "%s,%d: speedlimit=%s", __FILE__, __LINE__, (p_argc > 0) ? p_argv[0] : "<nil>");
	return 0;
}

int ac_speedlimitgrace(AdminCommandContext& p_acc, int p_argc, char* p_argv[])
{
	ats_logf(ATSLOG_INFO, "%s,%d: speedlimitgrace=%s", __FILE__, __LINE__, (p_argc > 0) ? p_argv[0] : "<nil>");
	return 0;
}

int ac_version(AdminCommandContext& p_acc, int p_argc, char* p_argv[])
{
	ats::String version;
	const int ret = ats::read_file("/version", version);
	if(ret)
	{
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "<err>failed to read version information</err>");
	}
	else
	{
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "<version>%s</version>", version.c_str());
	}
	return 0;
}

int ac_sn(AdminCommandContext& p_acc, int p_argc, char* p_argv[])
{
        db_monitor::ConfigDB db;
        const ats::String& version = db.GetValue("RedStone", "IMEI");

	send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "<sn>%s</sn>", ats::rtrim(version, " \t\n\r").c_str());
	return 0;
}

static ats::String query_system(const ats::String& p_cmd)
{
	std::stringstream s;
	ats::system(p_cmd, &s);
	return ats::rtrim(s.str(), "\n");
}

static bool set_micro_value(
	const char* p_label,
	const ats::String& p_val,
	const ats::String& p_get_cmd,
	const ats::String& p_set_cmd,
	int p_delay_ms)
{
	const int max_retry = 3;
	const int tid = ats::gettid();
	int i_attempt;

	for(i_attempt = 0;; ++i_attempt)
	{
		const ats::String& v = query_system(p_get_cmd);

		if(v == p_val)
		{
			break;
		}

		if(i_attempt >= max_retry)
		{
			ats_logf(ATSLOG_ERROR, "[%d] %s(%s): \"%s\" failed", tid, __FUNCTION__, p_label, p_set_cmd.c_str());
			return false;
		}

		ats_logf(ATSLOG_DEBUG, "[%d] %s(%s) [attempt %d]: \"%s\" to \"%s\"", tid, __FUNCTION__, p_label, i_attempt + 1, v.c_str(), p_val.c_str());
		ats::system(p_set_cmd);

		if(p_delay_ms > 0)
		{
			usleep(p_delay_ms * 1000);
		}

	}

	if(i_attempt)
	{
		ats_logf(ATSLOG_INFO, "[%d] %s(%s): success(%s)", tid, __FUNCTION__, p_label, p_val.c_str());
	}

	return true;
}

// Description: Function to check current db-config configuration of wakeup signals against
//	the values in the micro. If they are different the db-config values are written to the micro
//
//	Dave Huff - June 2014
//
// Return: 0 is returned if the micro was programmed properly and non-zero is returned otherwise.
int ac_wakeup(AdminCommandContext& p_acc, int p_argc, char* p_argv[])
{
	db_monitor::ConfigDB db;
	const ats::String& wakeup_mask = db.GetValue("system", "wakeup-mask");
	const ats::String& CriticalVoltage = db.GetValue("wakeup", "CriticalVoltage");
	const ats::String& WakeupVoltage = db.GetValue("wakeup", "WakeupVoltage");
	// TODO: Complete implementation for accelerometer.
	#if 0
	const ats::String& AccelTriggerG = db.GetValue("wakeup", "AccelTriggerG");
	#endif
	db.disconnect();

	const int micro_program_delay_ms = 100;
	bool failed = false;
	failed |= !set_micro_value("Wakeup Mask", wakeup_mask, "get-wakeup-mask", "set-wakeup-mask", micro_program_delay_ms);
	failed |= !set_micro_value("Low Battery Voltage", CriticalVoltage, "get-low-batt-limit", "set-low-batt-limit " + CriticalVoltage, micro_program_delay_ms);
	failed |= !set_micro_value("Wakeup Voltage", WakeupVoltage, "get-batt-wakeup-threshold", "set-batt-wakeup-threshold " + WakeupVoltage, micro_program_delay_ms);
	// TODO: Complete implementation for accelerometer.
	#if 0
	failed |= !set_micro_value("Accelerometer Trigger", AccelTriggerG, "get-accel-g", "set-accel-g " + AccelTriggerG, micro_program_delay_ms);
	#endif

	return failed ? 1 : 0;
}

int ac_info(AdminCommandContext& p_acc, int p_argc, char* p_argv[])
{
	ats::String sn;
	ats::read_file("/mnt/nvram/rom/sn.txt", sn);
	sn = ats::rtrim(sn, " \t\n\r");

	ats::String imei;
	ats::read_file("/tmp/config/imei", imei);
	imei = ats::rtrim(imei, " \t\n\r");

	ats::String version;
	ats::read_file("/version", version);

	std::stringstream gps;
	ats::system("NMEA show=NMEA_Client", &gps);

	send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL,
		"<sn>%s</sn>"
		"<imei>%s</imei>"
		"<version>%s</version>"
		"<gps>%s</gps>"
		, sn.c_str()
		, imei.c_str()
		, (ats::base64_encode(version)).c_str()
		, (ats::base64_encode(gps.str())).c_str());

	return 0;
}

int ac_ping(AdminCommandContext& p_acc, int p_argc, char* p_argv[])
{
	//send ping message to Cantrak
	send_redstone_ud_msg("message-assembler", 0, "ping\r");
	return 0;
}

// execute the state command and return the output.
int ac_state(AdminCommandContext& p_acc, int p_argc, char* p_argv[])
{
	return 0;
}

int ac_debug(AdminCommandContext& p_acc, int p_argc, char* p_argv[])
{

	if(p_argc >= 2)
	{
		g_dbg = strtol(p_argv[1], 0, 0);
	}

	send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "debug=%d\n", g_dbg);
	return 0;
}

int ac_db(AdminCommandContext& p_acc, int p_argc, char* p_argv[])
{
	const char* prefix = p_argv[0];

	std::stringstream out;

	if(p_argc < 2)
	{
		ats_ssprintf(&out, "%s: error=%s\n\r", prefix, "No arguments/parameters given");
	}
	else
	{
		ats::StringMap s;
		s.from_args( p_argc - 1, p_argv + 1);

		const ats::String& cmd = s.get("cmd");
		db_monitor::ConfigDB db;

		if("set" == cmd)
		{
			const ats::String& err = db.set_config(from_base64(s.get("app_name")), from_base64(s.get("key")), from_base64(s.get("value")));

			if(err.empty())
			{
				ats_ssprintf(&out, "%s: ok\n", prefix);
			}
			else
			{
				ats_ssprintf(&out, "%s: error=%s\n", prefix, to_base64(err).c_str());
			}

		}
		else if("get" == cmd)
		{
			ats::String err;
			const bool has_key = s.has_key("key");
			const bool has_app_name = s.has_key("app_name");

			if(has_key && has_app_name)
			{
				err = db.Get(from_base64(s.get("app_name")), from_base64(s.get("key")));
			}
			else if(has_app_name)
			{
				err = db.Get(s.get("app_name"));
			}
			else
			{
				err = db.Get();
			}

			if(err.empty())
			{
				ats_ssprintf(&out, "%s: ok\n", prefix);
				size_t i;

				for(i = 0; i < db.Table().size(); ++i)
				{
					const db_monitor::ResultRow& row = db.Table()[i];
					size_t i;

					for(i = 0; i < row.size(); ++i)
					{
						ats_ssprintf(&out, "%s'%s'", i ? " " : "", to_base64(row[i]).c_str());
					}

					ats_ssprintf(&out, "\n");
				}

			}
			else
			{
				ats_ssprintf(&out, "%s: error=%s\n", prefix, to_base64(err).c_str());
			}

		}
		else if("unset" == cmd)
		{
			const bool has_app_name = s.has_key("app_name");
			const bool has_key = s.has_key("key");

			ats::String err;

			if(has_app_name)
			{

				if(has_key)
				{
					err = db.Unset(from_base64(s.get("app_name")), from_base64(s.get("key")));
				}
				else
				{
					err = db.Unset(from_base64(s.get("app_name")));
				}

			}
			else
			{
				err = db.Unset();
			}

			if(err.empty())
			{
				ats_ssprintf(&out, "%s: ok\n", prefix);
			}
			else
			{
				ats_ssprintf(&out, "%s: error=%s\n", prefix, to_base64(err).c_str());
			}

		}
		else if("open" == cmd)
		{
			const ats::String& err = db.open_db(from_base64(s.get("key")), from_base64(s.get("file")));

			if(err.empty())
			{
				ats_ssprintf(&out, "%s: ok\n", prefix);
			}
			else
			{
				ats_ssprintf(&out, "%s: error=%s\n", prefix, to_base64(err).c_str());
			}

		}
		else if("query" == cmd)
		{
			const ats::String& err = db.query(from_base64(s.get("key")), from_base64(s.get("query")));

			if(err.empty())
			{
				ats_ssprintf(&out, "%s: ok\n", prefix);

				size_t i;

				for(i = 0; i < db.Column().size(); ++i)
				{
					ats_ssprintf(&out, "%s%s", i ? " " : "", to_base64(db.Column()[i]).c_str());
				}

				ats_ssprintf(&out, "\n");

				for(i = 0; i < db.Table().size(); ++i)
				{
					const db_monitor::ResultRow& row = db.Table()[i];
					size_t i;

					for(i = 0; i < row.size(); ++i)
					{
						ats_ssprintf(&out, "%s'%s'", i ? " " : "", to_base64(row[i]).c_str());
					}

					ats_ssprintf(&out, "\n");
				}

			}
			else
			{
				ats_ssprintf(&out, "%s: error=%s\n", prefix, to_base64(err).c_str());
			}

		}
		else
		{
			ats_ssprintf(&out, "%s: error=%s\n", prefix, to_base64("No command given").c_str());
		}

	}

	send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s", to_base64(out.str()).c_str());
	return 0;
}

int ac_reboot(AdminCommandContext& p_acc, int p_argc, char* p_argv[])
{
	ats::system(
		"uptime >> /tmp/logdir/reboot.txt"
		"date >> /tmp/logdir/reboot.txt"
		";echo \"admin-client: User reboot requested\" >> /tmp/logdir/reboot.txt"
		";sync;reboot");
	return 0;
}

int ac_reconnect(AdminCommandContext& p_acc, int p_argc, char* p_argv[])
{

	if((p_argc > 1) && (ats::String("force") == p_argv[1]))
		ats::system("kill -9 `ps|grep ssh|grep -v grep|grep \"\\-R\"|sed 's/ root.*//'|sed 's/ //'`");
	else
		ats::system("kill `ps|grep ssh|grep -v grep|grep \"\\-R\"|sed 's/ root.*//'|sed 's/ //'`");
	send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "Reconnecting to the server...\n");
	return 0;
}

int ac_system(AdminCommandContext& p_acc, int p_argc, char* p_argv[])
{
	ats::String cmd;
	int i;
	for(i = 1; i < p_argc; ++i)
	{
		if(!cmd.empty()) cmd += ' ';
		cmd += p_argv[i];
	}

	if(cmd.empty())
	{
		return 0;
	}

	std::stringstream output;
	const int ret = ats::system(cmd, &output);

	if(ret)
	{
		ats_logf(ATSLOG_ERROR, "%s,%d:ERROR: ret=%d for cmd=%s", __FILE__, __LINE__, ret, cmd.c_str());
	}

	const ats::String& b64 = to_base64(output.str());

	send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s\n", b64.c_str());
	return 0;
}

int ac_exec(AdminCommandContext& p_acc, int p_argc, char* p_argv[])
{

	if(p_argc < 3)
	{
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "error\nusage: %s <command> <command name> [arg 1] ... [arg n]\n", p_argv[0]);
		return 1;
	}

	char** argv = new char*[p_argc];

	int i;

	for(i = 0; i < (p_argc - 1); ++i)
	{
		argv[i] = p_argv[i+1];
	}

	argv[p_argc - 1] = (char*)NULL;

	const int pid = fork();

	if(!pid)
	{
		ats::daemonize();
		execv(argv[0], argv);
		exit(1);
	}

	waitpid(pid, 0, 0);
	return 0;
}

int ac_web(AdminCommandContext& p_acc, int p_argc, char* p_argv[])
{
	// Split admin-client and web command parts.
	int command_start_index = -1;
	{
		int i;

		for(i = 1; i < p_argc; ++i)
		{

			if(!strcmp("--", p_argv[i]))
			{
				command_start_index = i + 1;
				break;
			}

		}

	}

	// Special admin-client commands and flags.
	{
		int i;

		for(i = 1; i < command_start_index; ++i)
		{
		}

	}

	ats::String cmd;
	int i;

	for(i = command_start_index; i < p_argc; ++i)
	{

		if(!cmd.empty())
		{
			cmd += ' ';
		}

		cmd += p_argv[i];
	}

	if(cmd.empty())
	{
		return 0;
	}

	int pipefd[2];

	if(pipe(pipefd) < 0)
	{
		ats_logf(ATSLOG_ERROR, "%s,%d:ERROR: pipe() failed: (%d) %s", __FILE__, __LINE__, errno, strerror(errno));
		return 1;
	}

	const int pid = fork();

	if(!pid)
	{
		struct passwd pwd;
		struct passwd* result;
		char buf[4096];

		const int err = getpwnam_r(
			"www-data",
			&pwd,
			buf,
			sizeof(buf),
			&result);

		if(err || (!result))
		{
			ats_logf(ATSLOG_ERROR, "%s,%d:ERROR: getpwnam_r failed: result=%p, (%d) %s", __FILE__, __LINE__, result, err, strerror(err));
			exit(1);
		}

		if(0 == getuid())
		{

			if(setgid(result->pw_gid))
			{
				ats_logf(ATSLOG_ERROR, "%s,%d:ERROR: Failed to change to gid %d", __FILE__, __LINE__, int(result->pw_gid));
				exit(1);
			}

			if(setuid(result->pw_uid))
			{
				ats_logf(ATSLOG_ERROR, "%s,%d:ERROR: Failed to change to uid %d", __FILE__, __LINE__, int(result->pw_uid));
				exit(1);
			}

		}

		close(pipefd[0]);

		std::stringstream output;
		const int ret = ats::system(cmd, &output);
		const ats::String& s = output.str();

		if(write(pipefd[1], s.c_str(), int(s.length())) < 0)
		{
			ats_logf(ATSLOG_ERROR, "%s,%d:ERROR: write to pipe failed: (%d) %s", __FILE__, __LINE__, errno, strerror(errno));
		}

		if(WIFEXITED(ret))
		{
			exit(WEXITSTATUS(ret));
		}

		exit(1);
	}

	// 1. Close pipe write-end.
	// 2. Wait for child to finish outputting data.
	close(pipefd[1]);

	if(pid < 0)
	{
		close(pipefd[0]);
		ats_logf(ATSLOG_ERROR, "%s,%d:ERROR: fork() failed: (%d) %s", __FILE__, __LINE__, errno, strerror(errno));
		return 1;
	}

	ats::String output;

	for(;;)
	{
		char c;
		const ssize_t nread = read(pipefd[0], &c, 1);

		if(nread < 0)
		{
			ats_logf(ATSLOG_ERROR, "%s,%d:ERROR: read failed: (%d) %s", __FILE__, __LINE__, errno, strerror(errno));
			break;
		}
		else if(!nread)
		{
			break;
		}

		output += c;
	}

	send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s\n", to_base64(output).c_str());
	close(pipefd[0]); // Close read-end
	return 0;
}

int valid_ipaddress(const char * str)
{
	if( INADDR_NONE == inet_addr(str))
		return 0;
	return 1;
}

template<size_t N>
bool in_array(const ats::String& st, const ats::String (& array)[N])
{
	return std::find(array, array + N, st) != array + N;
}

int ac_atcmd(AdminCommandContext& p_acc, int p_argc, char* p_argv[])
{
	ats::StringMap &config = p_acc.m_data->m_config;
	const char* atcmd = p_argv[0];

	if(p_argc < 2)
	{
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s: No arguments given\n", atcmd);
		return 1;
	}

	if ((!strcmp("cellimsi", p_argv[1])) && (p_argc == 3)) //atcmd cellimsi value
	{
		p_acc.m_data->lock();
		config.set("cellimsi",p_argv[2]);
		p_acc.m_data->unlock();
	}
	else if ((!strcmp("cellccid", p_argv[1])) && (p_argc == 3)) //atcmd cellccid value
	{
		p_acc.m_data->lock();
		config.set("cellccid",p_argv[2]);
		p_acc.m_data->unlock();
	}
	else if ((!strcmp("cellpnumber", p_argv[1])) && (p_argc == 3)) //atcmd cellpnumber value
	{
		p_acc.m_data->lock();
		config.set("cellpnumber",p_argv[2]);
		p_acc.m_data->unlock();
	}
	else if ((!strcmp("cellnetwork", p_argv[1])) && (p_argc == 3)) //atcmd cellnetwork value
	{
		p_acc.m_data->lock();
		config.set("cellnetwork",p_argv[2]);
		p_acc.m_data->unlock();
	}
	else if ((!strcmp("cellapn", p_argv[1])) && (p_argc == 3)) //atcmd cellapn value
	{
		p_acc.m_data->lock();
		config.set("cellapn",p_argv[2]);
		p_acc.m_data->unlock();
	}
	else
	{
		ats_logf(ATSLOG_ERROR, "%s,%d:ERROR: Invalid command", __FILE__, __LINE__);
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s: Invalid command\n", atcmd);
		return 1;
	}

	sem_post(p_acc.m_data->m_gsm_sem);
	send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s: ok\n", atcmd);
	return 0;
}

static void blinkActivityLED()
{
	send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink kick name=LCM_activity led=act timeout=10000 script=\"0,10000;1,100000\" \r");
}

void MyData::refresh_keep_awake()
{
	lock();

	if(m_first_time || (m_resetTimer.DiffTime() > m_blink_refresh_seconds))
	{
		m_first_time = false;
		m_resetTimer.SetTime();
		unlock();
		send_msg("127.0.0.1", 41009, 0, "set_work key=webAccess expire=%d\r", m_set_work_expire_seconds);
		blinkActivityLED();
	}
	else
	{
		unlock();
	}

}

static const ats::String g_authmode[] = {"OPEN", "SHARED", "WEPAUTO", "WPAPSK", "WPA2PSK", "WPAPSKWPA2PSK"};
static const ats::String g_encryptype[] = {"NONE", "WEP", "TKIP", "AES", "TKIPAES"};
static const ats::String g_interface[] = {"eth0","ra0","aplci0"};

static int dbset(AdminCommandContext& p_acc, const ats::String& p_app, const ats::String& p_key, const ats::String& p_value)
{
	const ats::String& app = ats::from_hex(p_app);
	const ats::String& key = ats::from_hex(p_key);
	const ats::String& value = ats::from_hex(p_value);

	p_acc.m_data->refresh_keep_awake();

	if ( !(app.empty() ||key.empty()))
	{
		db_monitor::ConfigDB db;
		const ats::String& err = db.Set(app, key, value);

		if(err.empty())
		{
			send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "dbset: ok\n\r");
			return 0;
		}

		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "dbset: error\n%s\n\r", err.c_str());
		return 1;
	}
	send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "dbset: error\nNot enough arguments.\n\r");
	return 1;


}

static int dbsetfile(AdminCommandContext& p_acc, const ats::String& p_app, const ats::String& p_key, const ats::String& p_file)
{
	const ats::String& app = ats::from_hex(p_app);
	const ats::String& key = ats::from_hex(p_key);
	const ats::String& file = ats::from_hex(p_file);

	p_acc.m_data->refresh_keep_awake();

	if ( !(app.empty() ||key.empty()))
	{
		db_monitor::ConfigDB db;
		std::ifstream fileStream(file.c_str());

		if(fileStream.fail())
		{
			send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "dbsetfile: error\nFile %s cannot be openned.\n\r", file.c_str());
			return 1;
		}

		ats::String value;

		fileStream.seekg(0, std::ios::end);
		value.reserve(fileStream.tellg());
		fileStream.seekg(0, std::ios::beg);

		value.assign((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());

		const ats::String& err = db.Set(app, key, value);

		if(err.empty())
		{
			send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "dbsetfile: ok\n\r");
			return 0;
		}

		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "dbsetfile: error\n%s\n\r", err.c_str());
		return 1;
	}
	send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "dbsetfile: error\nNot enough arguments.\n\r");
	return 1;

}
/****************************
* ATS-FIXME: dbget does not return "dbget:ok\n....\n\r" on success, which is demanded by the TRULink socket API 
* communication protocol(from design document).
****************************/
static int dbget(AdminCommandContext& p_acc, const ats::String& p_app, const ats::String& p_key)
{
	const ats::String& app = ats::from_hex(p_app);
	const ats::String& key = ats::from_hex(p_key);

	p_acc.m_data->refresh_keep_awake();
	db_monitor::ConfigDB db;
	const db_monitor::ResultTable& t = db.Table();

	if(!(app.empty() || key.empty()))
	{
		db.Get(app, key);

		for(size_t i = 0; i < t.size(); ++i)
		{
			const db_monitor::ResultRow& r = t[i];

			if(r.size() >= 2)
			{
				send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s\n",
					ats::to_hex(r[1]).c_str());
			}

		}

	}
	else if(!(app.empty()))
	{
		db.Get(app);

		for(size_t i = 0; i < t.size(); ++i)
		{
			const db_monitor::ResultRow& r = t[i];

			if(r.size() >= 4)
			{
				send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s %s\n",
					ats::to_hex(r[3]).c_str(),
					ats::to_hex(r[1]).c_str());
			}

		}

	}
	else
	{
		db.Get();

		for(size_t i = 0; i < t.size(); ++i)
		{
			const db_monitor::ResultRow& r = t[i];

			if(r.size() >= 5)
			{
				send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s %s %s\n",
					ats::to_hex(r[4]).c_str(),
					ats::to_hex(r[3]).c_str(),
					ats::to_hex(r[1]).c_str());
			}

		}

	}

	send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "\r");
	return 0;
}

static int h_ac_phpcmd(AdminCommandContext& p_acc, int p_argc, char* p_argv[])
{
	const char* phpcmd = p_argv[0];

	if(p_argc < 2)
	{
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s: No arguments given\r", phpcmd);
		return 1;
	}

	if(!(strcmp("dbget", p_argv[1]))) // dbget [app name] [key]
	{
		return dbget(p_acc, ((p_argc >= 3) ? p_argv[2] : ""), ((p_argc >= 4) ? p_argv[3] : ""));
	}
	else if((!strcmp("dbconfigget", p_argv[1]))  && (p_argc == 4 )) // dbconfigget <app name> <key>
	{
		p_acc.m_data->refresh_keep_awake();
		db_monitor::ConfigDB db;
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s\r", db.GetValue(p_argv[2], p_argv[3]).c_str());
		return 0;
	}

	ats::String cmd;

	// ATS FIXME: Replace "if/else" ladder.
	if((!strcmp("setip", p_argv[1])) && (p_argc == 5)) // setip <eth/wifiap/wificl> <add> <mask>
	{
		if(((!strcmp("eth", p_argv[2]))||(!strcmp("wifiap", p_argv[2]))|| (!strcmp("wificl", p_argv[2])) ) &&  (valid_ipaddress(p_argv[3])) && (valid_ipaddress(p_argv[4])))
		{
			const ats::String& inf = (!strcmp("eth", p_argv[2]))?g_interface[0]:((!strcmp("wifiap", p_argv[2]))?g_interface[1]:g_interface[2]);
			const ats::String& db =  (!strcmp("eth", p_argv[2]))?ats::String(" eth0addr "):(!strcmp("wifiap", p_argv[2]))?ats::String(" ra0addr "):ats::String(" apcli0addr ");
			cmd = "ifconfig " + inf + " " +  p_argv[3] + " netmask " +  p_argv[4]; 
			cmd += "; db-config set system " + db + ats::String(p_argv[3]) + "\n";
		}
	}
	else if ((!strcmp("setssid", p_argv[1])) && (p_argc == 3)) // setssid  <ssid>
	{
		const ats::String& ssid=p_argv[2];
		cmd = "iwpriv ra0 set SSID=" + ssid +"\n";
	}
	else if ((!strcmp("setwpapsk", p_argv[1])) && (p_argc == 3)) // setwpapsk <password>
	{
		const ats::String& pass=p_argv[2];
		cmd = "iwpriv ra0 set WPAPSK=" + pass +"\n";
	}
	else if ((!strcmp("setauthmode", p_argv[1])) && (p_argc == 3)) // setauthmode <password>
	{

		if(in_array<6>(p_argv[2], g_authmode))
		{
			const ats::String& auth=p_argv[2];
			cmd = "iwpriv ra0 set AuthMode=" + auth +"\n";
		}
	}
	else if ((!strcmp("setencryptype", p_argv[1])) && (p_argc == 3)) // setencryptype <encryptype>
	{

		if(in_array<5>(p_argv[2], g_encryptype))
		{
			const ats::String& type=p_argv[2];
			cmd = "iwpriv ra0 set EncrypType=" + type +"\n";
		}
	}
	else if ((!strcmp("setwifiap", p_argv[1])) && ( p_argc == 5 || p_argc == 6 )) // setwifiap <essid> <authmode> <encryptype> [password]
	{
		if( (strcmp("NONE", p_argv[4]) != 0) && (p_argc == 5 ))
		{
			ats_logf(ATSLOG_ERROR, "%s,%d:ERROR: password not existing", __FILE__, __LINE__);
			send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s: password not existing\r", phpcmd);
			return 1;
		}
		const ats::String& essid = p_argv[2];
		const ats::String& authmod = p_argv[3];
		const ats::String& encryptype = p_argv[4];
		const ats::String& password = (p_argc == 6)?p_argv[5]:"";

		cmd = "/usr/bin/wificonfig.sh \'" + essid + "\' " + authmod + " " + encryptype + " \'" + password  + "\'\n";
		const int ret = ats::system(cmd);
		if((WIFEXITED(ret) && (0 == WEXITSTATUS(ret))))
		{
			send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s: ok\r", phpcmd);
			return 0;
		}
		else
		{
			ats_logf(ATSLOG_ERROR, "%s,%d: wifi config script failed: ret=%d, WIFEXITED(ret)=%d, WEXITSTATUS(ret)=%d", __FILE__, __LINE__,
					ret, WIFEXITED(ret), WEXITSTATUS(ret));
			send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s: setwifiap failed\r", phpcmd);
			return 1;
		}
	}
	else if ((!strcmp("enablenetwork", p_argv[1])) && (p_argc == 3)) // enablenetwork <eth/wifiap/wificl>
	{
		const ats::String& inf = (!strcmp("eth", p_argv[2]))?g_interface[0]:((!strcmp("wifiap", p_argv[2]))?g_interface[1]:g_interface[2]);
		cmd = "ifconfig " + inf + " up\n";
	}
	else if ((!strcmp("disablenetwork", p_argv[1])) && (p_argc == 3)) // disablenetwork <eth/wifiap/wificl>
	{
		const ats::String& inf = (!strcmp("eth", p_argv[2]))?g_interface[0]:((!strcmp("wifiap", p_argv[2]))?g_interface[1]:g_interface[2]);
		cmd = "ifconfig " + inf + " down\n";
	}
	else if ((!strcmp("enabledhcpd", p_argv[1]))) // enabledhcpd
	{
		cmd = "/etc/rc.d/init.d/dhcpd start\n";
	}
	else if ((!strcmp("disabledhcpd", p_argv[1]))) // disabledhcpd
	{
		cmd = "/etc/rc.d/init.d/dhcpd stop\n";
	}
	else if (((!strcmp("setdnsconf", p_argv[1])) ||(!strcmp("setipsecconf", p_argv[1]))||(!strcmp("setwifidata", p_argv[1]))|| (!strcmp("setipsecsecrets", p_argv[1]))) && (p_argc == 4 ))// setdnsconf/setipsecconf/setwifidata/setipsecsecrets <filename> <md5sum>, file is under /tmp/webconfig
	{
		const ats::String& command = p_argv[1];
		const ats::String& filename = p_argv[2];
		const ats::String& md5sum = p_argv[3];

		const ats::String &srcfile="/tmp/webconfig/"+filename;
		bool existing = (access(srcfile.c_str(), F_OK) != -1) ?  1: 0;
		if(!existing)
		{
			ats_logf(ATSLOG_ERROR, "%s,%d:ERROR: Source file not existing", __FILE__, __LINE__);
			send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s: Source file not existing\r", phpcmd);
			return 1;
		}

		ats::StringMap config;
		config.set("setdnsconf", "/etc/resolv.conf");
		config.set("setipsecconf", "/etc/ipsec.conf");
		config.set("setwifidata", "/etc/Wireless/RT2870AP/RT2870AP.dat");
		config.set("setipsecsecrets", "/etc/ipsec.secrets");

		if(command.compare("setipsecsecrets")==0 )
		{
			const ats::String& exec = "chmod 600 /tmp/webconfig/" + filename +"\n";//it is vital that the secret file be protected.
			ats::system(exec);
		}
		else if (command.compare("setwifidata")==0)
		{
			//update db-config database.
			const ats::String& exec = "db-config set WiFi RT2870AP.dat --file=/tmp/webconfig/" + filename +"\n";
			ats::system(exec);
		}

		const ats::String& dest = config.get(command);
		cmd = "/usr/bin/fileoverride.sh /tmp/webconfig/" + filename + " " + dest + " " + md5sum +"\n";
	}
	else if(((!strcmp("setwifidhcp", p_argv[1])) || (!strcmp("setethdhcp", p_argv[1]))) && (p_argc == 5)) // setwifidhcp/setethdhcp ip=192.168.36.1 startip=192.168.36.20 ipp=192.168.36.254
	{
		ats::StringMap sm;
		sm.from_args(p_argc - 1, p_argv + 1);

		if( sm.has_key("ip") && sm.has_key("startip") && sm.has_key("endip"))
		{
			const ats::String& ip = sm.get("ip");
			const ats::String& startip = sm.get("startip");
			const ats::String& endip = sm.get("endip");

			//check ip startip and endip all are valid ip address.
			struct sockaddr_in sa;
			if((inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr)) == 1) && (inet_pton(AF_INET, startip.c_str(), &(sa.sin_addr)) == 1) && (inet_pton(AF_INET, endip.c_str(), &(sa.sin_addr)) == 1))
			{
				const ats::String& ipnoLastOctet = ip.substr(0, ip.find_last_of('.'));
				const ats::String& startipnoLastOctet = startip.substr(0, startip.find_last_of('.'));
				const ats::String& endipnoLastOctet = endip.substr(0, endip.find_last_of('.'));

				if(!ipnoLastOctet.compare(startipnoLastOctet) && !ipnoLastOctet.compare(endipnoLastOctet))
				{
					const bool setwifi = (!strcmp("setwifidhcp", p_argv[1]))?true:false;
					db_monitor::ConfigDB db;
					ats::String err = db.Update("system", (setwifi)?"ra0addr":"eth0addr", ip);

					if(!err.empty())
					{
						ats_logf(ATSLOG_ERROR, "%s,%d: Error setting ra0addr/eth0addr in db-config: %s", __FILE__, __LINE__, err.c_str());
					}
					else
					{
						ats::String err = db.Update("system", (setwifi)?"ra0startip":"eth0startip", startip);
						if(!err.empty())
						{
							ats_logf(ATSLOG_ERROR, "%s,%d: Error setting ra0startip/eth0startip in db-config: %s", __FILE__, __LINE__, err.c_str());
						}
						else
						{
							ats::String err = db.Update("system", (setwifi)?"ra0endip":"eth0endip", endip);
							if(!err.empty())
							{
								ats_logf(ATSLOG_ERROR, "%s,%d: error setting ra0endip/eth0endip in db-config: %s", __FILE__, __LINE__, err.c_str());
							}
							else
							{
								ats::String err = db.Unset("system", "dhcpd.conf");
								if(!err.empty())
								{
									ats_logf(ATSLOG_ERROR, "%s,%d: error unset dhcpd.conf  in db-config: %s", __FILE__, __LINE__, err.c_str());
								}
								else
								{
									send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s: ok\r", phpcmd);
									return 0;
								}
							}
						}
					}
				}

			}
		}
	}
	else if ((!strcmp("ipsecstatus", p_argv[1])) && (p_argc == 3)) // ipsecstatus <policy name> , return value is "connected" or "disconnected" string.
	{
		const ats::String& policyname = p_argv[2];
		const ats::String& exec = "/usr/local/sbin/ipsec auto --status 2>/dev/null | grep -i " + policyname +
			" | grep -c -i 'ipsec sa established' \n";

		std::stringstream output;
		ats::system(exec, &output);

		if(atoi(output.str().c_str()) == 1)
			send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s:%s status: Connected\r", phpcmd, policyname.c_str());
		else
			send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s:%s status: Disconnected\r", phpcmd, policyname.c_str());

		return 0;
	}
	else if ((!strcmp("ipsecrestart", p_argv[1]))) // ipsecrestart
	{
		cmd = "echo -e restart | telnet localhost 41013 \n";
	}
	else if ((!strcmp("ipsecconnect", p_argv[1]) || (!strcmp("ipsecdisconnect", p_argv[1]))|| (!strcmp("ipsecrefresh", p_argv[1])))
			&& (p_argc == 3)) // ipsecconnect/ipsecdisconnect/ipsecrefresh <policyname>.
	{
		const ats::String& name=p_argv[2];
		cmd = "echo -e \"";
		cmd += (!strcmp("ipsecconnect", p_argv[1]))?"connect ":((!strcmp("ipsecdisconnect", p_argv[1]))
				?"disconnect ":"refresh ");
		cmd += " "+ name + "\"| telnet localhost 41013 \n";
	}
	else if ((!strcmp("getcellrssi", p_argv[1])) && (p_argc == 2)) // getcellrssi
	{
		REDSTONE_IPC ipc;
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%d\r", ipc.Rssi());
		return 0;
	}
	else if (
			(	!strcmp("getcellimsi", p_argv[1]) ||
				!strcmp("getcellccid", p_argv[1]) ||
				!strcmp("getcellpnumber", p_argv[1]) ||
				(!strcmp("getcellnetwork", p_argv[1])) ||
				(!strcmp("getcellapn", p_argv[1]))
			)
			&& (p_argc == 2)
		)// getcellimsi/getcellpnumber/getcellnetwork/getcellapn
	{
		// XXX: Not using "send_app_msg" because this command shall not be queued if "telit-monitor" is not ready.
		send_redstone_ud_msg("telit-monitor", 0, "%s\r", p_argv[1]);

		struct timespec ts;
		memset(&ts, 0, sizeof(ts));
		ts.tv_sec = time(NULL) + 2;
		const int ret = sem_timedwait(p_acc.m_data->m_gsm_sem, &ts);

		if(!ret)
		{
			const int skip_get_prefix = 3;
			const char* key = p_argv[1] + skip_get_prefix;
			p_acc.m_data->lock();
			const ats::String result((p_acc.m_data->m_config).get(key));
			p_acc.m_data->unlock();
			send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s\r", result.c_str());
			return 0;
		}

		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s: fail\r", phpcmd);
		return 1;
	}
	else if((!strcmp("getapnlist", p_argv[1]))) // getapnlist
	{
		db_monitor::ConfigDB db;

		{
			const ats::String& emsg = db.open_db("apn", "/etc/redstone/apn.db");

			if(!emsg.empty())
			{
				ats_logf(ATSLOG_ERROR, "%s,%d:ERROR: %s", __FILE__, __LINE__, emsg.c_str());
				send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s: APN database not found\r", phpcmd);
				return 1;
			}

		}

		std::set<ats::String> networkset;

		{
			db.query("apn", "select v_Network from t_APN");

			const db_monitor::ResultTable& t = db.Table();
			db_monitor::ResultTable::const_iterator i = t.begin();

			while(i != t.end())
			{
				const db_monitor::ResultRow& r = *i;
				++i;

				if(!r.empty())
				{
					networkset.insert(r[0]);
				}

			}

		}

		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "<?xml version=\"1.0\"?> <networks>");

		{
			const ats::String& defaultapn = db.GetValue("Cellular", "carrier");

			if(!(defaultapn.empty()))
			{
				send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "<defaultapn name=\"%s\"/>", defaultapn.c_str());
			}

		}

		std::set<ats::String>::iterator it;

		for(it = networkset.begin(); it != networkset.end(); ++it)
		{
			db.query("apn", "select v_APN from t_APN where v_Network='" + (*it) + "'");

			send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "<network name=\"%s\">", (*it).c_str());

			const db_monitor::ResultTable& t = db.Table();
			db_monitor::ResultTable::const_iterator i = t.begin();

			while(i != t.end())
			{
				const db_monitor::ResultRow& r = *i;
				++i;

				if(!r.empty())
				{
					send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "<apn>%s</apn>", r[0].c_str());
				}

			}

			send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "</network>");
		}

		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "</networks>\r");
		return 0;
	}
	else if((!strcmp("setdefaultapn", p_argv[1])) && (p_argc == 3)) // setdefaultapn <apnstring>
	{
		cmd = "db-config set Cellular carrier \"" + ats::String(p_argv[2]) + "\" \n";
		std::stringstream output;

		int ret = ats::system(cmd);
		if(ret)
		{
			ats_logf(ATSLOG_ERROR, "%s,%d:ERROR: ret=%d for cmd=%s", __FILE__, __LINE__, ret, cmd.c_str());
			send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s: fail\r", phpcmd);
			return 1;
		}

		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s: ok\r", phpcmd);

		std::stringstream out;
		ret = ats::system("ps | grep -v grep | grep -v '/bin/sh' | grep '\\s\\+modem-monitor' | awk '{print $1}'", &out);
		if(!ret && !out.str().empty())
		{
			int pid = strtol(out.str().c_str(), 0, 0);

			//check group id from /proc/pid/stat, pid should be equal to group pid.
			std::stringstream output;
			const ats::String& s = " cat /proc/" + ats::toStr(pid) +"/stat | awk '{gsub(/[()]/,\"\",$2); if($2==\"modem-monitor\" && $1==$5) {print 0;} else {print 1;}}'";
			ret= ats::system(s, &output);

			if(!ret && strtol(output.str().c_str(),0,0) == 0)
			{
				ret = kill(-pid, SIGTERM);
				if(ret)
				{
					ats_logf(ATSLOG_ERROR, "%s,%d:kill modem-monitor pid:%d fail ret %d, error %s", __FILE__, __LINE__, pid, ret,  strerror(errno));
				}
			}
			else
			{
				ats_logf(ATSLOG_ERROR, "%s,%d:Modem-monitor Group pid %d not equal to pid", __FILE__, __LINE__, pid);
			}

		}

		return 0;
	}
	else if((!strcmp("getgpsdata", p_argv[1]))) // getgpsdata
	{
		NMEA_Client& nmea_client = p_acc.m_data->getNmeaClient();

		if((nmea_client.IsValid()) && (nmea_client.GPS_Quality() != 0) && nmea_client.GetData().m_type == NMEA_DATA::LiveDataFlag)
		{
			const double lon = nmea_client.Lon();
			const double lat = nmea_client.Lat();
			const double alt = nmea_client.H_ortho();
			const double head = nmea_client.COG();
			const double hdop = nmea_client.HDOP();
			const double vel = nmea_client.SOG()* MS_TO_KPH / MS_TO_KNOTS ;

			const int num = nmea_client.NumSVs();
			const int quality = nmea_client.GPS_Quality();

			const short year = nmea_client.Year();
			const short month = nmea_client.Month();
			const short day = nmea_client.Day();
			const short hour = nmea_client.Hour();
			const short minute = nmea_client.Minute();
			const short seconds = nmea_client.Seconds();

			REDSTONE_IPC ipc;
			const short speed = ipc.Speed();

			send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL,
				"<?xml version=\"1.0\"?><gps>"
					"<latitude>%.6f</latitude>"
					"<longitude>%.6f</longitude>"
					"<time>%d-%.2d-%.2d %.2d:%.2d:%.2d</time>"
					"<elevation>%.3f</elevation>"
					"<heading>%.3f</heading>"
					"<hdop>%.3f</hdop>"
					"<velocity>%.3f</velocity>"
					"<satellites>%d</satellites>"
					"<quality>%d</quality>"
					"<obdspeed>%d</obdspeed>"
				"</gps>\r"
				, lat
				, lon
				, year, month, day, hour, minute, seconds
				, alt
				, head
				, hdop
				, vel
				, num
				, quality
				, speed
				);
			return 0;
		}

		ats_logf(ATSLOG_ERROR, "%s,%d:ERROR: GPS is invalid", __FILE__, __LINE__);
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "GPS is invalid\r");
		return 1;
	}
	else if ((!strcmp("dbconfigset", p_argv[1]))  && (p_argc == 5 )) // dbconfigset <app name> <key> <value>
	{
		cmd = "db-config set " +  ats::String(p_argv[2]) + " " + ats::String(p_argv[3]) + " " + ats::String(p_argv[4]) + "\n";
	}
	else if ((!strcmp("dbconfigunset", p_argv[1]))  && (p_argc == 4 )) // dbconfigunset <app name> <key>
	{
		cmd = "db-config unset " +  ats::String(p_argv[2]) + " " + ats::String(p_argv[3]) + "\n";
	}
	else if (!(strcmp("dbset", p_argv[1]))) // dbset [app name] [key] [value]
	{	
		return dbset(p_acc, ((p_argc >=3) ? p_argv[2] : ""), ((p_argc >= 4) ? p_argv[3] : ""), ((p_argc >= 5) ? p_argv[4]: ""));
	}
	else if (!(strcmp("dbsetfile", p_argv[1]))) // dbsetfile [app name] [key] [fileloc]
	{	
		return dbsetfile(p_acc, ((p_argc >=3) ? p_argv[2] : ""), ((p_argc >= 4) ? p_argv[3] : ""), ((p_argc >= 5) ? p_argv[4]: ""));
	}
	else if ((!strcmp("reboot", p_argv[1]))) // reboot
	{
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s: ok\r", phpcmd);
		ats::system(
				"uptime >> /tmp/logdir/reboot.txt"
				"date >> /tmp/logdir/reboot.txt"
				";echo \"admin-client: User reboot requested\" >> /tmp/logdir/reboot.txt"
				";sync;reboot");
		return 0;
	}
	else if ((!strcmp("featureset", p_argv[1]))  && (p_argc == 3 )) // featureset <app name>
	{
		cmd = "feature set " +  ats::String(p_argv[2]) + "\n";
	}
	else if ((!strcmp("featureunset", p_argv[1]))  && (p_argc == 3 )) // featureunset <app name>
	{
		cmd = "feature unset " +  ats::String(p_argv[2]) + "\n";
	}
	else if ((!strcmp("getobdspeed", p_argv[1]))) // getobdspeed
	{
		REDSTONE_IPC ipc;
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%d\r", ipc.Speed());
		return 0;
	}
	else if ((!strcmp("BatteryVoltage", p_argv[1]))) // getobdspeed
	{
		REDSTONE_IPC ipc;
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%.1f V\r", ipc.BatteryVoltage());
		return 0;
	}
	else if ((!strcmp("MicroVersion", p_argv[1]))) // get the micro version
	{
		std::stringstream output;
		ats::system("get_micro_version", &output);
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s\r", output.str().c_str());
		return 0;
	}
	else if ((!strcmp("HWVersion", p_argv[1]))) // get the hardware version
	{
		std::stringstream output;
		ats::system("hid", &output);
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s\r", output.str().c_str());
		return 0;
	}
	else if ((!strcmp("keepalive", p_argv[1])) && (p_argc == 2)) // keepalive
	{
		//keep 5 mins wake up.
// 		ats::system("echo \"unset_work key=lcm\"|telnet localhost 41009");
		ats::system("echo \"set_work key=lcm expire=300\"|telnet localhost 41009");
		return 0;
	}
	else if ((!strcmp("CellIP", p_argv[1])))
	{
		std::stringstream output;
		ats::system("ifconfig ppp0 | grep inet | awk 'BEGIN {FS=\":\"}{print $2}'| awk 'BEGIN {FS=\" \"}{print $1}'", &output);
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s\r", output.str().c_str());
		return 0;
	}
	else if ((!strcmp("getGPSSats", p_argv[1])))
	{
		NMEA_Client& nmea_client = p_acc.m_data->getNmeaClient();

		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%d\r", nmea_client.NumSVs());
		return 0;
	}
	else if ((!strcmp("getMessagesType", p_argv[1])) ) // getMessageType
	{
		db_monitor::ConfigDB db;
		{
			const ats::String& emsg = db.open_db("messages_db", "/mnt/update/database/messages.db");

			if(!emsg.empty())
			{
				ats_logf(ATSLOG_ERROR, "%s,%d:ERROR: %s", __FILE__, __LINE__, emsg.c_str());
				send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s: message database not found\r", phpcmd);
				return 1;
			}

		}

		db.query("messages_db", "select name,cantel_id from message_types;");
		{
			const db_monitor::ResultTable& t = db.Table();
			db_monitor::ResultTable::const_iterator i = t.begin();
			send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "<msgs>");

			while(i != t.end())
			{
				const db_monitor::ResultRow& r = *i;
				++i;

				if((r.size() > 1) && !(r[1].empty()))
				{
					send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "<msg name=\"%s\">%s</msg>", r[0].c_str(), r[1].c_str());
				}

			}

			send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "</msgs>\r");
			return 0;
		}
	}
	else if ((!strcmp("CheckUpdate", p_argv[1])) ) // run the check-update command
	{
		std::stringstream output;
		ats::system("rm /var/log/check-update.log", &output);
		ats::system("/bin/sh /usr/bin/check-update", &output);
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s\r", output.str().c_str());
	}
	else
	{
		ats_logf(ATSLOG_ERROR, "%s,%d:ERROR: Invalid command", __FILE__, __LINE__);
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s: Invalid command\r", phpcmd);
		return 1;
	}

	std::stringstream output;
	int ret = 0;
	if(!cmd.empty())
		ret = ats::system(cmd, &output);
	else
	{
		ats_logf(ATSLOG_ERROR, "%s,%d:ERROR: Invalid command", __FILE__, __LINE__);
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s: Invalid command\r", phpcmd);
		return 1;
	}

	if(ret)
	{
		ats_logf(ATSLOG_ERROR, "%s,%d:ERROR: ret=%d for cmd=%s", __FILE__, __LINE__, ret, cmd.c_str());
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s: fail\r", phpcmd);
		return 1;
	}

	send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s: ok\r", phpcmd);
	return 0;
}

int ac_phpcmd(AdminCommandContext& p_acc, int p_argc, char* p_argv[])
{
	const int ret = h_ac_phpcmd(p_acc, p_argc, p_argv);
	return ret;
}

MyData::MyData()
{
}

void MyData::init_wheel_data()
{
	// Privileged commands
	m_wheel_cmd.insert(AdminCommandPair("exec", ac_exec));
}

void MyData::init_data()
{
	m_first_time = true;

	{
		db_monitor::ConfigDB db;
		m_blink_refresh_seconds = db.GetInt(g_app_name, "blink_refresh_seconds", 5);
		m_set_work_expire_seconds = db.GetInt(g_app_name, "set_work_expire_seconds", 120);
	}

	m_mutex = new pthread_mutex_t;
	pthread_mutex_init(m_mutex, 0);

	m_gsm_sem = new sem_t;
	sem_init(m_gsm_sem, 0 ,0);

	// General commands

	m_command.insert(AdminCommandPair("speedlimit", ac_speedlimit));
	m_command.insert(AdminCommandPair("speedlimitgrace", ac_speedlimitgrace));

	m_command.insert(AdminCommandPair("version", ac_version));
	m_command.insert(AdminCommandPair("sn", ac_sn));
	m_command.insert(AdminCommandPair("info", ac_info));
	m_command.insert(AdminCommandPair("ping", ac_ping));
	m_command.insert(AdminCommandPair("web", ac_web));
	m_command.insert(AdminCommandPair("phpcmd", ac_phpcmd));
	m_command.insert(AdminCommandPair("atcmd", ac_atcmd));
	m_command.insert(AdminCommandPair("wakeup", ac_wakeup));
	m_command.insert(AdminCommandPair("state", ac_state));

	m_command.insert(AdminCommandPair("reboot", ac_reboot));

	// System administrator commands
	m_admin_cmd.insert(AdminCommandPair("debug", ac_debug));
	m_admin_cmd.insert(AdminCommandPair("db", ac_db));
	m_admin_cmd.insert(AdminCommandPair("reconnect", ac_reconnect));

	// Developer commands
	m_dev_cmd.insert(AdminCommandPair("system", ac_system));
}

int g_fd = -1;

static bool is_command_response(const ats::String& p_cmd)
{
	size_t i;

	for(i = 0; i < p_cmd.length(); ++i)
	{

		switch(p_cmd[i])
		{
		case ' ':
		case '\t':
		case '\n': return false;
		case ':': return true;
		}

	}

	return false;
}

static void* h_client_for_AdminServer( MyData& p_md)
{
	MyData& md = p_md;
	ClientSocket cs;
	init_ClientSocket(&cs);
	ats_logf(ATSLOG_DEBUG, "Connecting to AdminServer");
	const char* host = "127.0.0.1";
	const int port = 39001;
	const bool testmode = ats::file_exists(ATS_TEST_MODE_FILE);

	for(;;)
	{
		cs.m_keepalive_count = 0;
		cs.m_keepalive_idle = 0;
		cs.m_keepalive_interval = 0;

		if(!connect_client(&cs, host, port))
		{
			ats_logf(ATSLOG_DEBUG, "Connected to %sAdminServer \"%s:%d\" at %s", testmode ? "TESTMODE " : "", host, port, cs.m_resolved_ip);
			break;
		}

		sleep(1);
	}

	AdminCommandContext acc(md, cs);

	CommandBuffer cb;
	init_CommandBuffer(&cb);
	alloc_dynamic_buffers(&cb, 512, 16384);
	ats::SocketInterfaceResponse response(cs.m_fd, testmode ? 1 : 0, 0);

	for(;;)
	{
		const ats::String& cmdline = response.get();

		if(cmdline.empty())
		{

			if(response.error())
			{
				ats_logf(ATSLOG_ERROR, "%s,%d:ERROR %d: Read from \"%s:%d\" failed, closing connection", __FILE__, __LINE__, response.error(), host, port);
				break;
			}

			const int ret = send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "\r");

			if(ret < 0)
			{
				ats_logf(ATSLOG_DEBUG, "%s,%d: \"%s:%d\" (%d) %s", __FILE__, __LINE__, host, port, -ret, strerror(-ret));
				break;
			}

			continue;
		}

		const char* err;

		if((err = gen_arg_list(cmdline.c_str(), int(cmdline.size()), &cb)))
		{
			ats_logf(ATSLOG_ERROR, "%s,%d:ERROR: gen_arg_list failed (%s), from \"%s:%d\"", __FILE__, __LINE__, err, host, port);
			send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "xml <devResp error=\"gen_arg_list failed\">%s</devResp>\r", err);
			continue;
		}

		if(cb.m_argc <= 0)
		{
			continue;
		}

		const char* cmd_raw = "";
		ats::String cmd;
		ats::String respKey;
		int argc = cb.m_argc;
		char** argv = cb.m_argv;
		AdminCommandMap* cmd_map = &(md.m_command);

		if((!strcmp("cmd", cb.m_argv[0])) && (cb.m_argc >= 3))
		{
			respKey = cb.m_argv[1];
			cmd = cmd_raw = cb.m_argv[2];
			argc -= 2;
			argv += 2;
		}
		else if(!strcmp("dev", cb.m_argv[0]) && (cb.m_argc >= 3))
		{
			respKey = cb.m_argv[1];
			cmd = cmd_raw = cb.m_argv[2];
			argc -= 2;
			argv += 2;
			cmd_map = &(md.m_dev_cmd);
		}
		else if(!strcmp("adm", cb.m_argv[0]) && (cb.m_argc >= 3))
		{
			respKey = cb.m_argv[1];
			cmd = cmd_raw = cb.m_argv[2];
			argc -= 2;
			argv += 2;
			cmd_map = &(md.m_admin_cmd);
		}
		else
		{
			cmd = cmd_raw = cb.m_argv[0];
		}

		if(is_command_response(cmd))
		{
			continue;
		}

		{
			AdminCommandMap::const_iterator i = cmd_map->find( cmd);
			send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "xml <devResp respKey=\"%s\">", to_base64(respKey).c_str());

			if(i != cmd_map->end())
			{
				(i->second)(acc, argc, argv);
			}
			else
			{
				ats::String s;
				ats_sprintf(&s, "<error>Invalid command \"%s\"</error>", cmd_raw);
				send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "%s", to_base64(s).c_str());
			}

			send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "</devResp>\r");
		}
	}

	free_dynamic_buffers(&cb);

	shutdown(cs.m_fd, SHUT_WR | SHUT_RD);
	close(cs.m_fd);
	cs.m_fd = -1;

	return 0;
}

// ************************************************************************
// Description: Connects to AdminServer (through an SSH tunnel) and responds to
//	commands.
//
//	If no connection to AdminServer can be made (due to no SSH tunnel,
//	or no network access, etc.) then the function will sleep for 1 second, and
//	then try again to connect. This is repeated until a connection to AdminServer
//	is finally made.
//
//	Once a connection is made, the function will listen forever for AdminServer
//	commands. If an error occurs which causes the AdminServer connection to
//	be broken, then this function will log the error condition and exit.
//	It is expected that the caller which called "client_for_AdminServer"
//	will immediately call "client_for_AdminServer" again so that it can try
//	to re-establish the AdminServer connection and resume normal operations.
//
// Parameters:	p - pointer to ClientData
// Return:	NULL pointer
// ************************************************************************
static void* client_for_AdminServer(void* p)
{
	for(;;) h_client_for_AdminServer(*((MyData*)p));
	return 0;
}

// ************************************************************************
// Description: A local command server so that other applications or
//	developers can query admin-client directly (instead of having to
//	go through AdminServer and the SSH tunnel).
//
//	This function supports the same command set as "client_for_AdminServer".
//
//	One instance of this function is created for every local connection
//	made.
//
// Parameters:
// Return:	NULL pointer
// ************************************************************************
static void* local_command_server(void* p)
{
	const size_t max_command_length = 1024;
	char cmdline_buf[max_command_length + 1];
	char* cmdline = cmdline_buf;
	bool command_too_long = false;

	ClientData* cd = (ClientData*)p;
	MyData& md = *((MyData*)(cd->m_data->m_hook));
	AdminCommandContext acc(md, *cd);

	ClientDataCache cdc;
	init_ClientDataCache(&cdc);

	CommandBuffer cb;
	init_CommandBuffer(&cb);

	for(;;)
	{
		char ebuf[SOCKET_INTERFACE_ERROR_MAX_LENGTH];
		const int c = client_getc_cached(cd, ebuf, sizeof(ebuf), &cdc);

		if(c < 0)
		{
			if(c != -ENODATA) ats_logf(ATSLOG_ERROR, "%s,%d:ERROR: %s", __FILE__, __LINE__, ebuf);
			break;
		}

		if(c != '\r' && c != '\n')
		{
			if(size_t(cmdline - cmdline_buf) >= max_command_length) command_too_long = true;
			else *(cmdline++) = c;

			continue;
		}

		if(command_too_long)
		{
			ats_logf(ATSLOG_ERROR, "%s,%d:ERROR: command is too long", __FILE__, __LINE__);
			cmdline = cmdline_buf;
			command_too_long = false;
			send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "command is too long\r");
			continue;
		}

		const char* err = gen_arg_list(cmdline_buf, cmdline - cmdline_buf, &cb);
		ats_logf(ATSLOG_INFO, "%s,%d:%s: Command received: %.*s", __FILE__, __LINE__, __FUNCTION__, int(cmdline - cmdline_buf), cmdline_buf);
		cmdline = cmdline_buf;

		if(err)
		{
			ats_logf(ATSLOG_ERROR, "%s,%d:ERROR: gen_arg_list failed (%s)", __FILE__, __LINE__, err);
			send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "gen_arg_list failed%s\r", err);
			continue;
		}

		if(cb.m_argc <= 0)
		{
			continue;
		}

		AdminCommandMap* cmd_map = &(md.m_command);
		int argc = cb.m_argc;
		char** argv = cb.m_argv;
		const char* cmd = argv[0];

		if(!strcmp("dev", argv[0]) && (argc >= 2))
		{
			cmd = argv[1];
			argc -= 1;
			argv += 1;
			cmd_map = &(md.m_dev_cmd);
		}
		else if(!strcmp("adm", argv[0]) && (argc >= 2))
		{
			cmd = argv[1];
			argc -= 1;
			argv += 1;
			cmd_map = &(md.m_admin_cmd);
		}

		{
			AdminCommandMap::const_iterator i = cmd_map->find(cmd);

			if(i != cmd_map->end())
			{
				(i->second)(acc, argc, argv);
			}
			else
			{
				send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "Invalid command \"%s\"\r", cmd);
			}

		}
	}

	return 0;
}


// ************************************************************************
// Description: A "wheel" command server providing access to privileged commands.
//
//	One instance of this function is created for every local connection
//	made.
//
// Parameters:
// Return:	NULL pointer
// ************************************************************************
static void* wheel_server(void* p)
{
	ClientData* cd = (ClientData*)p;
	MyData& md = *((MyData*)(cd->m_data->m_hook));

	bool command_too_long = false;
	ats::String cmd;

	const size_t max_command_length = 1024;

	AdminCommandContext acc(md, *cd);

	ClientDataCache cdc;
	init_ClientDataCache(&cdc);

	CommandBuffer cb;
	init_CommandBuffer(&cb);
	alloc_dynamic_buffers(&cb, 512, 16384);

	for(;;)
	{
		char ebuf[SOCKET_INTERFACE_ERROR_MAX_LENGTH];
		const int c = client_getc_cached(cd, ebuf, sizeof(ebuf), &cdc);

		if(c < 0)
		{
			if(c != -ENODATA) ats_logf(ATSLOG_INFO, "%s,%d: %s", __FILE__, __LINE__, ebuf);
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
			cmd.clear();
			command_too_long = false;
			send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "error: command is too long\n\r");
			continue;
		}

		const char* err;

		if((err = gen_arg_list(cmd.c_str(), cmd.length(), &cb)))
		{
			send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "error: gen_arg_list failed: %s\n\r", err);
			cmd.clear();
			continue;
		}

		ats_logf(ATSLOG_DEBUG, "%s,%d:%s: Command received: %s", __FILE__, __LINE__, __FUNCTION__, cmd.c_str());
		cmd.clear();

		if(cb.m_argc <= 0)
		{
			continue;
		}

		{
			ats::String cmd;
			ats::String key;
			ats::get_command_key(cb.m_argv[0], cmd, key);
			AdminCommandMap::const_iterator i = md.m_wheel_cmd.find(cmd);

			if(i != md.m_wheel_cmd.end())
			{

				if(key.empty())
				{
					send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "%s: ", cmd.c_str());
				}
				else
				{
					send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "%s,%s: ", cmd.c_str(), key.c_str());
				}

				(i->second)(acc, cb.m_argc, cb.m_argv);
				send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "\n\r");
			}
			else
			{
				send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "error: Invalid command \"%s\"\n\r", cmd.c_str());
			}

		}

	}

	free_dynamic_buffers(&cb);
	return 0;
}

int main(int argc, char* argv[])
{
	MyData md;
	ats::StringMap &config = md.m_config;
	config.from_args(argc - 1, argv + 1);
	
	{  // scoping is required to force closure of db-config database
		db_monitor::ConfigDB db;
		g_dbg = db.GetInt(argv[0], "LogLevel", 0);  // 0:Error 1:Debug 2:Info
	}
	
	g_log.set_global_logger(&g_log);
	g_log.set_level(g_dbg);

	{
		const int pid = fork();

		if(!pid)
		{
			ats::daemonize();
			md.init_wheel_data();
			ServerData wheel_server_data;
			{
				ServerData& sd = wheel_server_data;
				init_ServerData(&sd, 64);
				sd.m_hook = &md;
				sd.m_cs = wheel_server;
				set_unix_domain_socket_user_group(&sd, "root", "wheel");
				start_redstone_ud_server(&sd, g_app_name.c_str(), 1);
			}

			ats::infinite_sleep();
			exit(1);
		}

		waitpid(pid, 0, 0);
	}

	g_log.open_testdata(g_app_name);
	md.init_data();
	ats_logf(ATSLOG_ERROR, "Admin Client started");

	{
		static ServerData sd;
		init_ServerData(&sd, 256);
		sd.m_hook = &md;
		sd.m_cs = local_command_server;
		set_unix_domain_socket_user_group(&sd, "applet", "applet");
		start_trulink_ud_server(&sd, (g_app_name + "-cmd").c_str(), 1);
	}

	// AWARE360 FIXME: This TCP server is deprecated. Users should use the "admin-client"
	//	Unix Domain Socket instead.
	{
		static ServerData server_data;
		ServerData* sd = &server_data;
		init_ServerData(sd, 256);
		sd->m_port = 39000;
		sd->m_hook = &md;
		sd->m_cs = local_command_server;
		::start_server(sd);
	}

	{
		static pthread_t client_for_AdminServer_thread;
		const int retval = pthread_create(
			&client_for_AdminServer_thread,
			(pthread_attr_t*)0,
			client_for_AdminServer,
			&md);
		if(retval) ats_logf(ATSLOG_ERROR, "%s,%d:ERROR: (%d) Failed to start run_client thread", __FILE__, __LINE__, retval);
	}

	ats::infinite_sleep();
	return 0;
}
