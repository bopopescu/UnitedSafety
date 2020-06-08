#define _XOPEN_SOURCE 600
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
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
#include <stdarg.h>
#include <semaphore.h>
#include <termios.h>

#include "ats-common.h"
#include "atslogger.h"
#include "db-monitor.h"
#include "checksum.h"

ATSLogger g_log;
static bool g_loop = false;

static const ats::String g_app_name("gps-sim");
ats::String modifyGPGGAwithCurrentTime(ats::String& str);
ats::String modifyGNRMCwithCurrentDate(ats::String&);

static void print_usage()
{
  fprintf(stderr, "Usage: gps-sim <filename> [loop]\n");
}

static int ptyMasterOpen(char *slaveName, size_t snLen)
{
	int masterFd, savedErrno;
	char *p;

	masterFd = posix_openpt(O_RDWR | O_NOCTTY);         /* Open pty master */
	if (masterFd == -1)
		return -1;

	if (grantpt(masterFd) == -1) {              /* Grant access to slave pty */
		savedErrno = errno;
		close(masterFd);                        /* Might change 'errno' */
		errno = savedErrno;
		return -1;
	}

	if (unlockpt(masterFd) == -1) {             /* Unlock slave pty */
		savedErrno = errno;
		close(masterFd);                        /* Might change 'errno' */
		errno = savedErrno;
		return -1;
	}

	p = ptsname(masterFd);                      /* Get slave pty name */
	if (p == NULL) {
		savedErrno = errno;
		close(masterFd);                        /* Might change 'errno' */
		errno = savedErrno;
		return -1;
	}

	if (strlen(p) < snLen) {
		strncpy(slaveName, p, snLen);
	} else {                    /* Return an error if buffer too small */
		close(masterFd);
		errno = EOVERFLOW;
		return -1;
	}

	return masterFd;
}

int set_slave_attr(const ats::String& slaveName)
{
	int fds = open(slaveName.c_str(), O_RDONLY);

	struct termios slave_orig_term_settings; // Saved terminal settings
	struct termios new_term_settings; // Current terminal settings

	// Save the default parameters of the slave side of the PTY
	tcgetattr(fds, &slave_orig_term_settings);

	// Set raw mode on the slave side of the PTY
	new_term_settings = slave_orig_term_settings;
	speed_t speed = B115200;
	cfsetispeed(&new_term_settings, speed);
	cfsetospeed(&new_term_settings, speed);
	cfmakeraw (&new_term_settings);
	tcsetattr (fds, TCSANOW, &new_term_settings);

	// The slave side of the PTY becomes the standard input and outputs of the child process
	close(0); // Close standard input (current terminal)
	close(1); // Close standard output (current terminal)
	close(2); // Close standard error (current terminal)

	dup(fds); // PTY becomes standard input (0)
	dup(fds); // PTY becomes standard output (1)
	dup(fds); // PTY becomes standard error (2)

	close(fds);

	return 0;
}

static int feed_data(const ats::String& nmeafile, const int masterfd)
{
	FILE* f = fopen(nmeafile.c_str(), "r");

	if(!f)
	{
		return -errno;
	}

	size_t offset = 0;

	ats::ReadDataCache_FILE rdc(f);

	ats::String p_des;

	while(1)
	{
		ats::String p_desLine;

		for(;;)
		{
			const int c = rdc.getc();

			++offset;

			if(c < 0)
			{
				fclose(f);
				return c;
			}

			p_desLine += c;

			if('\n' == c || feof(f))
			{
				break;
			}

		}

		//ats_logf(ATSLOG(0),"p_desLine %s", p_desLine.c_str());

		if(fseek(f, offset, SEEK_SET))
		{
			fclose(f);
			return -errno;
		}

		{
			if(p_desLine[0] != '$' || p_desLine[6] != ',')
				continue;

			bool end = false;

			if(p_desLine.find("$GPGGA") == 0)
			{
				p_des.clear();
				p_desLine = modifyGPGGAwithCurrentTime(p_desLine);
			}
			else if(p_desLine.find("$GNRMC") == 0)
			{
				p_des.clear();
				p_desLine = modifyGNRMCwithCurrentDate(p_desLine);
			}
			else if(p_desLine.find("$GPVTG,") == 0)
			{
				end = true;
			}

			p_des += p_desLine;

			if(!end)
				continue;

			write(masterfd, p_des.c_str(), p_des.size());

			sleep(1);
		}
	}

	fclose(f);
	return 0;
}

