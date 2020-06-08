#include <vector>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <utime.h>
#include <fcntl.h>
#include <dirent.h>

#include "ats-common.h"

using namespace ats;

const ats::String ats::g_zero_length;
const ats::String& ats::g_empty = ats::g_zero_length;
const ats::String& ats::StringMap::m_zero_length = ats::g_zero_length;

static const char* h_weekday(const int p_day)
{
	switch(p_day)
	{
	case 1: return "Monday, ";
	case 2: return "Tuesday, ";
	case 3: return "Wednesday, ";
	case 4: return "Thursday, ";
	case 5: return "Friday, ";
	case 6: return "Saturday, ";
	}

	return "Sunday, ";
}

ats::String ats::human_readable_date(const time_t p_sec, bool p_weekday)
{
	struct tm t;
	gmtime_r(&p_sec, &t);
	char buf[128];
	snprintf(buf, sizeof(buf) - 1, "%s%04d/%02d/%02d - %02d:%02d:%02d UTC",
		p_weekday ? h_weekday(t.tm_wday) : "",
		t.tm_year + 1900,
		t.tm_mon + 1,
		t.tm_mday,
		t.tm_hour,
		t.tm_min,
		t.tm_sec);
	buf[sizeof(buf) - 1] = '\0';
	return ats::String(buf);
}

ats::String ats::human_readable_date(bool p_weekday)
{
	struct timeval t;
	gettimeofday(&t, 0);
	return human_readable_date(t.tv_sec, p_weekday);
}

int ats::daemonize(bool p_exit_parent)
{
	/* already a daemon */
	if(getppid() == 1)
	{
		return -1;
	}

	return ats::detach(p_exit_parent);
}

int ats::detach(bool p_exit_parent)
{
	pid_t pid, sid;

	/* Fork off the parent process */
	pid = fork();

	if(pid < 0)
	{
		return -errno;
	}

	/* If we got a good PID, then we can exit the parent process. */
	if(pid > 0)
	{

		if(p_exit_parent)
		{
			exit(EXIT_SUCCESS);
		}

		return pid;
	}

	/* At this point we are executing as the child process */

	/* Change the file mode mask */
	umask(0);

	/* Create a new SID for the child process */
	sid = setsid();

	if(sid < 0)
	{
		return -errno;
	}

	/* Change the current working directory. This prevents the current
	   directory from being locked; hence not being able to remove it.
	*/
	if((chdir("/")) < 0)
	{
		return -errno;
	}

	/* Redirect standard files to /dev/null */
	ats::ignore_return<FILE*>(freopen( "/dev/null", "r", stdin));
	ats::ignore_return<FILE*>(freopen( "/dev/null", "w", stdout));
	ats::ignore_return<FILE*>(freopen( "/dev/null", "w", stderr));
	return 0;
}

int ats::get_uid_gid(const String& p_user, int& p_uid, int& p_gid)
{
	struct passwd* p = getpwnam(p_user.c_str());

	if(!p)
	{
		return -errno;
	}

	if(initgroups(p->pw_name, p->pw_gid))
	{
		return -errno;
	}

	p_uid = p->pw_uid;
	p_gid = p->pw_gid;
	return 0;
}

int ats::get_uid(const String& p_user, int& p_uid)
{
	struct passwd* p = getpwnam(p_user.c_str());

	if(!p)
	{
		return -errno;
	}

	p_uid = p->pw_uid;
	return 0;
}

int ats::get_gid(const String& p_group, int& p_gid)
{
	struct group* g = getgrnam(p_group.c_str());

	if(!g)
	{
		return -errno;
	}

	p_gid = g->gr_gid;
	return 0;
}

int ats::su(const String& p_user)
{
	int uid;
	int gid;

	const int ret = get_uid_gid(p_user, uid, gid);

	if(ret)
	{
		return ret;
	}

	if(setgid(gid))
	{
		return -errno;
	}

	if(setuid(uid))
	{
		return -errno;
	}

	return 0;
}

int ats::su(const String& p_user, const String& p_group)
{
	int uid;

	{
		const int ret = get_uid(p_user, uid);

		if(ret)
		{
			return ret;
		}

	}

	int gid;

	{
		const int ret = get_gid(p_group, gid);

		if(ret)
		{
			return ret;
		}

	}

	if(setgid(gid))
	{
		return -errno;
	}

	if(setuid(uid))
	{
		return -errno;
	}

	return 0;
}

