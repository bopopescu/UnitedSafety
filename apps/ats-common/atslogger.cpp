#include <vector>

#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>

#include "atslogger.h"

using namespace ats;

ATSLogger* ATSLogger::m_global_logger = 0;

ATSLogger::ATSLogger()
{
	init_logger();
	m_mode = 0666;
}

ATSLogger::ATSLogger(const ats::String& p_user, int p_mode)
{
	init_logger();

	if(!(p_user.empty()))
	{
		ats::get_uid_gid(p_user, m_uid, m_gid);
	}

	m_mode = p_mode;
}

void ATSLogger::init_logger()
{
	m_compress_time = false;
	m_first_compressed_time = true;
	m_syslog_priority = LOG_NOTICE;
	m_uid = -1;
	m_gid = -1;
	m_mode = -1;
	m_level = 0;
	m_cur_file_number = 0;
	m_max_file_number = 1;
	m_max_size = m_default_log_file_size;

	m_out = 0;
	m_file = 0;
	m_mutex = new pthread_mutex_t;
	pthread_mutex_init(m_mutex, 0);
}

ATSLogger::~ATSLogger()
{
	stop_all_capture();

	if(m_file)
	{
		fclose(m_file);
	}

	if(m_mutex)
	{
		pthread_mutex_destroy(m_mutex);
		delete m_mutex;
	}

}

void ATSLogger::set_global_logger(ATSLogger* p_log)
{
	m_global_logger = p_log;
}

ATSLogger* ATSLogger::get_global_logger()
{
	return m_global_logger;
}

ATSLogger* ATSLogger::global_logger(int p_level)
{
	ATSLogger* log = get_global_logger();
	return (log && (log->get_level() >= p_level)) ? log : 0;
}

ATSLogger* ATSLogger::operator()(int p_level)
{
	return (get_level() >= p_level) ? this : 0;
}

int ATSLogger::set_level(int p_level)
{
	lock();
	m_level = p_level;
	unlock();
	return p_level;
}

int ATSLogger::get_level() const
{
	lock();
	const int n = m_level;
	unlock();
	return n;
}

void ATSLogger::lock() const
{
	pthread_mutex_lock(m_mutex);
}

void ATSLogger::unlock() const
{
	pthread_mutex_unlock(m_mutex);
}

int ATSLogger::open_testdata(const ats::String& p_app_name, bool p_append)
{
	const ats::String dir("/var/log/testdata/");
	const ats::String& fname = p_app_name + ".log";
	struct stat st;

	if(0 == stat(dir.c_str(), &st))
	{
		set_max_size(0);
		mkdir((dir + p_app_name).c_str(), 0664);
		return open(dir + p_app_name + "/" + fname, p_append);
	}

	return open("/var/log/" + fname, p_append);
}

int ATSLogger::open(const ats::String& p_fname, bool p_append)
{
	bool use_default = true;

	if(!p_fname.empty())
	{
		const char* s = p_fname.c_str() + p_fname.size() - 1;

		while(s != p_fname.c_str())
		{
			const char c = *s;

			if('/' == c)
			{
				use_default = false;
				m_fname = s + 1;
				m_dir = p_fname.substr(0, s - p_fname.c_str());
				break;
			}

			--s;
		}
	}

	if(use_default)
	{
		m_fname = p_fname;
		m_dir = "/var/log";
	}

	lock();
	int err = h_open(p_fname, p_append);

	if(!err)
	{
		err = h_rotate_log_file_if_too_big();
	}

	unlock();
	return err;
}

int ATSLogger::h_open(const ats::String& p_fname, bool p_append)
{

	if(m_file)
	{
		fclose(m_file);
	}

	m_flush_fname = p_fname;
	m_file = fopen(p_fname.c_str(), p_append ? "a" : "w");
	const bool failed = !m_file;

	if(failed)
	{
		return -errno;
	}

	if((m_uid) >= 0 && (m_gid >=0))
	{
		ats::ignore_return<int>(chown(p_fname.c_str(), m_uid, m_gid));
	}

	if(m_mode >= 0)
	{
		fchmod(fileno(m_file), m_mode);
	}

	return 0;
}

