// Description: Displays basic state information for the TRULink.
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ats-common.h"
#include "ConfigDB.h"
#include "command_line_parser.h"
#include "socket_interface.h"
#include "RedStone_IPC.h"
#include "feature.h"
#include "TRULinkSocketQuery.h"

static void read_output(int p_stdout_pipe[2], std::stringstream* p_o)
{
	close(p_stdout_pipe[1]);

	for(;;)
	{
		char buf[1024];
		ssize_t nread = read(p_stdout_pipe[0], buf, sizeof(buf));

		if(nread <= 0)
		{
			break;
		}

		if(p_o)
		{
			p_o->write(buf, std::streamsize(nread));
		}

	}

	close(p_stdout_pipe[0]);
}

static int get_firmware_version(std::stringstream& p_o)
{
	int p[2];
	pipe(p);
	const int pid = fork();

	if(pid < 0)
	{
		return -1;
	}

	if(!pid)
	{
		close(p[0]);
		dup2(p[1], STDOUT_FILENO);
		const char* app = "/usr/bin/i2cget";
		execl(app, app, "-y", "0", "0x30", "0x11", "w", (char*)NULL);
		exit(1);
	}

	read_output(p, &p_o);
	waitpid(pid, 0, 0);
	return 0;
}
static int get_hardware_version(std::stringstream& p_o)
{
	int p[2];
	pipe(p);
	const int pid = fork();

	if(pid < 0)
	{
		return -1;
	}

	if(!pid)
	{
		close(p[0]);
		dup2(p[1], STDOUT_FILENO);
		const char* app = "/usr/bin/hid";
		execl(app, app, (char*)NULL);
		exit(1);
	}

	read_output(p, &p_o);
	waitpid(pid, 0, 0);
	return 0;
}

static const char* get_interface_addr_by_ioctl(const ats::String& p_iface, ats::String& p_emsg)
{
	const int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

	if(fd < 0)
	{
		ats_sprintf(&p_emsg, "(%d) %s", errno, strerror(errno));
		return ats::g_empty.c_str();
	}

	struct ifreq ifr;
	const size_t max_len = IFNAMSIZ - 1;
	const size_t len = (p_iface.length() < max_len) ? p_iface.length() : max_len;
	memcpy(ifr.ifr_name, p_iface.c_str(), len);
	ifr.ifr_name[len] = '\0';
	const int ret = ioctl(fd, SIOCGIFADDR, &ifr);
	const int err = errno;
	close(fd);

	if(-1 == ret)
	{
		ats_sprintf(&p_emsg, "(%d) %s", err, strerror(err));
		return ats::g_empty.c_str();
	}

	p_emsg.clear();
	return inet_ntoa(((struct sockaddr_in*)&(ifr.ifr_addr))->sin_addr);
}

const char* get_ra0_essid();

static void print_tabbed(FILE* p_f, const ats::String& p_s, const ats::String& p_tab)
{
	const char* s = p_s.c_str();

	for(;;)
	{
		const char* p = strchr(s, '\n');

		if(p)
		{
			fwrite(s, 1, (p + 1) - s, p_f);
			s = p + 1;

			if(*s)
			{
				fwrite(p_tab.c_str(), 1, p_tab.length(), p_f);
			}

		}
		else
		{
			fwrite(s, 1, (p_s.length() + p_s.c_str()) - s, p_f);
			break;
		}

	}

}

static int ubiattach()
{
	const int pid = fork();

	if(pid < 0)
	{
		return -1;
	}

	if(!pid)
	{
		const int dev_null_fd = open("/dev/null", O_WRONLY);
		dup2(dev_null_fd, STDOUT_FILENO);
		dup2(dev_null_fd, STDERR_FILENO);
		const char* app = "/usr/bin/ubiattach";
		execl(app, app, "/dev/ubi_ctrl", "-m", "8", "-d", "8", (char*)NULL);
		exit(1);
	}

	int status;
	waitpid(pid, &status, 0);
	return (WIFEXITED(status) ? WEXITSTATUS(status) : -2);
}