bool ats::get_bool(const String& p_val)
{
	const char *s = p_val.c_str();

	switch(*s)
	{
	case 'O':
	case 'o': return (!strncasecmp(s + 1, "n", p_val.size() - 1));

	case 'S':
	case 's': return (!strncasecmp(s + 1, "et", p_val.size() - 1));

	case 'T':
	case 't': return (!strncasecmp(s + 1, "rue", p_val.size() - 1));

	case 'Y':
	case 'y':
	case '1':

		if((1 == p_val.size()))
		{
			return true;
		}

		return (!strncasecmp(s + 1, "es", p_val.size() - 1));
	}

	return strtol(s, 0, 0) ? true : false;
}

const String &StringMap::get(const String& p_key) const
{
	const_iterator i = find(p_key);
	if(end() != i) return i->second;
	return m_zero_length;
}

const String &StringMap::get(const String& p_key, const String& p_default) const
{
	const_iterator i = find(p_key);
	if(end() != i) return i->second;
	return p_default;
}

ats::String& StringMap::get_if_exists(ats::String& p_des, const String& p_key) const
{
	bool b;
	return get_if_exists(p_des, p_key, b);
}

ats::String& StringMap::get_if_exists(ats::String& p_des, const String& p_key, bool& p_exists) const
{
	const ats::String& s = get(p_key);

	if((p_exists = ((&s) != (&m_zero_length))))
	{
		p_des = s;
	}

	return p_des;
}

bool StringMap::get_bool(const String &p_key, bool p_default) const
{
	const_iterator i = find(p_key);

	if(end() != i)
	{
		return ats::get_bool(i->second);
	}

	return p_default;
}

int StringMap::get_int(const String &p_key, int p_default) const
{
	const_iterator i = find(p_key);

	if(end() != i)
	{
		return (int)(strtol((i->second).c_str(), 0, 0));
	}

	return p_default;
}

float StringMap::get_float(const String &p_key, float p_default) const
{
	const_iterator i = find(p_key);

	if(end() != i)
	{
		return strtof((i->second).c_str(), 0);
	}

	return p_default;
}

double StringMap::get_double(const String &p_key, double p_default) const
{
	const_iterator i = find(p_key);

	if(end() != i)
	{
		return strtod((i->second).c_str(), 0);
	}

	return p_default;
}

long long StringMap::get_long_long(const String &p_key, long long p_default) const
{
	const_iterator i = find(p_key);

	if(end() != i)
	{
		return strtoll((i->second).c_str(), 0, 0);
	}

	return p_default;
}

bool StringMap::has_key(const String &p_key) const
{
	return !(end() == find(p_key));
}

bool StringMap::set(const String &p_key, const String &p_value, bool p_overwrite)
{
	std::pair<iterator, bool> r = insert(StrStrPair(p_key, p_value));

	if(!r.second)
	{

		if(p_overwrite)
		{
			(r.first)->second = p_value;
			return true;
		}

		return false;
	}

	return true;
}

void StringMap::unset(const String &p_key)
{
	iterator i = find(p_key);

	if(i != end())
	{
		erase(i);
	}

}

void StringMap::get_key_val(const char* p_s, String& p_key, String& p_val)
{
	p_key.clear();
	p_val.clear();

	if(!p_s)
	{
		return;
	}

	while(*p_s)
	{
		const char c = *(p_s++);

		if('=' == c)
		{
			break;
		}

		p_key += c;
	}

	p_val = p_s;
}

void StringMap::from_args(int p_argc, char* p_argv[])
{
	int i = 0;
	String key;
	String val;

	for(; i < p_argc; ++i)
	{
		get_key_val(p_argv[i], key, val);
		set(key, val);
	}

}

