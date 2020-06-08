#pragma once

#include "ats-common.h"

#define ATSLOG(P_level) (ATSLogger::global_logger(P_level))

#define LOG_LEVEL_NONE  (0)
#define LOG_LEVEL_ERROR (1)
#define LOG_LEVEL_DEBUG (2)
#define LOG_LEVEL_INFO  (3)
#define ATSLOG_NONE  (ATSLOG(LOG_LEVEL_NONE))
#define ATSLOG_ERROR (ATSLOG(LOG_LEVEL_ERROR))
#define ATSLOG_DEBUG (ATSLOG(LOG_LEVEL_DEBUG))
#define ATSLOG_INFO  (ATSLOG(LOG_LEVEL_INFO))


// Description: Message logging helper class.
//
//    This class, and all of its functions, are thread safe (exceptions are noted in the function descriptions).
//
//    The ATSLogger instance must NOT be destroyed until all threads using the instance
//    have terminated (or will no longer use the logger instance).
//
//    Each ATSLogger manages one logging file. Therefore multiple instances can be created
//    to log to different files.
//
//    NOTE: Using two ATSLogger instances to write/log to the same file will produce
//          undefined results.
class ATSLogger
{
public:
	static const size_t m_default_log_file_size = 131072;

	// Description: Creates a new independent logging instance.
	ATSLogger();

	ATSLogger(const ats::String& p_user, int p_mode);

	// Description:
	//
	// XXX: This destructor is not thread safe. Do not call until all threaded (concurrent)
	//	usage of this class instance has stopped. This is an obvious limitation (cannot destroy a
	//	resource that is in use).
	virtual~ ATSLogger();

	// Description: Sets the global logger to "p_log". This logger will be used by libraries that need
	//	to output logging (libraries and other low-level code will call "ATSLogger::get_global_logger"
	//	when logging is required).
	//
	// XXX: This function is NOT thread safe, and therefore must be used before threading starts (otherwise
	//      all threads must be stopped/halted before calling this function).
	//
	// Parameters:
	//    p_log - If "p_log" is NULL, then no global logger will be set. A non-NULL logger set must remain
	//            valid (cannot be deleted), since libraries or low-level functions may access it.
	static void set_global_logger(ATSLogger* p_log);

	// Description: Returns the current global logger, or NULL if there is no global logger set.
	//
	// Return: The current global logger, or NULL if there is no global logger.
	static ATSLogger* get_global_logger();

	// Description: Returns the current global logger only if it exists and its logging level is "p_level" or
	//	greater. Otherwise NULL is returned. This behavior is used to limit logging to specific levels.
	//
	// Return: The current global logger, or NULL if there is no global logger.
	static ATSLogger* global_logger(int p_level);

	// Description: Returns "this" ATSLogger if "p_level" less than or equal to the current logging level "m_level".
	ATSLogger* operator()(int p_level);

	// Description: Set the current logging level.
	//
	// Return: The current logging level.
	int set_level(int p_level);

	// Description: Get the current logging level.
	//
	// Return: The current logging level.
	int get_level() const;

	// Description: Opens file "p_fname" for logging. If this is the second call
	//	to open, then the previous file is closed, and the new file is opened
	//	(this feature can be used to switch logging files without having to notify
	//	the functions/threads calling the logging functions).
	//
	//	By default, log files are opened in "append" mode. If "p_append" is false, then
	//	the named file is overwritten.
	//
	// ATS FIXME: When logging to multiple files, should continue off of file with largest numbered extension.
	//	(Do not start appending from the first file in the set so that it is simpler for users to
	//	follow/read).
	//
	// Return: A negative errno number is returned on error, or -1 if the error is generic or unknown.
	//	Zero is returned on success.
	int open(const ats::String& p_fname, bool p_append=true);

	// Description: Opens file "<p_app_name>.log" in the standard "/var/log/testdata" directory. If
	//	"/var/log/testdata" does not exist, then the log file will be opened in "/var/log".
	//
	//	When "/var/log/testdata" exists, the log file will have infinite size, otherwise it will
	//	be whatever default size that "ATSLogger::open" uses.
	//
	// Return: A negative errno number is returned on error, or -1 if the error is generic or unknown.
	//	Zero is returned on success.
	int open_testdata(const ats::String& p_app_name, bool p_append=true);