static int mount_ubi(const char* p_mount_point)
{
	const int pid = fork();

	if(pid < 0)
	{
		return -1;
	}

	if(!pid)
	{
		const char* app = "/bin/mount";
		execl(app, app, "-t", "ubifs", "ubi8_0", p_mount_point, "-o", "ro", (char*)NULL);
		exit(1);
	}

	int status;
	waitpid(pid, &status, 0);
	return (WIFEXITED(status) ? WEXITSTATUS(status) : -2);
}

static int umount_ubi(const char* p_mount_point)
{
	const int pid = fork();

	if(pid < 0)
	{
		return -1;
	}

	if(!pid)
	{
		const char* app = "/bin/umount";
		execl(app, app, p_mount_point, (char*)NULL);
		exit(1);
	}

	int status;
	waitpid(pid, &status, 0);
	return (WIFEXITED(status) ? WEXITSTATUS(status) : -2);
}

static int detach_ubi()
{
	const int pid = fork();

	if(pid < 0)
	{
		return -1;
	}

	if(!pid)
	{
		const char* app = "/usr/bin/ubidetach";
		execl(app, app, "/dev/ubi_ctrl", "-m", "8", (char*)NULL);
		exit(1);
	}

	int status;
	waitpid(pid, &status, 0);
	return (WIFEXITED(status) ? WEXITSTATUS(status) : -2);
}

static int get_factory_version(ats::String& p_version)
{

	if(0 == ubiattach())
	{
		char mount_point[512];
		snprintf(mount_point, sizeof(mount_point) - 1, "/tmp/factory.%d", getpid());
		mount_point[sizeof(mount_point) - 1] = '\0';

		if(0 == mkdir(mount_point, 0500))
		{

			if(0 == mount_ubi(mount_point))
			{
				ats::get_file_line(p_version, mount_point + ats::String("/version"), 1);
				umount_ubi(mount_point);
			}

			rmdir(mount_point);
		}

		detach_ubi();
		return 0;
	}

	return -1;
}