void StringMap::from_args(int p_argc, char* p_argv[], StringMap& p_opt)
{
	String key;
	String val;
	int i = 0;
	char opt[2] = {'\0', '\0'};

	for(; i < p_argc; ++i)
	{

		if('-' == p_argv[i][0])
		{

			if('-' == p_argv[i][1])
			{

				if('\0' == p_argv[i][2])
				{
					++i;
					break;
				}

			}
			else
			{
				const char* s = p_argv[i] + 1;

				while(*s)
				{
					opt[0] = *(s++);
					p_opt.set(opt, String());
				}

				continue;
			}

		}

		get_key_val(p_argv[i], key, val);
		set(key, val);
	}

	from_args(p_argc - i, p_argv + i);
}

void StringMap::from_file(const ats::String& p_fname)
{
	FILE* f = fopen(p_fname.c_str(), "r");

	if(!f)
	{
		return;
	}

	ReadDataCache_FILE rdc(f);

	for(;;)
	{
		ats::String s;
		const int ret = get_file_line(s, rdc, 1);

		if(ret)
		{
			break;
		}

		ats::String k;
		ats::String v;
		get_key_val(s.c_str(), k, v);

		if(!s.empty())
		{
			set(k,v);
		}

	}

	fclose(f);
}

StringMap& StringMapMap::get(const String& p_key)
{
	iterator i = find(p_key);

	if(end() == i)
	{
		i = (insert(StrMapPair(p_key, StringMap()))).first;
	}

	return i->second;
}

bool StringMapMap::has_key(const String& p_key) const
{
	return (find(p_key) != end());
}

void StringMapMap::unset(const String& p_key)
{
	iterator i = find(p_key);

	if(end() != i)
	{
		erase(i);
	}

}

int ats::system(const String &p_cmd, std::ostream *p_stdout)
{
	FILE *f = popen(p_cmd.c_str(), "r");
	if(!f) return -1;
	for(;;)
	{
		char buf[2048];
		const size_t nread = fread(buf, 1, sizeof(buf), f);
		if(!nread) break;
		if(p_stdout) p_stdout->write(buf, nread);
	}
	return pclose(f);
}

int ats::read_file(const String& p_fname, String& p_des, size_t p_len)
{
	p_des.clear();
	FILE* f = fopen(p_fname.c_str(), "r");

	if(!f)
	{
		return -1;
	}

	if(p_len)
	{

		for(;p_len;)
		{
			char buf[2048];
			const size_t nread = fread(buf, 1, (p_len > sizeof(buf)) ? sizeof(buf) : p_len, f);

			if(!nread)
			{
				break;
			}

			p_des.append(buf, nread);
			p_len -= nread;
		}

	}
	else
	{

		for(;;)
		{
			char buf[2048];
			const size_t nread = fread(buf, 1, sizeof(buf), f);

			if(!nread)
			{
				break;
			}

			p_des.append(buf, nread);
		}

	}

	fclose(f);
	return 0;
}

int ats::ReadDataCache::getc()
{

	if(!m_remain)
	{
		const ssize_t nread = do_read(m_buf, sizeof(m_buf));

		if(nread < 0)
		{
			return nread;
		}

		if(!nread)
		{
			return -ENODATA;
		}

		m_remain = size_t(nread);
		m_c = m_buf;
	}

	const unsigned char c = *(m_c++);
	--m_remain;

	return c;
}

ssize_t ats::ReadDataCache_fd::do_read(char* p_buf, size_t p_len)
{
	const ssize_t nread = read(m_fd, p_buf, p_len);

	return (nread < 0) ? ssize_t(-errno) : nread;
}

ssize_t ats::ReadDataCache_FILE::do_read(char* p_buf, size_t p_len)
{

	if(!m_f)
	{
		return -EINVAL;
	}

	const size_t nread = fread(p_buf, 1, p_len, m_f);

	if(!nread)
	{
		return ssize_t((feof(m_f)) ? nread : -EIO);
	}

	return ssize_t(nread);
}

int ats::get_file_line(String& p_des, const String &p_fname, size_t p_line, size_t p_len)
{
	FILE* f = fopen(p_fname.c_str(), "r");

	if(!f)
	{
		p_des.clear();
		return -errno;
	}

	ats::ReadDataCache_FILE rdc(f);
	const int ret = get_file_line(p_des, rdc, p_line, p_len);
	fclose(f);
	return ret;
}