	// Description: Writes "p_msg" (up to "p_len" characters from "p_msg") to the log file using
	//	the following format:
	//
	//	<date time><p_msg><\n>
	//
	//	Nothing is written to the log file if "p_msg" is NULL or if "p_len" is zero.
	//
	// ATS FIXME: When the output is to syslog, the output stops at the first NULL byte. All characters
	//	should be written to syslog.
	//
	// XXX: This function also supports writing imbedded NULL bytes in "p_msg".
	//
	// Return: The total number of characters written (which, for a successful function call,
	//	will always be greater than "p_len" due to the addition of date/time and newline character).
	//	On error, a negative number is returned. If there is no file opened to log to, then
	//	"p_msg" is written to syslog and zero is returned.
	//
	//	NOTE: If "m_out" is not NULL, then it will override syslog, and messages will be sent to
	//	"m_out". The format of the messages sent to "m_out" is exactly the same as "syslog" except
	//	that a newline is appended to messages going out "m_out".
	int write(const char* p_msg, size_t p_len);

	// Description: Same as "write" above however no time string or newline is written (the string "p_msg"
	//	is written "as-is").
	int write_raw(const char* p_msg, size_t p_len);

	size_t set_cur_file_number(size_t p);
	size_t set_max_file_number(size_t p);
	size_t set_max_size(size_t p);

	size_t get_cur_file_number() const;
	size_t get_max_file_number() const;
	size_t get_max_size() const;

	void flush();

	bool capture_fd(int p_fd);
	void stop_capture_fd(int p_fd);
	void stop_all_capture();

	// Description: Specify a file to override syslog, such as "stdout" or "stderr".
	//
	//	"p_out" will only be used for messages if "m_file" has not been set.
	//
	//	The "p_out" override can be cleared by passing NULL for "p_out".
	void set_output(FILE* p_out);

	void set_syslog_priority(int p_pri);

	// Description: Enables time compression in logs if "p_compress" is true, and disables
	//	time compression otherwise. By default, time is not compressed in log files.
	//
	//	XXX: This function shall be called before logging starts. Do not call it after
	//	     logging has started (since race conditions could result in unpredictable
	//	     behaviour).
	//
	//	AWARE360 FIXME: Add support for calling this function at any time.
	void compress_time(bool p_compress);
	

private:

	class CaptureContext
	{
	public:
		CaptureContext(ATSLogger& p_log)
		{
			m_log = &p_log;
			m_pipe_capture[0] = -1;
			m_pipe_capture[1] = -1;
			m_pipe_com[0] = -1;
			m_pipe_com[1] = -1;
			m_started = false;
		}

		~CaptureContext();

		bool start();

		ATSLogger* m_log;
		pthread_t m_thread;
		int m_pipe_capture[2];
		int m_pipe_com[2];
		bool m_started;
	};

	typedef std::map <int, CaptureContext*> CaptureContextMap;
	typedef std::pair <int, CaptureContext*> CaptureContextPair;
	CaptureContextMap m_cc;

	void init_logger();

	void lock() const;
	void unlock() const;

	ATSLogger(const ATSLogger&);
	ATSLogger& operator=(const ATSLogger&);

	int h_open(const ats::String& p_fname, bool p_append=true);

	int h_rotate_log_file_if_too_big();

	int h_rotate_file();

	ats::String m_dir;
	ats::String m_fname;
	ats::String m_flush_fname;
	FILE* m_out;
	FILE* m_file;
	pthread_mutex_t* m_mutex;
	size_t m_cur_file_number;
	size_t m_max_file_number;
	size_t m_max_size;

	int m_syslog_priority;
	int m_level;
	int m_uid;
	int m_gid;
	int m_mode;

	size_t m_prev_sec;
	size_t m_prev_usec;
	bool m_compress_time;
	bool m_first_compressed_time;

	static ATSLogger* m_global_logger;

	static void* h_capture_fd(void*);
};

// Description: "printf" style logging for ATSLogger. If "p_log" is NULL, then zero is returned
//	and nothing is logged.
//
// Return: A negative errno number is returned on error, or -1 if the error is generic or unknown.
//	The number of characters written to the log file is returned otherwise (which may be
//	zero).
EXTERN_C int ats_logf(ATSLogger* p_log, const char* p_format, ...)
	__attribute__ ((format (printf, 2, 3)));

EXTERN_C int ats_logf_raw(ATSLogger* p_log, const char* p_format, ...)
	__attribute__ ((format (printf, 2, 3)));

// Description: "std::cout" style logging for ATSLogger.
//
// Return: A negative errno number is returned on error, or -1 if the error is generic or unknown.
//	The number of characters written to the log file is returned otherwise (which may be
//	zero).
template <class T> int ats_log(ATSLogger& p_log, const T& p_t)
{
	std::stringstream s;
	s << p_t;
	return ats_logf(&p_log, "%s", s.str().c_str());
}
