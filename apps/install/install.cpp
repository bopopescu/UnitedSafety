#include <iostream>
#include <string>
#include <sstream>
#include <map>
#include <vector>

#include <stdio.h>
#include <stdlib.h>

#ifdef PC_MODE
#define LOG_ERR 0
#define LOG_INFO 0
#define LOG_WARNING 0
void syslog(int p_level, const char *p_format, ...)
	__attribute__((format(printf,2,3)));
void syslog(int p_level, const char *p_format, ...)
{
}

#else
#include <syslog.h>
#endif

#include "../include/nand_partition.h"

typedef std::map <const std::string, std::string> StrMap;
typedef std::pair <const std::string, std::string> StrPair;

std::string to_str(int i)
{
	std::stringstream s;
	s << i;
	return s.str();
}

std::string get_value(const StrMap &p_env, const std::string &p_key)
{
	StrMap::const_iterator i = p_env.find(p_key);
	if(p_env.end() == i) return std::string();
	return i->second;
}

int get_int_value(const StrMap &p_env, const std::string &p_key)
{
	return strtol(get_value(p_env, p_key).c_str(), 0, 0);
}

std::string get_part_size(int p_size)
{
	std::stringstream s;
	const int k = 1024;
	const int m = k * 1024;
	if(p_size < k) {
		s << p_size;

	} else if((p_size < m) || (p_size % (k*k))) {
		s << (p_size / k) << "K";

	} else if(p_size >= m) {
		s << (p_size / m) << "M";
	}
	return s.str();
}

std::string get_part_size(const StrMap &p_env, const std::string &p_key, int p_offset = 0)
{
	return get_part_size(get_int_value(p_env, p_key) + p_offset);
}

std::vector <std::string> split_string(char p_c, std::string p_s)
{
	std::vector <std::string> v;
	for(;;) {
		size_t i = p_s.find(p_c);
		if(std::string::npos == i) {
			v.push_back(p_s);
			break;
		}
		v.push_back(p_s.substr(0, i));
		p_s.erase(0, i + 1);
	}
	return v;
}

std::string gen_bootargs(const StrMap &p_env)
{
	std::string s;

	s += "console=ttyS0,115200";
	s += " root=/dev/mtdblock" + get_value(p_env, "rootfs_part_no");

	{
		size_t alloc = 0;
		s += " mtdparts=atmel_nand:";
		std::vector <std::string> parts = split_string(',', get_value(p_env, "part_order"));
		size_t prev_size = 0;
		size_t i;
		for(i = 0; i < parts.size(); ++i) {
			const std::string &p = parts[i];
			const size_t n = get_int_value(p_env, p + "_size");
			if(!n) {
				syslog(LOG_ERR, "Partition \"%s\" has zero size", p.c_str());
				exit(1);
			}
			const size_t addr = get_int_value(p_env, p + "_add");
			if(addr < alloc) {
				syslog(LOG_ERR, "Partition \"%s\" address starts at %u (0x%08X) but %u (0x%08X) allocated",
					p.c_str(),
					addr, addr,
					alloc, alloc);
				exit(1);
			}
			if(addr > alloc) {
				syslog(LOG_INFO, "%u bytes (%u block%s) of padding before partition \"%s\"",
					addr - alloc,
					(addr - alloc) / 0x20000,
					(((addr - alloc) / 0x20000) != 1) ? "s" : "",
					p.c_str());
				s += (i ? "," : "") + get_part_size(n) +
					"@" + get_part_size(addr) +
					"(" + char('a' + i) + ")";
			} else {
				if(addr > 0x20000) {
					syslog(LOG_WARNING, "No padding before partition \"%s\"", p.c_str());
				}
				s += (i ? "," : "") + get_part_size(n) + "(" +
					char('a' + i) + ")";
			}
			alloc =  std::max(alloc, addr + n);
			prev_size = n;
		}
		syslog(LOG_INFO, "%u partition%s and %s bytes allocated in NAND",
			parts.size(),
			(parts.size() != 1) ? "s" : "",
			get_part_size(alloc).c_str());
	}

	s += " rw rootfstype=jffs2";

	return s;
}