int ats::get_file_line(String& p_des, ReadDataCache& p_rdc, size_t p_line, size_t p_len)
{
	p_des.clear();

	if(!p_line)
	{
		return 0;
	}

	size_t line = 1;

	for(;;)
	{
		const int c = p_rdc.getc();

		if(c < 0)
		{
			return c;
		}

		if('\n' == c)
		{

			if(line == p_line)
			{
				break;
			}

			++line;
			continue;
		}

		if(p_line == line)
		{
			p_des += c;

			if(p_len)
			{

				if(!(--p_len))
				{
					break;
				}

			}

		}

	}

	return 0;
}

int ats::write_file(const String& p_fname, const String& p_data, const char* p_mode)
{
	FILE* f = fopen(p_fname.c_str(), p_mode ? p_mode : "w");

	if(!f)
	{
		return -1;
	}

	const size_t nwrite = fwrite(p_data.c_str(), 1, int(p_data.length()), f);
	fclose(f);

	return (nwrite == p_data.length()) ? 0 : -1;
}

String ats::ltrim(const String& p_s, const ats::String& p_char, size_t p_count)
{
	size_t i = 0;

	for(; i < p_s.size(); ++i)
	{
		const char c = p_s[i];

		size_t j;

		for(j = 0; j < p_char.size(); ++j)
		{

			if(c == p_char[j])
			{
				break;
			}

		}

		if(p_char.size() == j)
		{
			break;
		}

		if(p_count)
		{
			--p_count;

			if(!p_count)
			{
				break;
			}

		}

	}

	return (!i) ? p_s : p_s.substr(i);
}

String ats::rtrim(const String& p_s, const ats::String& p_char, size_t p_count)
{
	size_t i = p_s.size();

	for(;;)
	{

		if(!i)
		{
			break;
		}

		const char c = p_s[i - 1];

		size_t j;

		for(j = 0; j < p_char.size(); ++j)
		{

			if(c == p_char[j])
			{
				break;
			}

		}

		if(p_char.size() == j)
		{
			break;
		}

		--i;

		if(p_count)
		{
			--p_count;

			if(!p_count)
			{
				break;
			}

		}

	}

	return (p_s.size() == i) ? p_s : p_s.substr(0, i);
}

String& ats::rtrim_newline(String& p_s, char p_char)
{
	return (p_s = rtrim(p_s, String() + p_char, 1));
}

CommonData::CommonData()
{
	m_config_mutex = new pthread_mutex_t[2];
	m_data_mutex = m_config_mutex + 1;

	pthread_mutex_init(m_config_mutex, 0);
	pthread_mutex_init(m_data_mutex, 0);
}

CommonData::~CommonData()
{
	delete [] m_config_mutex;
}

void CommonData::lock_data() const
{
	pthread_mutex_lock(m_data_mutex);
}

void CommonData::unlock_data() const
{
	pthread_mutex_unlock(m_data_mutex);
}

void CommonData::lock_config() const
{
	pthread_mutex_lock(m_config_mutex);
}

void CommonData::unlock_config() const
{
	pthread_mutex_unlock(m_config_mutex);
}

ats::String CommonData::get(const ats::String &p_key) const
{
	lock_config();
	const ats::String s(m_config.get(p_key));
	unlock_config();
	return s;
}

ats::String CommonData::get(const ats::String &p_key, const ats::String& p_default) const
{
	lock_config();
	const ats::String s(m_config.get(p_key, p_default));
	unlock_config();
	return s;
}

bool CommonData::get_bool(const ats::String &p_key, bool p_default) const
{
	lock_config();
	const bool b = m_config.get_bool(p_key, p_default);
	unlock_config();
	return b;
}

int CommonData::get_int(const ats::String &p_key, int p_default) const
{
	lock_config();
	const int n = m_config.get_int(p_key, p_default);
	unlock_config();
	return n;
}

bool CommonData::set(const ats::String &p_key, const ats::String &p_val, bool p_overwrite)
{
	lock_config();
	const bool b = m_config.set(p_key, p_val, p_overwrite);
	unlock_config();
	return b;
}

void CommonData::unset(const ats::String &p_key)
{
	lock_config();
	m_config.unset(p_key);
	unlock_config();
}