ats::String modifyGNRMCwithCurrentDate(ats::String& str)
{
	time_t     now;
	struct tm  *ts;
	now = ::time(NULL);
	ts = localtime(&now);
	ats::String fixDate;
	ats_sprintf(&fixDate, "%.2d%.2d%.2d", ts->tm_mday, ts->tm_mon+1, (ts->tm_year-100));
	ats::String fixTime;
	ats_sprintf(&fixTime, "%.2d%.2d%.2d.000", ts->tm_hour, ts->tm_min,(short)(ts->tm_sec));

	ats::StringList m;
	ats::split(m, str,",");

	if( m.size() < 13 )
		return ats::String();

	m[1]=fixTime;
	m[9]=fixDate;

	size_t pos = m[12].find('*');
	if( pos == ats::String::npos) return ats::String();
	m[12] = m[12].substr(0, pos);

	ats::String buf;
	ats::StringList::const_iterator i = m.begin();
	while(i != m.end()-1)
	{
		ats::String s;
		ats_sprintf(&s, "%s,", (*i).c_str());
		buf+=s;
		++i;
	}
	buf+=m[12];

	CHECKSUM cs;
	cs.add_checksum(buf);
	return buf;
}

ats::String modifyGPGGAwithCurrentTime(ats::String& str)
{
	time_t     now;
	struct tm  *ts;
	now = ::time(NULL);
	ts = localtime(&now);
	ats::String fixTime;
	ats_sprintf(&fixTime, "%.2d%.2d%.2d.000", ts->tm_hour, ts->tm_min,(short)(ts->tm_sec));

	if( str.size() <= 17 ) return ats::String();
	ats::String s = str.substr(17);
	if( s[0] != ',' ) return ats::String();
	size_t pos = s.find('*');
	if( pos == ats::String::npos) return ats::String();
	s = s.substr(0, pos);

	ats::String buf;
	ats_sprintf(&buf, "$GPGGA,%s%s", fixTime.c_str(), s.c_str());
	CHECKSUM cs;
	cs.add_checksum(buf);
	return buf;
}

int main(int argc, char* argv[])
{

	if(argc < 2)
	{
		print_usage();
		return 1;
	}

	g_log.set_global_logger(&g_log);
	g_log.set_level(0);

	g_log.open_testdata(g_app_name);
	ats_logf(ATSLOG(0), "gps-sim started");

	ats::String name;
	int masterfd;

	if(( masterfd = ptyMasterOpen((char *)(name.c_str()), 32)) > 0)
	{
		ats_logf(ATSLOG(0), "Device name %s\n", name.c_str());
		printf("Device name %s\n", name.c_str());
	}
	else
	{
		return 1;
	}

	set_slave_attr(name);

	if( argc >= 3 && strcmp(argv[2], "loop") == 0)
	{
		ats_logf(ATSLOG(0), "enter loop mode\n");
		g_loop = true;
	}

	ats::String buf;
	ats_sprintf(&buf, "ln -sf %s /dev/ttySER1", name.c_str());
	ats::system(buf);
	ats::system("killall SER_GPS");

	while(1)
	{
		feed_data(argv[1], masterfd);
		ats_logf(ATSLOG(0), "gps-sim ended");
		if( !g_loop )
			break;
	}

	close(masterfd);
	return 0;
}
