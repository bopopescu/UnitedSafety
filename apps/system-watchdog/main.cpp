#include <stdio.h>
#include <syslog.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sys/wait.h>

#include "ats-string.h"
#include "socket_interface.h"
#include "command_line_parser.h"
#include "system_watchdog.h"

static const ats::String g_app_name("system-watchdog");
static int g_dbg = 0;

static int ac_debug(AdminCommandContext &p_acc, const AdminCommand&, int p_argc, char *p_argv[])
{

	if(p_argc <= 1)
	{
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "debug=%d\n", g_dbg);
	}
	else
	{
		g_dbg = strtol(p_argv[1], 0, 0);
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "debug=%d\n", g_dbg);
	}

	return 0;
}

MyData::MyData()
{
	m_mutex = new pthread_mutex_t;
	pthread_mutex_init(m_mutex, 0);

	m_command.insert(AdminCommandPair("debug", AdminCommand(ac_debug)));
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
			if(c != -ENODATA) syslog(LOG_ERR, "%s,%d: %s", __FILE__, __LINE__, ebuf);
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
			syslog(LOG_ERR, "%s,%d: command is too long", __FILE__, __LINE__);
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
			syslog(LOG_ERR, "%s,%d: gen_arg_list failed (%s)", __FILE__, __LINE__, err);
			send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "gen_arg_list failed: %s\n", err);
			cmd.clear();
			continue;
		}

		if(g_dbg >= 2)
		{
			syslog(LOG_INFO, "%s,%d:%s: Command received: %s", __FILE__, __LINE__, __FUNCTION__, cmd.c_str());
		}

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
				(i->second).m_fn(acc, i->second, cb.m_argc, cb.m_argv);
			}
			else
			{
				send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "Invalid command \"%s\"\n", cmd.c_str());
			}
		}
	}

	return 0;
}

// watch_loop will allow the system to run for a number seconds. During this time,
// watch_loop only protects against catastrophic failures (ones which break even
// watch_loop). Once the watch_loop duration has expired, this function returns
// to allow System-Watchdog to check for more sutble failures.
void watch_loop(const ats::String& p_wdfile)
{
	const int seconds_to_run = 20;
	int i;

	for(i = 0; i < seconds_to_run; ++i)
	{
		FILE* f = fopen(p_wdfile.c_str(), "w");

		if(f)
		{
			fwrite("\n", 1, 1, f);
			fclose(f);
		}

		sleep(1);
	}

}

#define READ 0
#define WRITE 1
static int g_p2c[2]; // Parent to child pipe
static int g_c2p[2]; // Child to parent pipe

void watch_process(MyData& p_md)
{
	const int loops_before_state_check = 6;
	int i;
	const ats::String wdfile(p_md.get("watchdog-file"));

	for(i = 0;; ++i)
	{
		// Wait for ready flag from parent
		char c;
		const ssize_t nread = read(g_p2c[READ], &c, 1);

		if(nread <= 0)
		{
			break;
		}

		watch_loop(wdfile);

		if(i >= loops_before_state_check)
		{
			// XXX: The absolute watchdog time limit is 19 seconds (from i.MX28 and Kernel settings).
			//	It is expected that "state" will take 0.5 seconds under normal opreation, and perhaps
			//	3 seconds or so under "adverse" conditions. "state" will always return in time for the
			//	watchdog to continue being kicked. If not, then the system really has failed.
			//
			//	Also this means that the implementation of "state" must keep in mind that it is being
			//	monitored by a watchdog, so no extreme delays may be added.
			const int ret = ats::system("state");

			if(ret)
			{
				syslog(LOG_ERR, "\"state\" returned: %d", ret);
				break;
			}

			i = 0;
		}

		// Send ready flag to parent
		write(g_c2p[WRITE], "k", 1); 

		// Detach a child process and kill "this" parent process. This forces the Kernel to
		// run process creation tasks to verify that the system is still stable enough to
		// start new processes.
		//
		// The new detached child process will continue to communicate with the initial
		// system watchdog parent (via "g_p2c" and "g_c2p").
		ats::detach();
	}

	exit(1);
}

// This function is where all of the complex watchdog stuff happens (such as examining
// information provided via the watchdog socket server).
//
// Return: 0 is returned if the system has failed, and the watchdog must be stopped. 1 is returned if everything is running normally.
int examine_system_state()
{
	return 1;
}

// Description: Checks if "p_mount_line" is exactly as specified by the remaining mount parameters.
//
//
//
// Return: 0 is returned if the specified mount does not match "p_mount_line", and 1 is returned otherwise.
int has_mount(const ats::String& p_mount_line,
	const ats::String& p_mount_spec,
	const ats::String& p_mount_point,
	const ats::String& p_mount_type,
	const ats::String& p_option)
{
	ats::StringList sl;
	ats::split(sl, p_mount_line, " ");

	if(sl.size() <= 0)
	{
		return 0;
	}

	if(p_mount_spec != sl[0])
	{
		return 0;
	}

	if((sl.size() > 1) && (p_mount_point == sl[1]))
	{

		if(((sl.size() > 3) && (p_mount_type == sl[2])))
		{

			ats::StringList o;
			ats::split(o, sl[3], ",");
			ats::StringMap m;

			{
				ats::StringList opt;
				ats::split(opt, p_option, ",");

				size_t i;

				for(i = 0; i < opt.size(); ++i)
				{
					m.set(opt[i], ats::String());
				}

			}

			size_t i;

			for(i = 0; i < o.size(); ++i)
			{
				m.unset(o[i]);
			}

			return m.empty();
		}

	}

	return 0;
}