void CommonData::set_from_args(int p_argc, char *p_argv[])
{
	lock_config();
	m_config.from_args(p_argc, p_argv);
	unlock_config();
}

void CommonData::set_from_file(const ats::String& p_fname)
{
	lock_config();
	m_config.from_file(p_fname);
	unlock_config();
}

void CommonData::copy_config(ats::StringMap& p_des) const
{
	lock_config();
	p_des = m_config;
	unlock_config();
}

void ats::infinite_sleep()
{
	// Implement sleep using a semaphore deadlock.
	sem_t* s = new sem_t;
	sem_init(s, 0, 0);
	sem_wait(s);

	// If the above semaphore failed, then do a polling sleep as a last resort.
	for(;;)
	{
		sleep(1);
	}
}

int ats_sprintf(String* p_des, const char* p_format, ...)
{

	if(!p_des)
	{
		return -EINVAL;
	}

	/* Guess we need no more than 248 bytes. */
	int n, size = 248;
	char *p, *np;
	va_list ap;

	if ((p = (char *)malloc(size)) == NULL)
	{
		return -ENOMEM;
	}

	for(;;)
	{
		/* Try to print in the allocated space. */
		va_start(ap, p_format);
		n = vsnprintf(p, size, p_format, ap);
		va_end(ap);

		/* If that worked, return the string. */

		if (n > -1 && n < size)
		{
			p_des->assign(p, n);
			free(p);
			return n;
		}

		/* Else try again with more space. */

		if (n > -1)
		{
			/* glibc 2.1 */
			size = n+1; /* precisely what is needed */
		}
		else
		{
			/* glibc 2.0 */
			size *= 2; /* twice the old size */
		}

		if ((np = (char *)realloc (p, size)) == NULL)
		{
			free(p);
			return -ENOMEM;
		}
		else
		{
			p = np;
		}

	}

}

int ats_ssprintf(std::stringstream* p_des, const char* p_format, ...)
{

	if(!p_des)
	{
		return -EINVAL;
	}

	/* Guess we need no more than 248 bytes. */
	int n, size = 248;
	va_list ap;

	std::vector<char> p(size);

	for(;;)
	{
		/* Try to print in the allocated space. */
		va_start(ap, p_format);
		n = vsnprintf(&(p[0]), size, p_format, ap);
		va_end(ap);

		/* If that worked, return the string. */

		if (n > -1 && n < size)
		{
			(*p_des) << ats::String(&(p[0]), n);
			return n;
		}

		/* Else try again with more space. */

		if (n > -1)
		{
			/* glibc 2.1 */
			size = n+1; /* precisely what is needed */
		}
		else
		{
			/* glibc 2.0 */
			size *= 2; /* twice the old size */
		}

		p.resize(size);
	}

}

char* ats::to_hex(char* p_des, size_t p_des_len, const String& p_src, char** p_end)
{
	const size_t max = p_des_len >> 1;
	size_t count = (p_src.length() < max) ? p_src.length() : max;
	char* des = p_des;
	const char* src = p_src.c_str();

	while(count--)
	{
		const unsigned char c = *(src++);
		des[0] = c >> 4;
		des[1] = c & 0xf;
		des[0] = (des[0] < 10) ? ('0' + des[0]) : ('7' + des[0]);
		des[1] = (des[1] < 10) ? ('0' + des[1]) : ('7' + des[1]);
		des += 2;
	}

	if(p_end)
	{
		*p_end = des;
	}

	return p_des;
}

String to_hex(std::vector <char> p_src)
{
	const size_t buflen = p_src.size() << 1;
  String p_des;
	p_des.resize(buflen);

	to_hex(&(p_des[0]), buflen, p_src.data());
	return p_des;
}

String& ats::to_hex(ats::String& p_des, const String& p_src)
{
	const size_t buflen = p_src.size() << 1;

	if(p_des.size() != buflen)
	{
		// AWARE360 FIXME: Is this faster/more-efficient than just resizing and letting std::string do a string copy to new memory?
		p_des.clear();
		p_des.resize(buflen);
	}

	to_hex(&(p_des[0]), buflen, p_src);
	return p_des;
}

String ats::to_hex(const String& p_s)
{
	String str;
	const size_t buflen = p_s.size() << 1;
	str.resize(buflen);
	to_hex(&(str[0]), buflen, p_s);
	return str;
}