int main(int argc, char* argv[])
{
	const char* col_off = "\x1b[0m";
	const char* col_red = "\x1b[1;31m";
	const char* col_green = "\x1b[32m";
	const char* col_yellow = "\x1b[33m";
	db_monitor::ConfigDB db;

	printf("Current Time: %s%s%s\n", col_green, (ats::human_readable_date()).c_str(), col_off);

	{
		const ats::String& s = db.GetValue("RedStone", "Owner");
		printf("\nOwner: %s%s%s\n", col_green, s.c_str(), col_off);
	}
	{
		ats::String line;
		ats::get_file_line(line, "/version", 1);
		printf("\nVersion: %s%s%s\n", col_green, line.c_str(), col_off);
	}

	if((argc >= 2) && (0 == strcmp("-d", argv[1])))
	{
		ats::String version;

		if(get_factory_version(version))
		{
			printf("Default Version: %sError querying factory default version%s\n", col_red, col_off);
		}
		else
		{
			printf("Default Version: %s%s%s\n", col_yellow, version.c_str(), col_off);
		}

	}
	else
	{
		ats::String line;
		ats::get_file_line(line, "/mnt/nvram/config/FactoryVersion", 1);
		printf("\nDefault Version: %s%s%s\n", col_green, line.c_str(), col_off);
	}

	{
		std::stringstream s;
		get_hardware_version(s);
		printf("Hardware Version: %s%s%s", col_green, s.str().c_str(), col_off);
	}
	{
		std::stringstream s;
		get_firmware_version(s);
		printf("Firmware Version: %s%s%s", col_green, s.str().c_str(), col_off);
	}

	{
		const ats::String& s = db.GetValue("wakeup", "mask");
		printf("\nIgnition Source: %s%s%s\n", col_green, s.c_str(), col_off);
	}

	{
		const ats::String& s = db.GetValue("RedStone", "KeepAwakeMinutes");
		printf("Keep Awake: %s%s%s\n", col_green, s.c_str(), col_off);
	}

	{
		ats::String line;
		ats::get_file_line(line, "/tmp/logdir/wakeup.txt", 1);
		printf("\nWoke up on: %s%s%s\n", col_green, line.c_str(), col_off);
	}

	{
		ats::String s;
		ats::read_file("/tmp/logprev/reboot.txt", s);
		const char* header = "\nPowered Down On:\n";

		if(s.empty())
		{
			printf("%s", header);
		}
		else
		{
			printf("%s\t%s", header, col_yellow);
			print_tabbed(stdout, s, "\t");
			printf("%s\n", col_off);
		}

	}

	{
		ats::String emsg;
		const char* ip = get_interface_addr_by_ioctl("ppp0", emsg);
		printf("PPP0: %s%s%s\n", emsg.empty() ? col_yellow : col_red, emsg.empty() ? ip : emsg.c_str(), col_off);

		ip = get_interface_addr_by_ioctl("ra0", emsg);
		printf("Wireless: %s%s%s\n", emsg.empty() ? col_green : col_red, emsg.empty() ? ip : emsg.c_str(), col_off);

		ip = get_interface_addr_by_ioctl("eth0", emsg);
		printf("Ethernet: %s%s%s\n", emsg.empty() ? col_green : col_red, emsg.empty() ? ip : emsg.c_str(), col_off);
	}

	{
		const ats::String& s = db.GetValue("Cellular", "carrier");
		printf("\nCellular APN: %s\n", s.c_str());
	}

	{
		REDSTONE_IPC ipc;
		printf("Current RSSI: %s%d%s\n\n", col_green, ipc.Rssi(), col_off);
	}

	{
		const ats::String& h = db.GetValue("packetizer-cams", "host");
		const ats::String& p = db.GetValue("packetizer-cams", "port");

		if(!(h.empty() && p.empty()))
		{
			printf("CAMS host: %s%s:%s%s\n", col_green, h.c_str(), p.c_str(), col_off);
		}

	}

	{
		printf("Calamps database records: %s", col_green);
		db_monitor::DBMonitorContext db("cams_db", "/mnt/update/database/cams.db");
		db.query("select count(mid) from message_table");

		if(db.Table().empty())
		{
			printf("0\n%s", col_off);
		}
		else
		{
			size_t i;

			for(i = 0; i < db.Table().size(); ++i)
			{
				const db_monitor::ResultRow& r = db.Table()[i];
				size_t i;

				for(i = 0; i < r.size(); ++i)
				{
					printf("%s", r[i].c_str());
				}

				printf("\n");
			}

			printf("%s", col_off);
		}

	}

	printf("\nOutputs:\n");
	{
		FeatureQuery fq;
		ats::StringList on;
		ats::StringList off;
		fq.get_features(on, off);
		size_t i;

		for(i = 0; i < on.size(); ++i)
		{
			const ats::String& s = on[i];

			if(s.empty())
			{
				continue;
			}

			if(0 == s.find("\"packetizer"))
			{
				printf("\t%.*s - %sON%s\n", int(s.size()) - 2, s.c_str() + 1, col_green, col_off);
			}

		}

		for(i = 0; i < off.size(); ++i)
		{
			const ats::String& s = off[i];

			if(s.empty())
			{
				continue;
			}

			if(0 == s.find("\"packetizer"))
			{
				printf("\t%.*s - %sOFF%s\n", int(s.size()) - 2, s.c_str() + 1, col_red, col_off);
			}

		}

	}

	{
		printf("\nWiFi SSID: %s%s%s\n", col_green, get_ra0_essid(), col_off);
	}

	{
		printf("\nWiFi Leases:");
		FILE* f = fopen("/var/state/dhcp/dhcpd.leases", "r");

		if(f)
		{
			ats::ReadDataCache_FILE rdc(f);
			bool first_color = true;

			for(;;)
			{
				ats::String line;

				if(get_file_line(line, rdc, 1))
				{
					break;
				}

				if(ats::String::npos != line.find("client"))
				{
					printf("\n\t%s%s", first_color ? col_green : "", line.c_str());
					first_color = false;
				}

			}

			fclose(f);
			printf("%s\n", first_color ? "" : col_off);
		}

	}

	{
		ats::TRULinkSocketQuery q("power-monitor");
		const ats::String& resp = q.query("PowerMonitorStateMachine stats");
		const char* header = "Power Monitor Status:\n";

		if(resp.empty())
		{
			printf("%s", header);
		}
		else
		{
			printf("%s\t%s", header, col_yellow);
			print_tabbed(stdout, resp, "\t");
			printf("%s\n", col_off);
		}

	}

	return 0;
}