int main()
{
	StrMap env;

	// 0: Partition A
	env.insert(StrPair("bootstrap_add", to_str(CONFIG_BOOTSTRAP_ADD)));
	env.insert(StrPair("bootstrap_size", to_str(CONFIG_BOOTSTRAP_SIZE)));

	// 1: Partition B
	env.insert(StrPair("uboot_add", to_str(CONFIG_UBOOT_ADD)));
	env.insert(StrPair("uboot_size", to_str(CONFIG_UBOOT_SIZE)));

	// 2: Partition C
	env.insert(StrPair("def_env_add", to_str(CONFIG_DEF_ENV_ADD)));
	env.insert(StrPair("def_env_size", to_str(CONFIG_DEF_ENV_SIZE)));

	// 3: Partition D
	env.insert(StrPair("env_add", to_str(CONFIG_ENV_ADD)));
	env.insert(StrPair("env_size", to_str(CONFIG_ENV_SIZE)));

	// 4: Partition E
	env.insert(StrPair("def_linux_add", to_str(CONFIG_DEF_LINUX_ADD)));
	env.insert(StrPair("def_linux_size", to_str(CONFIG_DEF_LINUX_SIZE)));

	// 5: Partition F
	env.insert(StrPair("linux_add", to_str(CONFIG_LINUX_ADD)));
	env.insert(StrPair("linux_size", to_str(CONFIG_LINUX_SIZE)));

	// 6: Partition G
	env.insert(StrPair("def_fs_add", to_str(CONFIG_DEF_FS_ADD)));
	env.insert(StrPair("def_fs_size", to_str(CONFIG_DEF_FS_SIZE)));

	// 7: Partition H
	env.insert(StrPair("install_linux_add", to_str(CONFIG_INSTALL_LINUX_ADD)));
	env.insert(StrPair("install_linux_size", to_str(CONFIG_INSTALL_LINUX_SIZE)));

	// 8: Partition I
	env.insert(StrPair("install_fs_add", to_str(CONFIG_INSTALL_FS_ADD)));
	env.insert(StrPair("install_fs_size", to_str(CONFIG_INSTALL_FS_SIZE)));

	// 9: Partition J
	env.insert(StrPair("nvm_add", to_str(CONFIG_NVM_ADD)));
	env.insert(StrPair("nvm_size", to_str(CONFIG_NVM_SIZE)));

	// 10: Partition K
	env.insert(StrPair("fs_add", to_str(CONFIG_FS_ADD)));
	env.insert(StrPair("fs_size", to_str(CONFIG_FS_SIZE)));

	// 11: Partition L
	env.insert(StrPair("data_add", to_str(CONFIG_DATA_ADD)));
	env.insert(StrPair("data_size", to_str(CONFIG_DATA_SIZE)));

	env.insert(StrPair("part_order",
		"bootstrap"
		",uboot"
		",def_env"
		",env"
		",def_linux"
		",linux"
		",def_fs"
		",install_linux"
		",install_fs"
		",nvm"
		",fs"
		",data"
		));

	env.insert(StrPair("rootfs_part_no", "10"));
	env.insert(StrPair("install_kernel_part_no", "7"));
	env.insert(StrPair("install_rootfs_part_no", "8"));

	#ifdef PC_MODE
	{
		const std::string bootargs = gen_bootargs(env);
		printf( "bootargs=%s\n", bootargs.c_str());
	}
	#else
	openlog("install", LOG_PID | LOG_PERROR, LOG_USER);

#if 0
	syslog(LOG_INFO, "Writing Install Kernel to NAND...");
	std::string cmd("flash_eraseall /dev/mtd" + get_value(env, "install_kernel_part_no"));
	int ret = system(cmd.c_str());
	if((-1 == ret) || WEXITSTATUS(ret)) {
		syslog(LOG_ERR, "Command Failed(%d): %s", (-1==ret) ? ret : WEXITSTATUS(ret), cmd.c_str());
		exit(1);
	}
	cmd("nandwrite -p /dev/mtd" + get_value(env, "install_kernel_part_no") + " install_kernel");
	int ret = system(cmd.c_str());
	if((-1 == ret) || WEXITSTATUS(ret)) {
		syslog(LOG_ERR, "Command Failed(%d): %s", (-1==ret) ? ret : WEXITSTATUS(ret), cmd.c_str());
		exit(1);
	}

	syslog(LOG_INFO, "Writing Install Rootfs to NAND...");
	cmd = "flash_eraseall /dev/mtd" + get_value(env, "install_rootfs_part_no");
	ret = system(cmd.c_str());
	if((-1 == ret) || WEXITSTATUS(ret)) {
		syslog(LOG_ERR, "Command Failed(%d): %s", (-1==ret) ? ret : WEXITSTATUS(ret), cmd.c_str());
		exit(1);
	}
	cmd = "nandwrite -p /dev/mtd" + get_value(env, "install_rootfs_part_no") + " install_rootfs.jffs2";
	ret = system(cmd.c_str());
	if((-1 == ret) || WEXITSTATUS(ret)) {
		syslog(LOG_ERR, "Command Failed(%d): %s", (-1==ret) ? ret : WEXITSTATUS(ret), cmd.c_str());
		exit(1);
	}
#endif

	syslog(LOG_INFO, "Checking for update image");
	{
		FILE *f = fopen("/home/root/update/fw.bin", "r");
		if(!f) {
			syslog(LOG_ERR, "No fw.bin found");
			exit(1);
		}
		fclose(f);
	}
	chdir("/home/root/update");

	syslog(LOG_INFO, "Decrypting update image...");
	{
		const int ret = system(
			"gpg"
				" --no-tty"
				" --batch"
				" --yes"
				" --passphrase-file /home/root/.ppf/.txt"
				" -d /home/root/update/fw.bin"
				" | tar -C /home/root/update -x fw.sig fw.bz2"
				);
		if((-1 == ret) || WEXITSTATUS(ret)) {
			syslog(LOG_ERR, "Failed to decrypt fw.bin (error %d)", (-1 == ret) ? ret : WEXITSTATUS(ret));
			system("rm -f /home/root/update/*");
			exit(1);
		}
		system("rm -f /home/root/update/fw.bin");
	}

	syslog(LOG_INFO, "Verifying update image signature...");
	{
		const int ret = system(
			"gpg"
				" --no-tty"
				" --batch"
				" --yes"
				" --passphrase-file /home/root/.ppf/.txt"
				" --verify /home/root/update/fw.sig /home/root/update/fw.bz2"
				);
		if((-1 == ret) || WEXITSTATUS(ret)) {
			syslog(LOG_ERR, "Failed to verify fw.bz2 (error %d)", (-1 == ret) ? ret : WEXITSTATUS(ret));
			system("rm -f /home/root/update/*");
			exit(1);
		}
	}

	syslog(LOG_INFO, "Preparing firmware update...");
	{
		system("mkdir -p /home/root/update/fw");
		system("tar -C /home/root/update/fw -xjf /home/root/update/fw.bz2");
		system("rm -f /home/root/update/fw.bz2 /home/root/update/fw.sig");
	}

	syslog(LOG_INFO, "Running firmware update...");
	{
		const int ret = system("/home/root/update/fw/start.sh");
		if((-1 == ret) || (WEXITSTATUS(ret))) {
			syslog(LOG_ERR, "Failed to execute start.sh (%d)", (-1 == ret) ? ret : WEXITSTATUS(ret));
			system("rm -rf /home/root/update/*");
			exit(1);
		}
	}

#if 0
	syslog(LOG_INFO, "Modifying boot procedure to boot Install Kernel...");
	{
		const std::string bootargs = gen_bootargs(env);
		printf( "bootargs=%s\n", bootargs.c_str());
		const std::string cmd = "set_env bootargs '" + bootargs + "'";
		const int ret = system(("set_env bootargs '" + bootargs + "'").c_str());
		if((-1 == ret) || WEXITSTATUS(ret)) {
			syslog(LOG_ERR, "Command Failed(%d): %s", (-1==ret) ? ret : WEXITSTATUS(ret), cmd.c_str());
			exit(1);
		}
	}

	syslog(LOG_INFO, "Rebooting device...");
	{
		system("reboot");
	}
#endif
	#endif

	return 0;
}