// this is for a char array that may contain 0x00 values
String ats::to_hex(const char *data, const int len)
{
	char str[16];
	String s;

	for(int i = 0; i < len; ++i)
	{
		sprintf(str, "%02x ", data[i]);
		s += str;
	}

	return s;
}

const char ats::g_hextable[128] =
{
	0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0,  1,  2,  3,  4,  5,  6, 7, 8, 9, 0, 0, 0, 0, 0, 0,
	0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

String ats::from_hex(const String& p_s)
{
	size_t count = p_s.size() >> 1;
	String str;
	str.resize(count);
	char* s = &(str[0]);
	const char* p = p_s.c_str();

	while(count--)
	{
		*(s++) = (g_hextable[p[0] & 0x7f] << 4) | (g_hextable[p[1] & 0x7f]);
		p += 2;
	}

	return str;
}

char* ats::getstrtimef(const struct timeval& p_tv, const String& p_format, char* p_buf, size_t p_max)
{

	if(!(p_buf && p_max))
	{
		return NULL;
	}

	time_t nowtime;
	struct tm* nowtm;
	nowtime = p_tv.tv_sec;
	nowtm = localtime(&nowtime);
	strftime(p_buf, p_max, p_format.c_str(), nowtm);
	p_buf[p_max - 1] = '\0';
	return p_buf;
}

char* ats::getstrtimef(const String& p_format, char* p_buf, size_t p_max)
{

	if(!(p_buf && p_max))
	{
		return NULL;
	}

	struct timeval tv;
	gettimeofday(&tv, NULL);
	return getstrtimef(tv, p_format, p_buf, p_max);
}

String ats::getstrtime()
{
	char buf[64];
	return getstrtimef("%Y-%m-%d %H:%M:%S", buf, sizeof(buf));
}

String& ats::getstrtime_ms(String& p_buf, struct timeval& p_tv)
{
	char buf[64];
	ats_sprintf(
		&p_buf,
		"%s.%06lu",
		getstrtimef(p_tv, "%Y-%m-%d %H:%M:%S", buf, sizeof(buf)),
		p_tv.tv_usec);
	return p_buf;
}

String& ats::getstrtime_ms(String& p_buf)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return getstrtime_ms(p_buf, tv);
}

String ats::getstrtime_ms()
{
	String buf;
	return getstrtime_ms(buf);
}

void ats::get_command_key(const ats::String& p_arg, ats::String& p_cmd, ats::String& p_key)
{
	const size_t i = p_arg.find(':');

	if(ats::String::npos == i)
	{
		p_cmd = p_arg;
		p_key.clear();
	}
	else
	{
		p_cmd = p_arg.substr(0, i);
		p_key = p_arg.substr(i + 1);
	}

}

bool ats::file_exists(const char* p_fname)
{
	struct stat s;
	return (0 == stat(p_fname, &s));
}

bool ats::file_exists(const String& p_fname)
{
	return file_exists(p_fname.c_str());
}

bool ats::touch(const String& p_fname)
{
	// XXX: Keep file opened during access time update to make sure the file
	//	exists while the time is being updated.
	FILE* f = fopen(p_fname.c_str(), "w");

	const int ret = utime(p_fname.c_str(), 0);

	if(f)
	{
		fclose(f);
	}

	return (!ret);
}

int ats::get_hw_config(ats::StringMap& p_hw_cfg)
{
	p_hw_cfg.clear();
	FILE* f = fopen("/dev/redstone-hw-config", "r");

	if(!f)
	{
		return -errno;
	}

	ReadDataCache_FILE rdc(f);
	ats::String line;
	const size_t max_line_length = 256;
	line.reserve(max_line_length);

	for(;;)
	{
		const int c = rdc.getc();

		if(c < 0)
		{

			if(c != -ENODATA)
			{
				fclose(f);
				return c;
			}

			break;
		}

		if('\n' == c)
		{
			ats::String key, val;
			p_hw_cfg.get_key_val(line.c_str(), key, val);

			if(!key.empty())
			{
				line.clear();
				line.reserve(max_line_length);
				p_hw_cfg.set(key, val);
			}

			continue;
		}

		line += c;
	}

	fclose(f);
	return 0;
}