size_t ATSLogger::set_cur_file_number(size_t p)
{
	lock();
	m_cur_file_number = p;
	unlock();
	return p;
}

size_t ATSLogger::set_max_file_number(size_t p)
{
	lock();
	m_max_file_number = p;
	unlock();
	return p;
}

size_t ATSLogger::set_max_size(size_t p)
{
	lock();
	m_max_size = p;
	unlock();
	return p;
}

size_t ATSLogger::get_cur_file_number() const
{
	lock();
	const size_t n = m_cur_file_number;
	unlock();
	return n;
}

size_t ATSLogger::get_max_file_number() const
{
	lock();
	const size_t n = m_max_file_number;
	unlock();
	return n;
}

size_t ATSLogger::get_max_size() const
{
	lock();
	const size_t n = m_max_size;
	unlock();
	return n;
}

int ATSLogger::h_rotate_file()
{
	bool append = true;

	if((++m_cur_file_number) > m_max_file_number)
	{
		m_cur_file_number = 0;
		append = false;
	}

	ats::String s(m_dir + '/' + m_fname);

	if(m_cur_file_number)
	{	
		s += '.';
		s += ats::toStr(m_cur_file_number);
	}

	if(append && (m_file && (!m_flush_fname.empty())))
	{
		struct stat st;

		if(0 == stat(m_flush_fname.c_str(), &st))
		{
			const time_t cur_time = st.st_mtime;

			if(0 == stat(s.c_str(), &st))
			{
				append = (st.st_mtime >= cur_time);
			}

		}

	}

	return h_open(s, append);
}

int ATSLogger::h_rotate_log_file_if_too_big()
{

	if(!(m_max_size && m_file))
	{
		return 0;
	}

	size_t i;
	const size_t attempts = 2;

	for(i = 0; i < (m_max_file_number * attempts); ++i)
	{

		if(!m_file)
		{
			h_rotate_file();
			m_first_compressed_time = true;
			continue;
		}

		const int offset = ftell(m_file);
		bool rotate = false;

		if(offset < 0)
		{

			if(EBADF == errno)
			{
				return -errno;
			}

			rotate = true;
		}
		else if(size_t(offset) > m_max_size)
		{
			rotate = true;
		}

		if(rotate)
		{
			h_rotate_file();
			m_first_compressed_time = true;
		}
		else
		{
			return 0;
		}

	}

	return -EAGAIN;
}

void ATSLogger::set_output(FILE* p_out)
{
	lock();
	m_out = p_out;
	unlock();
}

void ATSLogger::set_syslog_priority(int p_pri)
{
	m_syslog_priority = p_pri;
}