int check_rootfs()
{
	FILE* f = fopen("/proc/mounts", "r");

	if(!f)
	{
		syslog(LOG_ERR, "%s: Failed to open /proc/mounts", __FUNCTION__);
		return 1;
	}

	bool has_rw = false;

	ats::ReadDataCache_FILE rdc(f);
	ats::String line;

	for(;;)
	{
		const int nread = get_file_line(line, rdc, 1, 1024);

		if(nread)
		{
			break;
		}

		if(has_mount(line, "ubi0", "/", "ubifs", "rw"))
		{
			has_rw = true;
			break;
		}

	}

	fclose(f);

	if(!has_rw)
	{
		syslog(LOG_ERR, "%s: RootFS is not present as R/W", __FUNCTION__);
		return 1;
	}

	return 0;
}

int check_nvramfs()
{
	FILE* f = fopen("/proc/mounts", "r");

	if(!f)
	{
		syslog(LOG_ERR, "%s: Failed to open /proc/mounts", __FUNCTION__);
		return 1;
	}

	bool has_rw = false;

	ats::ReadDataCache_FILE rdc(f);
	ats::String line;

	for(;;)
	{
		const int nread = get_file_line(line, rdc, 1, 1024);

		if(nread)
		{
			break;
		}

		if(has_mount(line, "ubi4_0", "/mnt/nvram", "ubifs", "rw"))
		{
			has_rw = true;
			break;
		}

	}

	fclose(f);

	if(!has_rw)
	{
		syslog(LOG_ERR, "%s: NVRAMFS is not present as R/W", __FUNCTION__);
		return 1;
	}

	return 0;
}

int check_updatefs()
{
	FILE* f = fopen("/proc/mounts", "r");

	if(!f)
	{
		syslog(LOG_ERR, "%s: Failed to open /proc/mounts", __FUNCTION__);
		return 1;
	}

	bool has_rw = false;

	ats::ReadDataCache_FILE rdc(f);
	ats::String line;

	for(;;)
	{
		const int nread = get_file_line(line, rdc, 1, 1024);

		if(nread)
		{
			break;
		}

		if(has_mount(line, "ubi10_0", "/mnt/update", "ubifs", "rw"))
		{
			has_rw = true;
			break;
		}

	}

	fclose(f);

	if(!has_rw)
	{
		syslog(LOG_ERR, "%s: UpdateFS is not present as R/W", __FUNCTION__);
		return 1;
	}

	return 0;
}

// Description: These functions simply requests that their respective partitions (filesystems) be restored on next boot.
//
// XXX: Since at this point the file systems might be in use, or there may be processes actively trying to use them, it is
//	not safe to perform the repair now (a "hot" repair). Instead, the entire system must be restarted, and repaired
//	before it is used.
//
// XXX: The system cannot request that "RootFS" be repaired. If "RootFS" is corrupt, then the entire system (including this
//	application) is corrupt, and instead the TRULink shall keep failing/rebooting-due-to-watch-dog until the boot manager
//	repairs it.
void restore_nvramfs()
{
	ats::touch("/.repair-nvramfs");
}

// Description: Requests that the update partition be restored on next boot.
void restore_updatefs()
{
	ats::touch("/.repair-updatefs");
}

typedef void (*ROM_update_fn)(const ats::String&, void*);

static void backup_config(const ats::String& p_des, void* p_config_des)
{

	if(p_config_des)
	{
		ats::cp("/mnt/nvram/config/config.db", ((const ats::String*)p_config_des)->c_str());
	}

}

static void check_ROM_file(const ats::String& p_src, const ats::String& p_des, ROM_update_fn p_fn=0, void* p_fn_data=0)
{

	if(ats::diff_files(p_src, p_des))
	{
		ats::cp(p_src, p_des);

		if(p_fn)
		{
			p_fn(p_des, p_fn_data);
		}

	}

}

static int sqlite3(const ats::String& p_dbfile, const ats::String& p_query, ats::String& p_out)
{
	p_out.clear();
	int comm[2];

	if(pipe(comm))
	{
		return 124;
	}

	const int pid = fork();

	if(pid < 0)
	{
		close(comm[READ]);
		close(comm[WRITE]);
		return 125;
	}

	if(!pid)
	{
		close(comm[READ]);
		dup2(comm[WRITE], STDOUT_FILENO);
		const char* app = "/usr/bin/sqlite3";
		execl(app, app, p_dbfile.c_str(), p_query.c_str(), NULL);
		exit(1);
	}

	close(comm[WRITE]);
	int err;

	for(;;)
	{
		char buf[2048];
		const ssize_t nread = read(comm[READ], &buf, sizeof(buf));

		if(nread <= 0)
		{
			err = nread;
			close(comm[READ]);
			break;
		}

		p_out.append(buf, nread);
	}

	int status;
	waitpid(pid, &status, 0);

	if(err)
	{
		return 126;
	}

	if(WIFEXITED(status))
	{
		return WEXITSTATUS(status);
	}

	return 127;
}