const ats::String g_trulink_model_store_file("/mnt/nvram/rom/trulink-model.txt");

const ats::String ats::g_trulink_hid_2500("2500");
const ats::String ats::g_trulink_hid_3000("3000");
const ats::String ats::g_trulink_hid_5000("5000");
const ats::String ats::g_trulink_hid_error("error");
const ats::String ats::g_trulink_hid_unknown("unknown");

int ats::store_trulink_model(TRULINK_HID p_hid, TRULINK_MODEL_REV p_rev, int p_model_code)
{
	FILE* f = fopen(g_trulink_model_store_file.c_str(), "w");

	if(!f)
	{
		return errno;
	}

	fprintf(f, "%d,%d,%d\n", p_hid, p_rev, p_model_code);
	fclose(f);
	return 0;
}

const ats::String& ats::get_stored_trulink_model(TRULINK_MODEL_REV* p_rev, int* p_model_code)
{

	if(!ats::file_exists(g_trulink_model_store_file))
	{
		return get_trulink_model(p_rev, p_model_code);
	}

	FILE* f = fopen(g_trulink_model_store_file.c_str(), "r");

	if(f)
	{
		int hid;
		int rev;
		int model_code;
		ats::ignore_return<int>(fscanf(f, "%d\n%d\n%d\n", &hid, &rev, &model_code));
		fclose(f);

		if(p_rev)
		{
			*p_rev = TRULINK_MODEL_REV(rev);
		}

		if(p_model_code)
		{
			*p_model_code = model_code;	
		}

		switch(hid)
		{
		case TRULINK_HID_UNKNOWN: break;
		case TRULINK_HID_2500: return g_trulink_hid_2500;
		case TRULINK_HID_3000: return g_trulink_hid_3000;
		}

	}

	return get_trulink_model(p_rev, p_model_code);
}

const ats::String& ats::get_trulink_model(TRULINK_MODEL_REV* p_rev, int* p_model_code)
{
	const float VREF = 3.7f;
	const float magic = 4096.0f;

	// AWARE360 FIXME: "/dev/hid" is reserved for Human Interface Devices (HID class of USB devices).
	//	Rename to "trulink_hid" or something else.
	const int fd = open("/dev/hid", O_RDONLY);

	if(fd < 0)
	{
		return g_trulink_hid_error;
	}

	// AWARE360 FIXME: Add feature to "ReadDataCache" so that a smaller cache buffer can be used. The
	//	current default size of 1024 bytes is far too big for this particular use case.
	ReadDataCache_fd rdc(fd);
	bool adc_ok = false;
	int adc = 0;

	for(;;)
	{
		const int c = rdc.getc();

		if(c < 0)
		{
			break;
		}

		if((c >= '0') && (c <= '9'))
		{
			adc_ok = true;
			adc = (adc * 10) + (c - '0');
		}
		else if('\n' == c)
		{
			break;
		}

	}

	close(fd);

	if(p_model_code)
	{
		*p_model_code = adc;
	}

	if(!adc_ok)
	{
		return g_trulink_hid_error;
	}

	TRULINK_MODEL_REV rev;

	if(!p_rev)
	{
		p_rev = &rev;
	}

	const float voltage = (float(adc) * VREF) / magic;
	// AWARE360 FIXME: Difference between TRULink 2500 2.0 and 2.00 hardware is "< 0.3f" or
	//	"> 0.3f" with no margin for error. It is expected that there should be "guard bands"
	//	of invalid values when dealing with analog conversions.
	//
	//	Example:
	//
	//	[1 to 3] = Model_A
	//	[3 to 4] = Invalid
	//	[4 to 5] = Model_B
	//	[5 to 6] = Invalid
	//	   .
	//	   .
	//	   .
	//	[ etc ]
	if((voltage >= 0.2f) && (voltage < 0.3f))
	{
		*p_rev = TRULINK_MODEL_REV_2_0;
		return g_trulink_hid_2500;
	}
	else if((voltage >= 0.3f) && (voltage <= 0.5f))
	{
		*p_rev = TRULINK_MODEL_REV_2_00;
		return g_trulink_hid_2500;
	}
	else if((voltage >= 0.9f) && (voltage <= 1.1f))
	{
		*p_rev = TRULINK_MODEL_REV_2_00;
		return g_trulink_hid_3000;
	}
	else if((voltage >= 1.6f) && (voltage <= 1.8f))
	{
		*p_rev = TRULINK_MODEL_REV_2_0;
		return g_trulink_hid_3000;
	}
	else if((voltage >= 1.2f) && (voltage <= 1.4f))
	{
		*p_rev = TRULINK_MODEL_REV_3_0;
		return g_trulink_hid_5000;
	}

	return g_trulink_hid_unknown;
}