int ATSLogger::write(const char* p_msg, size_t p_len)
{

	if(!p_len)
	{
		return 0;
	}

	if(!p_msg)
	{
		return -EINVAL;
	}

	if(!m_file)
	{

		if(m_out)
		{
			lock();
			const int ret = fprintf(m_out, "%.*s\n", int(p_len), p_msg);
			unlock();
			return (ret < 0) ? -EIO : ret;
		}

		syslog(LOG_NOTICE, "%.*s", int(p_len), p_msg);
		return 0;
	}

	
	struct timeval tv;
        gettimeofday(&tv, NULL);
	ats::String time;

	if(m_compress_time)
	{
		lock();
		const bool first_time = m_first_compressed_time;
		m_first_compressed_time = false;
		size_t diff_sec = 0;
		size_t diff_usec = 0;

		if(!first_time)
		{
			diff_sec = tv.tv_sec - m_prev_sec;
			const long long ti = ((long long)(tv.tv_sec) * (long long)1000000) + (long long)(tv.tv_usec);
			const long long tf = ((long long)(m_prev_sec) * (long long)1000000) + (long long)(m_prev_usec);
			const long long diff = ti - tf;
			diff_usec = size_t(diff - ((diff / ((long long)1000000)) * ((long long)1000000)));
		}

		unlock();

		if(first_time)
		{
			ats_sprintf(&time, "%zu.%zu", size_t(tv.tv_sec), size_t(tv.tv_usec));
		}
		else
		{

			if(diff_sec)
			{
				ats_sprintf(&time, "%zu.%zu", size_t(diff_sec), size_t(diff_usec));
			}
			else
			{
				ats_sprintf(&time, "%zu", size_t(diff_usec));
			}

		}

	}
	else
	{
		getstrtime_ms(time, tv);
	}

	lock();
	m_prev_sec = tv.tv_sec;
	m_prev_usec = tv.tv_usec;
	size_t nwrite = 0;
	size_t n;

	// Write time
	{
		n = fwrite(time.c_str(), 1, time.size(), m_file);

		if(time.size() != n)
		{
			unlock();
			return -EIO;
		}

		nwrite += n;

		n = fwrite(m_compress_time ? ":" : ": ", 1, 2, m_file);

		if(2 != n)
		{
			unlock();
			return -EIO;
		}

		nwrite += n;
	}

	// Write message
	n = fwrite(p_msg, 1, p_len, m_file);

	if(p_len != n)
	{
		unlock();
		return -EIO;
	}

	nwrite += n;

	// Terminate line
	{
		n = fwrite("\n", 1, 1, m_file);

		if(1 != n)
		{
			unlock();
			return -EIO;
		}

		nwrite += n;
	}

	// Break the Kernel caching when log file is on a buffered filesystem.
	{
		const int ret = h_open(m_flush_fname, true);

		if(ret)
		{
			unlock();
			return ret;
		}

	}

	h_rotate_log_file_if_too_big();

	unlock();
	return nwrite;
}

int ats_logf(ATSLogger* p_log, const char* p_format, ...)
{

	if(!p_log)
	{
		return 0;
	}

	int size = 224;
	std::vector <char> buf(size);
	va_list ap;

	for(;;)
	{
		// Try to print in the allocated space.
		va_start(ap, p_format);
		const int n = vsnprintf(&(buf[0]), int(buf.size()), p_format, ap);
		va_end(ap);

		// If that worked, return the string.
		if (n > -1 && n < size)
		{
			const int ret = p_log->write(&(buf[0]), n);
			return ret;
		}

		// Else try again with more space.
		if(n > -1)
		{
			// glibc 2.1
			size = n+1; // precisely what is needed
		}
		else
		{
			// glibc 2.0
			size *= 2; // twice the old size
		}

		buf.resize(size);
	}

}

// just log the input - no date or time added
//
int ats_logf_raw(ATSLogger* p_log, const char* p_format, ...)
{

	if(!p_log)
	{
		return 0;
	}

	int size = 224;
	std::vector <char> buf(size);
	va_list ap;

	for(;;)
	{
		// Try to print in the allocated space.
		va_start(ap, p_format);
		const int n = vsnprintf(&(buf[0]), int(buf.size()), p_format, ap);
		va_end(ap);

		// If that worked, return the string.
		if (n > -1 && n < size)
		{
			const int ret = p_log->write_raw(&(buf[0]), n);
			return ret;
		}

		// Else try again with more space.
		if(n > -1)
		{
			// glibc 2.1
			size = n+1; // precisely what is needed
		}
		else
		{
			// glibc 2.0
			size *= 2; // twice the old size
		}

		buf.resize(size);
	}

}

int ATSLogger::write_raw(const char* p_msg, size_t p_len)
{

	if(!p_len)
	{
		return 0;
	}

	if(!p_msg)
	{
		return -EINVAL;
	}

	if(!m_file)
	{

		if(m_out)
		{
			lock();
			const int ret = fprintf(m_out, "%.*s\n", int(p_len), p_msg);
			unlock();
			return (ret < 0) ? -EIO : ret;
		}

		syslog(LOG_NOTICE, "%.*s", int(p_len), p_msg);
		return 0;
	}

	lock();

	size_t nwrite = 0;
	size_t n;


	// Write message
	n = fwrite(p_msg, 1, p_len, m_file);

	if(p_len != n)
	{
		unlock();
		return -EIO;
	}

	nwrite += n;

	h_rotate_log_file_if_too_big();

	unlock();
	return nwrite;
}