// Description: Ensures that critical ROM information is backed up in multiple places.
//
// XXX: The U-Boot factory location is NOT checked because that part should only be written during
//	manufacturing.
int check_ROM()
{
	const ats::String romdir[] = {
		// Source directory (must always be first)
		"/mnt/nvram/rom",

		// Back up directories
		"/etc/redstone/rom",
		"/mnt/update/.rom"
	};

	const ats::String sn_src((*romdir) + "/sn.txt");
	const ats::String eth_src((*romdir) + "/eth0macaddr.txt");
	const ats::String wifi_src((*romdir) + "/wifimacaddr.txt");

	const ats::String apn_src("/tmp/config/.apn.txt");
	{
		ats::String o;
		const int err = sqlite3("/mnt/nvram/config/config.db", "select v_value from t_Config where v_app='43656C6C756C6172' and v_key='63617272696572'", o);
		const ats::String& carrier = err ? ats::String() : ats::from_hex(ats::rtrim(o, "\t\n\r "));
		ats::write_file(apn_src, carrier);
	}

	size_t i;

	for(i = 1; i < (sizeof(romdir) / sizeof(*romdir)); ++i)
	{

		if(!ats::dir_exists(romdir[i]))
		{
			const bool auto_make_parents = true;
			ats::mkdir(romdir[i], auto_make_parents, 0700);
		}

		check_ROM_file(sn_src, romdir[i] + "/sn.txt");
		check_ROM_file(eth_src, romdir[i] + "/eth0macaddr.txt");
		check_ROM_file(wifi_src, romdir[i] + "/wifimacaddr.txt");
		ats::String s(romdir[i]);
		check_ROM_file(apn_src, romdir[i] + "/apn.txt", backup_config, &s);
	}

	return 0;
}

static void reboot()
{
	const int pid = fork();

	if(!pid)
	{
		const char* app = "/sbin/rebootw";
		execl(app, app, NULL);
		exit(1);
	}

	int status;
	waitpid(pid, &status, 0);
	exit(2);
}

int main(int argc, char* argv[])
{
	MyData md;
	md.set("watchdog-file", "/dev/watchdog");
	md.set_from_args(argc - 1, argv + 1);

	g_dbg = md.get_int("debug");

	openlog(g_app_name.c_str(), LOG_PID, LOG_USER);
	syslog(LOG_NOTICE, "System WatchDog started");

	if(check_rootfs())
	{
		reboot();
	}

	{
		int failed = 0;
		failed |= check_nvramfs();
		failed |= check_updatefs();

		if(failed)
		{
			reboot();
		}

	}

	if(check_ROM())
	{
		exit(1);
	}

	ats::daemonize();

	pipe(g_p2c);
	pipe(g_c2p);

	{
		const int pid = fork();

		if(0 == pid)
		{
			close(g_p2c[WRITE]);
			close(g_c2p[READ]);
			// To prevent "this" first child process from becoming a Zombie (since parent is not actively reaping processes),
			// detach now, thus causing the init process (PID == 1) to manage the next and all future child processes. The parent
			// will then reap "this" process once.
			ats::detach();
			// At this point, the init process (PID == 1) is the parent.
			watch_process(md);
		}

		close(g_p2c[READ]);
		close(g_c2p[WRITE]);
		// Reap the process "pid" (which shall be the only child process managed by "this" parent). The init process
		// (PID == 1) shall manage all other processes.
		waitpid(pid, 0, 0);
	}

	// System watchdog supports getting information from external processes (via sockets).
	// This information can be used to decide wether to kick the watchdog or to let the
	// system restart.
	//
	// NOTE: The server is started "after" the fork which creates "watch_process". This is done
	//	to reduce the complexity and confusion associated with running global servers and
	//	then fork/pthread_create. This (the main process) will communicate with "watch_process"
	//	via "g_p2c" and "g_c2p" pipes.
	static ServerData server_data;
	{
		ServerData* sd = &server_data;
		init_ServerData(sd, 8);
		sd->m_hook = &md;
		sd->m_cs = local_command_server;
		::start_redstone_ud_server(sd, g_app_name.c_str(), 1);
	}

	for(; examine_system_state();)
	{
		// Tell watch process to "go"
		write(g_p2c[WRITE], "g", 1);

		// Wait for watch process to acknowledge completion
		char c;
		const ssize_t nread = read(g_c2p[READ], &c, 1);

		if(nread <= 0)
		{
			syslog(LOG_ERR, "Watch process did not return data: nread=%d: %d: %s", nread, errno, strerror(errno));
			break;
		}

		if(c != 'k')
		{
			syslog(LOG_ERR, "Watch process returned '%c'", c);
			break;
		}

	}

	return 0;
}