bool ats::is_trulink_model(const ats::String& p_type)
{
	return (p_type == get_stored_trulink_model());
}

bool ats::is_trulink_model(const ats::String& p_type, const ats::StringMap& p_hw_cfg)
{

	if(p_hw_cfg.empty())
	{
		ats::StringMap cfg;
		const int err = ats::get_hw_config(cfg);

		if(err)
		{
			return false;
		}

		return (p_type == cfg.get("g_trulink_model"));
	}

	return (p_type == p_hw_cfg.get("g_trulink_model"));
}

bool ats::testmode()
{
	return ats::file_exists(ATS_TEST_MODE_FILE);
}

bool ats::called_as(const char* p_argv_entry, const char* p_program_name)
{
	const char* s = strstr(p_argv_entry, p_program_name);
	// If name found, and is last part of string, and (name occurs at beginning of string, or follows '/')
	return (s && (0 == strcmp(p_program_name, s)) && ((s == p_argv_entry) || ('/' == s[-1])));
}

int ats::gettid()
{
	return syscall(SYS_gettid);
}

ats::String& ats::getenv(ats::String& p_des, const char* p_key)
{
	const char* env = ::getenv(p_key);

	if(env)
	{
		p_des = env;
	}
	else
	{
		p_des.clear();
	}

	return p_des;
}

int ats::mkdir(const ats::String& p_path, bool p_auto_make_parents, int p_mode)
{

	if(!p_auto_make_parents)
	{
		return ::mkdir(p_path.c_str(), p_mode) ? 1 : 0;
	}

	const int pid = fork();

	if(!pid)
	{
		const char* app = "/bin/mkdir";
		execl(app, app, "-p", p_path.c_str(), NULL);
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

int ats::trulink_firmware_version(
	ats::String* p_revision,
	ats::String* p_full_version,
	ats::String* p_build_date)
{
	FILE* f = fopen("/version", "r");

	if(!f)
	{
		return -errno;
	}

	ats::ReadDataCache_FILE rdc(f);
	ats::String s;
	ats::String* param[] =
		{
			p_revision,
			p_full_version,
			p_build_date
		};
	int ret = 0;
	size_t i;

	for(i = 0; i < 3; ++i)
	{
		ret = get_file_line(param[i] ? (*(param[i])) : s, rdc, 1);

		if(ret < 0)
		{
			break;
		}

	}

	fclose(f);
	return ret;
}

bool ats::dir_exists(const ats::String& p_path)
{
	DIR* dir = opendir(p_path.c_str());

	if(dir)
	{
		closedir(dir);
		return true;
	}

	return false;
}

bool ats::diff_files(const ats::String& p_a, const ats::String& p_b)
{
	const int pid = fork();

	if(!pid)
	{
		const char* app = "/usr/bin/diff";
		execl(app, app, "-a", "-q", p_a.c_str(), p_b.c_str(), NULL);
		exit(1);
	}

	if(pid > 0)
	{
		int status;
		waitpid(pid, &status, 0);

		if(WIFEXITED(status))
		{
			return WEXITSTATUS(status);
		}

	}

	return 1;
}

int ats::cp(const ats::String& p_src, const ats::String& p_des)
{
	const int pid = fork();

	if(!pid)
	{
		const char* app = "/bin/cp";
		execl(app, app, "-a", p_src.c_str(), p_des.c_str(), NULL);
		exit(1);
	}

	if(pid > 0)
	{
		int status;
		waitpid(pid, &status, 0);

		if(WIFEXITED(status))
		{
			return WEXITSTATUS(status);
		}

	}

	return 99;
}