void ATSLogger::flush()
{
	lock();

	if(m_file)
	{
		fflush(m_file);
	}

	unlock();
}

bool ATSLogger::capture_fd(int p_fd)
{
	CaptureContext* cc = new CaptureContext(*this);
	lock();
	std::pair <CaptureContextMap::iterator, bool> r = m_cc.insert(CaptureContextPair(p_fd, cc));

	if(!(r.second))
	{
		delete cc;
		unlock();
		return true;
	}

	if((pipe(cc->m_pipe_capture) >= 0) && (pipe(cc->m_pipe_com) >= 0) && (cc->start()))
	{
		dup2(cc->m_pipe_capture[1], p_fd);
		close(cc->m_pipe_capture[1]);
		cc->m_pipe_capture[1] = -1;
		unlock();
		return true;
	}

	m_cc.erase(r.first);
	unlock();
	return false;
}

bool ATSLogger::CaptureContext::start()
{
	const int retval = pthread_create(
		&m_thread,
		(pthread_attr_t *)0,
		ATSLogger::h_capture_fd,
		this);

	return (m_started = (0 == retval));
}

ATSLogger::CaptureContext::~CaptureContext()
{

	if(m_started)
	{
		char c = '\r';
		ats::ignore_return<ssize_t>(::write(m_pipe_com[1], &c, 1));
		pthread_join(m_thread, 0);
	}

	if(m_pipe_capture[0] >= 0)
	{
		close(m_pipe_capture[0]);
	}

	if(m_pipe_capture[1] >= 0)
	{
		close(m_pipe_capture[1]);
	}

	if(m_pipe_com[0] >= 0)
	{
		close(m_pipe_com[0]);
	}

	if(m_pipe_com[1] >= 0)
	{
		close(m_pipe_com[1]);
	}

}

void ATSLogger::stop_capture_fd(int p_fd)
{
	lock();
	CaptureContextMap::iterator i = m_cc.find(p_fd);

	if(m_cc.end() == i)
	{
		unlock();
		return;
	}

	CaptureContext* cc = i->second;
	m_cc.erase(i);
	unlock();
	delete cc;
}

void ATSLogger::stop_all_capture()
{
	lock();
	CaptureContextMap::iterator i = m_cc.begin();
	CaptureContextMap tmp = m_cc;

	while(i != m_cc.end())
	{
		m_cc.erase(i);
		++i;
	}

	unlock();

	{
		CaptureContextMap::const_iterator i = tmp.begin();

		while(i != tmp.end())
		{
			delete (i->second);
			++i;
		}

	}

}

void* ATSLogger::h_capture_fd(void* p)
{
	CaptureContext& cc = *((CaptureContext*)p);
	const int fd = cc.m_pipe_capture[0];
	const int pipe_read = cc.m_pipe_com[0];
	ATSLogger& l = *(cc.m_log);

	for(;;)
	{
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
		FD_SET(pipe_read, &rfds);
		const int max_fd = (pipe_read > fd) ? pipe_read : fd;
		const int retval = select(max_fd + 1, &rfds, NULL, NULL, NULL);

		if(-1 == retval)
		{
			ats_logf(&l, "%s,%d: select failed: (%d) %s", __FILE__, __LINE__, errno, strerror(errno));
			break;
		}

		if(FD_ISSET(pipe_read, &rfds))
		{
			break;
		}

		if(FD_ISSET(fd, &rfds))
		{
			char buf[4096];
			const ssize_t nread = read(fd, buf, sizeof(buf));

			if(nread <= 0)
			{
				break;
			}

			l.write(buf, nread);
		}

	}

	return 0;
}

void ATSLogger::compress_time(bool p_compress)
{
	m_compress_time = p_compress;
}

