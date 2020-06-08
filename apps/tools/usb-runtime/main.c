#define _XOPEN_SOURCE 600
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <ftw.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <utime.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <grp.h>
#include <pwd.h>
#include <syslog.h>
#include <ctype.h>

static int unlink_cb(const char* p_fpath, const struct stat* p_sb, int p_typeflag, struct FTW* p_ftwbuf)
{
	const int rv = remove(p_fpath);

	if(rv)
	{
		perror(p_fpath);
	}

	return rv;
}

static void rm_rf(const char* p_path)
{
	nftw(p_path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
}

static int file_exists(const char* p_fname)
{
	struct stat s;
	return stat(p_fname, &s) ? 0 : 1;
}

static int mkpath(const char* p_path)
{
	const int pid = fork();

	if(!pid)
	{
		execl("/bin/mkdir", "/bin/mkdir", "-p", p_path, NULL);
		exit(1);
	}

	if(pid > 0)
	{
		int status;
		waitpid(pid, &status, 0);

		if(WIFEXITED(status) && (0 ==  WEXITSTATUS(status)))
		{
			return 0;
		}

	}

	return 1;
}

static void modprobe(const char* p_name)
{
	const int pid = fork();

	if(!pid)
	{
		execl("/sbin/modprobe", "/sbin/modprobe", p_name, NULL);
		exit(1);
	}

	if(pid > 0)
	{
		waitpid(pid, 0, 0);
	}

}

static void modprobe_r(const char* p_name)
{
	const int pid = fork();

	if(!pid)
	{
		execl("/sbin/modprobe", "/sbin/modprobe", "-r", p_name, NULL);
		exit(1);
	}

	if(pid > 0)
	{
		waitpid(pid, 0, 0);
	}

}

static int mount_usb(const char* p_dev, const char* p_target, const char* p_fs, const char* p_option)
{

	if((!p_fs) || ('\0' == p_fs[0]))
	{
		modprobe("vfat");

		if(mount(p_dev, p_target, "vfat", 0, p_option))
		{
			modprobe_r("vfat");
			modprobe("ext4");

			if(mount(p_dev, p_target, "ext4", 0, 0))
			{
				modprobe_r("ext4");
				modprobe("ntfs");

				if(mount(p_dev, p_target, "ntfs", 0, 0))
				{
					modprobe_r("ntfs");
					return errno;
				}

			}

		}

		return 0;
	}

	modprobe(p_fs);
	return mount(p_dev, p_target, p_fs, 0, p_option) ? errno : 0;
}

static char* tr(char* p_src, char p_old, char p_new)
{
	char* s = p_src;

	while((s = strchr(s, p_old)) != NULL)
	{
		*(s++) = p_new;
	}

	return s;
}

// ATS FIXME: Function prototype to work around compiler warning. Where is "initgroups" declared?
//
//	main.c:279: warning: implicit declaration of function 'initgroups'
//
// Prototype from "man -s3 initgroups", release 3.35, 2007-07-26
int initgroups(const char *user, gid_t group);

#define UPDATE_TMP_DIR "/tmp/.usb-autorun-ud-tmp-dir"
#define UPDATE_FW_DIR "/tmp/.usb-autorun-ud-fw-dir"

static	const char* g_prepare_sw_update_env[] =
{
	"REDSTONE_AUTOUPDATE=1",
	"UPDATE_TMP_DIR=" UPDATE_TMP_DIR,
	"UPDATE_FW_DIR=" UPDATE_FW_DIR,
	"UPDATE_FLAG=/tmp/.usb-autorun-ready",
	"UPDATE_AUTO_FLAG=/tmp/.usb-autorun-autoready",
	NULL
};

// Description: Checks that the app name "p_name" only contains valid characters, and returns 1 if it does, and 0
//	if it doesn't.
//
//	A valid app name cannot be the empty string.
//
// Returns 1 if "p_name" only contains the following characters: [0-9A-Za-z]. 0 is returned otherwise.
static int check_app_name(const char* p_name)
{
	int valid = 0;

	for(;*p_name; ++p_name)
	{
		const char c = *p_name;

		if(!(isalnum(c) || ('_' == c) || ('-' == c)))
		{
			return 0;
		}

		valid = 1;
	}

	return valid;
}

static void gen_key_value_pair(char** p_key, char** p_val, char* p_setting)
{
	*p_key = p_setting;
	*p_val = 0;

	while(*p_setting)
	{
		const char c = *(p_setting++);

		if('=' == c)
		{
			*(p_setting - 1) = '\0';
			*p_val = p_setting;
			return;
		}

	}

	return;
}

static int decrypt_and_run(const char* p_run_dir, const char* p_app_name)
{
	syslog(LOG_INFO, "Encrypted application \"%s\" found, checking/verifying...", p_app_name);
	const int pid = fork();

	if(!pid)
	{
		chdir(p_run_dir);
		execle("/usr/bin/prepare-sw-update", "/usr/bin/prepare-sw-update", p_app_name, NULL, g_prepare_sw_update_env);
		syslog(LOG_ERR, "%s,%d: execle failed. (%d) %s", __FILE__, __LINE__, errno, strerror(errno));
		exit(1);
	}

	if(pid > 0)
	{
		int status;
		waitpid(pid, &status, 0);

		if(WIFEXITED(status) && (0 ==  WEXITSTATUS(status)))
		{
			syslog(LOG_INFO, "Running start script (\"%s\")...", p_app_name);
			const int pid = fork();

			if(!pid)
			{
				chdir(UPDATE_FW_DIR);
				char app[1024];
				snprintf(app, sizeof(app) - 1, "%s/%s", UPDATE_FW_DIR, p_app_name);
				app[sizeof(app) - 1] = '\0';
				execl(app, app, NULL);
				exit(1);
			}

			if(pid > 0)
			{
				int status;
				waitpid(pid, &status, 0);

				if(!(WIFEXITED(status) && (0 ==  WEXITSTATUS(status))))
				{
					syslog(LOG_ERR, "Start application exited with status=%08X", status);
					return -1;
				}

			}

		}
		else
		{
			syslog(LOG_ERR, "Check/verification failed for start application, not running");
			return -2;
		}

	}

	return 0;
}

static int run_unencrypted(const char* p_run_dir, const char* p_app_name)
{
	syslog(LOG_INFO, "Running unencrypted application \"%s\"...", p_app_name);
	const int pid = fork();

	if(!pid)
	{
		chdir(p_run_dir);
		char app[1024];
		snprintf(app, sizeof(app) - 1, "%s/%s", p_run_dir, p_app_name);
		app[sizeof(app) - 1] = '\0';
		execl(app, app, NULL);
		syslog(LOG_ERR, "Failed to run start application: (%d) %s", errno, strerror(errno));
		exit(1);
	}

	if(pid > 0)
	{
		int status;
		waitpid(pid, &status, 0);

		if(WIFEXITED(status))
		{
			const int exit_status = WEXITSTATUS(status);

			if(exit_status)
			{
				syslog(LOG_ERR, "\"start\" application exited with status %d", exit_status);
			}

		}
		else if(WIFSIGNALED(status))
		{
			const int sig = WTERMSIG(status);
			syslog(LOG_ERR, "\"start\" application exited with signal %d", sig);
			return -1;
		}
		else
		{
			syslog(LOG_ERR, "\"start\" application exited abnormally: status=0x%08X", status);
			return -2;
		}

	}

	return 0;
}

static void add_action(const char* p_autorun, const char* p_dev, const char* p_fs)
{

	if(p_fs && p_fs[0])
	{
		syslog(LOG_INFO, "USB Mass Storage \"%s\" (filesystem=\"%s\") inserted", p_dev, p_fs);
	}
	else
	{
		syslog(LOG_INFO, "USB Mass Storage \"%s\" inserted", p_dev);
	}

	if(!file_exists(p_autorun))
	{
		mkpath(p_autorun);
	}

	syslog(LOG_INFO, "Mounting USB Mass Storage \"%s\" (%s) on to \"%s\"", p_dev, p_fs, p_autorun);

	if(mount_usb(p_dev, p_autorun, p_fs, 0))
	{
		syslog(LOG_ERR, "Failed to mount \"%s\" (%s) on \"%s\"", p_dev, p_fs, p_autorun);
		rm_rf(p_autorun);
		modprobe_r(p_fs);
		exit(1);
	}

	char run_dir[1024];
	snprintf(run_dir, sizeof(run_dir) - 1, "%s/redstone/autorun", p_autorun);
	run_dir[sizeof(run_dir) - 1] = '\0';

	// AWARE360 FIXME: Due to time constraints, a high level shell script is used to implement the
	//	authorization system. When time permits, reimplement in C for efficiency.
	const char* script =
		"#!/bin/sh\n"
		"cd \"$MY_RUN_DIR\"\n"
		"workdir=\"/tmp/.usb$$\"\n"
		"mkdir \"$workdir\"\n"
		"\n"
		"finish()\n"
		"{\n"
		"	rm -rf \"$workdir\"\n"
		"	exit $1\n"
		"}\n"
		"\n"
		// Decrypt the authorization file.
		"emsg=`gpg --no-tty --passphrase=\"\\`cat /home/root/.ppf/.txt\\`\" --output \"$workdir/auth.tar\" -d auth 2>&1`\n"
		"ret=$?\n"
		"\n"
		"if [ 0 != $ret ];then\n"
		"	logger \"$ret: Bad auth file: $emsg\"\n"
		"	finish 1\n"
		"fi\n"
		"\n"
		// Extract the manifest and application signatures.
		"cd \"$workdir\"\n"
		"\n"
		"emsg=`tar -xf './auth.tar' 2>&1`\n"
		"ret=$?\n"
		"\n"
		"if [ 0 != $ret ];then\n"
		"	logger \"$ret: Untar failed: $emsg\"\n"
		"	finish 1\n"
		"fi\n"
		"\n"
		// Verify manifest signature.
		"emsg=`gpg --no-tty --verify './manifest.sig' \"$MY_RUN_DIR/manifest.txt\" 2>&1`\n"
		"ret=$?\n"
		"\n"
		"if [ 0 != $ret ];then\n"
		"        logger \"$ret: Bad manifest.txt: $emsg\"\n"
		"        finish 1\n"
		"fi\n"
		"\n"

		"compipe=\"/proc/$$/fd/$APP_NAME_PIPE\"\n"
		"exec 3>\"$compipe\"\n"

		// Get the application name. "start=<app name>" specifies an encrypted application.
		// "start_app=<app name>" specifies a non-encrypted application.
		"app=`grep '^start=' \"$MY_RUN_DIR/manifest.txt\"|sed 's/.*=//'`\n"
		"\n"
		"if [ '' == \"$app\" ];then\n"
		"	app=`grep '^start_app=' \"$MY_RUN_DIR/manifest.txt\"|sed 's/.*=//'`\n"
		"	echo \"decrypt=0\" >&3\n"
		"fi\n"

		// Verify application signature.
		"\n"
		"emsg=`gpg --no-tty --verify './start-app.sig' \"$MY_RUN_DIR/$app\" 2>&1`\n"
		"ret=$?\n"
		"\n"
		"if [ 0 != $ret ];then\n"
		"	logger \"$ret: Bad app: $emsg\"\n"
		"	finish 1\n"
		"fi\n"
		"\n"
		// Send name of application to parent process (usb-runtime).
		"echo \"app_name=$app\" >&3\n"
		"echo 3>&-\n"
		"finish 0\n";

	char auth_file[1024];
	snprintf(auth_file, sizeof(auth_file) - 1, "%s/auth", run_dir);
	auth_file[sizeof(auth_file) - 1] = '\0';

	if(file_exists(auth_file))
	{
		syslog(LOG_INFO, "Found authorization file, processing...");

		int app_name_pipe[2];
		pipe(app_name_pipe);

		setenv("MY_RUN_DIR", run_dir, 1);
		const int pid = fork();

		if(!pid)
		{
			close(app_name_pipe[0]);
			{
				char s[256];
				snprintf(s, sizeof(s) - 1, "%d", app_name_pipe[1]);
				s[sizeof(s) - 1] = '\0';
				setenv("APP_NAME_PIPE", s, 1);
			}

			setenv("MY_RUN_DIR", run_dir, 1);

			const char* app = "/bin/sh";
			execl(app, app, "-c", script, NULL);
			syslog(LOG_ERR, "%s,%d: execl failed. (%d) %s", __FILE__, __LINE__, errno, strerror(errno));
			exit(1);
		}

		close(app_name_pipe[1]);

		if(pid > 0)
		{
			int status;
			waitpid(pid, &status, 0);

			if(WIFEXITED(status) && (0 ==  WEXITSTATUS(status)))
			{
				const int max_len = 2048;
				char setting[max_len];
				char app_name[max_len];
				int remain = max_len;
				int decrypt = 1;

				{

					char buf[1024];
					char* s = setting;
					setting[sizeof(setting) - 1] = '\0';

					for(;;)
					{
						ssize_t nread = read(app_name_pipe[0], buf, sizeof(buf));

						if(nread <= 0)
						{
							*s = '\0';
							break;
						}

						char* b = buf;

						while(nread--)
						{
							const char c = *(b++);

							if('\n' == c)
							{
								char *key;
								char *val;
								gen_key_value_pair(&key, &val, setting);

								if(!strcmp("app_name", key))
								{
									strncpy(app_name, val, sizeof(app_name) - 1);
									app_name[sizeof(app_name) - 1] = '\0';
								}
								else if(!strcmp("decrypt", key))
								{
									decrypt = strtol(val, 0, 0);
								}

								s = setting;
								remain = sizeof(setting);
								continue;
							}

							if(remain > 1)
							{
								*(s++) = c;
								--remain;
							}

						}

					}

					if(!strcmp("", app_name))
					{
						strncpy(app_name, "start", sizeof(app_name) - 1);
						app_name[sizeof(app_name) - 1] = '\0';
					}

				}

				if(check_app_name(app_name))
				{
					if(decrypt)
					{
						decrypt_and_run(run_dir, app_name);
					}
					else
					{
						run_unencrypted(run_dir, app_name);
					}

				}
				else
				{
					syslog(LOG_ERR, "App name \"%s\" is not valid", app_name);
				}

			}

		}
		else
		{
			syslog(LOG_INFO, "Failed to authorize start application, will not run");
		}

		close(app_name_pipe[0]);
	}

}

static void remove_action(const char* p_autorun, const char* p_dev, const char* p_fs)
{

	if(strncmp("/dev/sd", p_dev, 7))
	{
		exit(0);
	}

	if((p_dev[7] < 'a') || (p_dev[7] > 'z'))
	{
		exit(0);
	}

	if(p_fs && p_fs[0])
	{
		syslog(LOG_INFO, "USB Mass Storage \"%s\" (%s) has been removed", p_dev, p_fs);
	}
	else
	{
		syslog(LOG_INFO, "USB Mass Storage \"%s\" has been removed", p_dev);
	}

	if(file_exists(p_autorun))
	{

		if(!umount(p_autorun))
		{
			rm_rf(p_autorun);

			char name[1024];
			strncpy(name, p_dev, sizeof(name) - 1);
			name[sizeof(name) - 1] = '\0';
			tr(name, '/', '-');

			char stop_script[1024];
			snprintf(stop_script, sizeof(stop_script) - 1, "/tmp/usb-stop%s", name);
			stop_script[sizeof(stop_script) - 1] = '\0';

			if(file_exists(stop_script))
			{
				syslog(LOG_INFO, "Stop application found, checking/verifying...");
				const int pid = fork();

				if(!pid)
				{
					execle("/usr/bin/prepare-sw-update", "/usr/bin/prepare-sw-update", stop_script, NULL, g_prepare_sw_update_env);
					syslog(LOG_ERR, "%s,%d: execle failed. (%d) %s", __FILE__, __LINE__, errno, strerror(errno));
					exit(1);
				}

				if(pid > 0)
				{
					int status;
					waitpid(pid, &status, 0);

					if(WIFEXITED(status) && (0 ==  WEXITSTATUS(status)))
					{
						syslog(LOG_INFO, "Running stop application...");
						const int pid = fork();

						if(!pid)
						{
							chdir(UPDATE_FW_DIR);
							execl(UPDATE_FW_DIR "/start.sh", UPDATE_FW_DIR "/start.sh", NULL);
							exit(1);
						}

						if(pid > 0)
						{
							int status;
							waitpid(pid, &status, 0);

							if(!(WIFEXITED(status) && (0 ==  WEXITSTATUS(status))))
							{
								syslog(LOG_ERR, "Stop application exited with status=%08X", status);
							}

						}

					}
					else
					{
						syslog(LOG_ERR, "Check/verification failed for stop application, not running");
					}

				}

			}

		}

	}

}

int main(int argc, char* argv[])
{
	openlog("usb-runtime", LOG_PID, LOG_USER);
	const char* dev = getenv("DEVNAME");

	if((!dev) || ('\0' == dev[0]))
	{
		return 1;
	}

	const char* action = getenv("ACTION");
	const char* fs = getenv("ID_FS_TYPE");

	char autorun[1024];
	snprintf(autorun, sizeof(autorun) - 1, "/tmp/usb-autorun%s", dev);
	autorun[sizeof(autorun) - 1] = '\0';

	if(!strcmp("add", action))
	{
		add_action(autorun, dev, fs);
		system("/usr/bin/usb-monitor &");
	}
	else if(!strcmp("remove", action))
		remove_action(autorun, dev, fs);

	return 0;
}
