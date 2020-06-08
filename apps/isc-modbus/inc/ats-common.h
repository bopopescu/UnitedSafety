#pragma once

#include <map>
#include <vector>
#include <string>
#include <sstream>

#include "gcc.h"

#define ATS_TEST_MODE_FILE "/mnt/nvram/config/testmode"
#define ATS_LOG_KEY "log"
#define ATS_LOG_DB "/mnt/update/database/log.db"

namespace ats
{

template <class T> void ignore_return(T)
{
}

typedef std::string String;

template <class T> String toStr(const T &p_t)
{
	std::stringstream s;
	s << p_t;
	return s.str();
}

extern const String g_zero_length;
extern const String& g_empty;

// Description: Returns a human readable date given the number of seconds "p_sec"
//	since the Unix epoch.
//
//	If "p_weekday" is false, then no day of week name is included (i.e. Sunday to Saturday).
//
// Return: A human reable date in the following format:
//	Weekday, YYYY/MM/DD - HH:MM:SS
ats::String human_readable_date(const time_t p_sec, bool p_weekday=true);

// Description: Same as the above function except that the current system time is used.
ats::String human_readable_date(bool p_weekday=true);

// Description: Parses string "p_val" and returns its boolean conversion (true or false).
//
// Return: True is returned for (case insensitive) "true", "on", "set", "y", "yes" or any
//	non-zero number (format must be decimal, octal or hex). All other values return
//	false.
//
//	NOTE: A hexidecimal number must be preceeded with "0x", an octal with "0". Otherwise
//	the number is assumed to be in decimal format.
bool get_bool(const String& p_val);

bool chop(const String& p_match, String& p_str);

int daemonize(bool p_exit_parent=true);

int detach(bool p_exit_parent=true);

// Description:
//
// Return: 0 is returned on success and p_uid and p_gid are set to the user ID and group ID respectively.
//	A negative errno number is returned otherwise.
int get_uid_gid(const String& p_user, int& p_uid, int& p_gid);

int get_uid(const String& p_user, int& p_uid);
int get_gid(const String& p_group, int& p_gid);

int su(const String& p_user);
int su(const String& p_user, const String& p_group);

class StringMap : public std::map <const String, String>
{
public:
	typedef std::pair <const String, String> StrStrPair;

	// Description: Returns a constant reference the value pointed to by "p_key".
	//	If there is no value "p_key" then a reference to "m_zero_length"
	//	is returned.
	const String &get(const String &p_key) const;
	const String &get(const String &p_key, const ats::String& p_default) const;

	// Description: Sets "p_des" to the value with key "p_key" only if "p_key" exists.
	//	If "p_key" does not exist, then "p_des" is not modified. This function is
	//	more efficient than "has_key" followed by "get", since it only requires
	//	one map look-up, while the later method requires two.
	//
	//	"p_exists", if specified, is set to true if "p_des" was modified, and false
	//	is returned otherwise ("p_key" did not exist).
	//
	// Return: A reference to "p_des" is always returned.
	ats::String& get_if_exists(String& p_des, const String& p_key, bool& p_exists) const;
	ats::String& get_if_exists(String& p_des, const String& p_key) const;

	// Description: Returns the boolean conversion of the value for key "p_key"
	//	(using conversion function "ats::get_bool"). See "ats::get_bool" for
	//	more information. If key "p_key" does not exist, then "p_default"
	//	is returned.
	//
	// Return: Returns true/false depending on the boolean conversion of the value with
	//	key "p_key". If "p_key" does not exist then "p_default" is returned.
	bool get_bool(const String &p_key, bool p_default=false) const;

	// Description: Returns the integer value of the string referenced by "p_key".
	int get_int(const String &p_key, int p_default=0) const;

	// Description: Returns the floating point value of the string referenced by "p_key".
	float get_float(const String &p_key, float p_default=0.0f) const;

	// Description: Returns the double precision value of the string referenced by "p_key".
	double get_double(const String &p_key, double p_default=0.0) const;

	// Description: Returns the long long value of the string referenced by "p_key".
	long long get_long_long(const String &p_key, long long p_default=0) const;

	// Description: Returns true if "p_key" exists in the mapping, and false is
	//	returned otherwise.
	bool has_key(const String &p_key) const;

	// Description: Adds key "p_key" with value "p_value" to the mapping. By default
	//	if "p_key" already exists in the mapping, then its value will be
	//	overwritten.
	//
	// Return: True is returned if the value was set, and false is returned otherwise
	//	(for example, "false" is returned if the key "p_key" already exists, and
	//	"p_overwrite" is false).
	bool set(const String &p_key, const String &p_value, bool p_overwrite=true);

	// Description: Removes key "p_key" from the map.
	void unset(const String &p_key);

	// Description: Creates a mapping of "key/value" pairs from the given argument list.
	//
	//	Key/value pairs are defined as a group of non-white space characters. The
	//	key portion is on the left of the "=" sign, and the value portion is on the
	//	right side. If there is no "=" sign then the entire token is treated as a
	//	key value.
	void from_args(int p_argc, char* p_argv[]);

	// Description: Same as "from_args" above, except that "-" option arguments are
	//	also processed.
	//
	//	Any token starting with a single "-" followed by characters is treated as
	//	a collection of option characters. Each option will be set in the map as
	//	"-<character>".
	//
	//	Any token starting with "--" is treated as a normal key/value pair. For
	//	example "--list" means key="--list" where value is the empty string, and
	//	"--list=a,b,c" means key="--list" and the value part is "a,b,c".
	//
	//	"--" means the end of option processing.
	void from_args(int p_argc, char* p_argv[], StringMap& p_opt);

	void from_file(const ats::String& p_fname);

	// Description: Takes string "p_s" (which should be in the form "key=value") and
	//	extracts the "key" and "value" part, returning them in "p_key" and
	//	"p_val" respectively.
	//
	//	If "p_s" is the NULL string, then "p_key" and "p_val" will be set to the
	//	empty string.
	static void get_key_val(const char *p_s, String &p_key, String &p_val);

	static const String& m_zero_length;
};

class StringMapMap : public std::map <const String, StringMap>
{
public:
	typedef std::pair <const String, StringMap> StrMapPair;

	// Description: Returns the StringMap with key "p_key". If StringMap "p_key" does
	//	not exist, then it will be created and returned (use "has_key" to test
	//	for the presence of a StringMap without creating one).
	StringMap& get(const String& p_key);

	// Description: Returns true if "p_key" exists in the mapping, and false is
	//	returned otherwise.
	bool has_key(const String &p_key) const;

	// Description: Removes key "p_key" from the map.
	void unset(const String &p_key);
};

// Description: Returns true or false if the file "p_fname" exists on the filesystem.
//
// Return: True is returned if the file "p_fname" exists, and false is returned otherwise.
bool file_exists(const String& p_fname);
bool file_exists(const char* p_fname);

bool dir_exists(const ats::String& p_path);

// Description: Comapres two files using the "diff" application.
//
// Return: 0 is returned if the files are identical. 1 is returned if they differ, or if an error occured.
bool diff_files(const ats::String& p_a, const ats::String& p_b);

bool touch(const String& p_fname);

// Description: Like the "stdlib.h" "system" command, where command "p_cmd" is executed
//	by the shell. The standard output is stored in "p_stdout", unless "p_stdout" is
//	NULL, in which case standard output messages are thrown away.
//
// XXX: Standard error messages are lost, however if you wish to capture them and there
//	is no problem with mixing standard error and standard output, then redirect the
//	standard error of "p_cmd" to standard output.
//
//		Example: ats::system("./HelloWorld.sh 2>&1", p_stdout);
//
// XXX: This function is implemented using the "popen" system call.
//
// Return: The return value of the system call "pclose" is returned (see man page for
//	"pclose"), however an alternative/simpler explanation follows:
//
//	-1 is returned on error and "errno" is set appropriately. Otherwise the
//	process exit status is returned. The format of the process exit status is
//	described in the Linux man page of "waitpid", DESCRIPTION section, for parameter
//	"status".
//
//	Example:
//
//		const int ret = ats::system("./HelloWorld.sh");
//
//		if(WIFEXITED(ret))
//		{
//			printf("My exit status is: %d\n", WEXITSTATUS(ret));
//		}
//		else if(WIFSIGNALED(ret))
//		{
//			printf("My received signal is: %d\n", WTERMSIG(ret));
//		}
//		else if(WCOREDUMP(ret))
//		{
//			printf("Core dump occurred\n");
//		}
//		else ( /* and so on, see man page on "waitpid" for more status test macros */ )
//
int system(const String &p_cmd, std::ostream *p_stdout=0);

// Description: Reads file "fname" and places the contents into string "p_des". If "p_len"
//	is greater than zero, then only "p_len" characters will be read. Otherwise if "p_len"
//	is zero, then the whole file will be read.
//
// Return: Zero is returned on success and a negative number is returned on error.
int read_file(const String &p_fname, String &p_des, size_t p_len=0);

class ReadDataCache
{
public:

	ReadDataCache()
	{
		m_remain = 0;
	}

	virtual~ ReadDataCache()
	{
	}

	int getc();

private:
	char m_buf[1024];
	char* m_c;
	size_t m_remain;

	virtual ssize_t do_read(char* p_buf, size_t p_len) = 0;

	ReadDataCache(const ReadDataCache&);
	ReadDataCache& operator= (const ReadDataCache&);
};

class ReadDataCache_fd : public ReadDataCache
{
public:

	ReadDataCache_fd(int p_fd)
	{
		m_fd = p_fd;
	}

	virtual~ ReadDataCache_fd()
	{
	}

private:
	int m_fd;

	virtual ssize_t do_read(char* p_buf, size_t p_len);
};

class ReadDataCache_FILE : public ReadDataCache
{
public:

	ReadDataCache_FILE(FILE* p_f)
	{
		m_f = p_f;
	}

	virtual~ ReadDataCache_FILE()
	{
	}

private:
	FILE* m_f;

	virtual ssize_t do_read(char* p_buf, size_t p_len);
};

int read_cached(int p_fd, ReadDataCache& p_cache);

// Description: Reads a single specified line from file "p_fname". The line read (excluding the
//	terminating newline character) is returned in p_des.
//
//	"p_len" is used to limit the number of characters returned in "p_des". If "p_len" is 0
//	then it means that there is no limit to the length of "p_des".
//
// XXX: Lines are numbered starting at 1. Use "p_line = 1" to read the first line of a file.
//
// XXX: This function call is generally more efficient than using a shell "system" call with
//	"head" or "tail" to read a single line from a file.
//
// Return: 0 is returned on success and a negative error number is returned otherwise. The contents
//	of "p_des" are undefined on error.
int get_file_line(String& p_des, const String &p_fname, size_t p_line, size_t p_len = 0);

// Description: Same as "get_file_line" above, except that the already opened file (cached data stream)
//	in "p_cache" is used. The first line is the current line of "p_cache". Data stream "p_cache" is
//	not closed when this function returns, and so this function is useful for parsing a file multiple times
//	without closing it.
//
// Return: Same as "get_file_line" above. ReadDataCache (file handle/cached data stream) "p_cache"
//	is not closed on return. -ENODATA is returned at end of file.
int get_file_line(String& p_des, ReadDataCache& p_cache, size_t p_line, size_t p_len = 0);

// Description: Writes string "p_data" to file "p_fname".
//
//	p_fname - The file name (which may include a path).
//
//	p_data - The string to write to the file.
//
//	p_mode - The file write mode, which is exactly as specified for the "fopen" stream open function.
//		The default mode is "w" (which means truncate existing files, and create non-existant files).
//		See the man page for "fopen" for additional modes.
//
// Return: The number of characters written is returned.
int write_file(const String &p_fname, const String &p_data, const char* p_mode="w");

String& rtrim_newline(String& s, char p_char='\n');

String ltrim(const ats::String& p_s, const ats::String& p_char, size_t p_count=0);

// Description: Removes all characters defined in "p_char" from the right side of string "p_s".
//
//	Only up to "p_count" characters will be removed. If p_count is zero, then it means
//	there is no limit to the number of characters removed.
//
// Return: The trimmed string is returned.
String rtrim(const ats::String& p_s, const ats::String& p_char, size_t p_count=0);

void infinite_sleep();

// Description: Converts string "p_src" to hexadecimal representation and stores the result in the buffer at
//	"p_des". The maximum amount of data written will be "p_des_len".
//
//	XXX: "p_des" will NOT be NULL terminated.
//
// Return: "p_des" is returned. "p_end", if not NULL, is updated to point to the end of the hexadecimal conversion
//	(within "p_des") is returned. If there is no conversion done, then "p_end" will point to "p_des".
char* to_hex(char* p_des, size_t p_des_len, const String& p_src, char** p_end=0);

String to_hex(const String& p_s);
String to_hex(std::vector <char> & p_src);

String& to_hex(ats::String& p_des, const String& p_src);

String from_hex(const String& p_s);

// Description: Returns the time from "p_tv" using format "p_format".
//
//	See the "Linux Programmer's Manual" for "strftime" to get documentation on the
//	format requirements.
//
// Return: The time formatted according to "p_format" is written in "p_buf". NULL is returned
//	if "p_buf" is NULL or "p_max" is zero, otherwise "p_buf" is returned.
char* getstrtimef(const struct timeval& p_tv, const String& p_format, char* p_buf, size_t p_max);

// Description: Returns the system time (from gettimeofday) using format "p_format".
//
//	This is just an overload of "getstrtimef" above.
char* getstrtimef(const String& p_format, char* p_buf, size_t p_max);

// Description: Returns the system time (from gettimeofday) in a human-readable format.
//	The time returned has a 1 second resolution.
//
//	The format of the time returned is: YYYY-MM-DD HH:MM:SS
//
// Return: A human-readable time string is returned.
String getstrtime();

// Description: Exactly like "getstrtime" except that micro seconds are appened at
//	the end as ".XXXXXX".
//
// Return: A human-readable time string is returned.
String getstrtime_ms();

// Description: Exactly like "getstrtime_ms" above, except that the time is returned
//	in "p_buf".
//
// Return: A human-readable time string is returned in "p_buf" (a reference to "p_buf"
//	is returned).
String& getstrtime_ms(String& p_buf);

// Description: Exactly like "getstrtime_ms" above, except that the time is not "now" and
//	is instead specified by "p_tv".
//
// Return: Same as "getstrtime_ms" above.
String& getstrtime_ms(String& p_buf, struct timeval& p_tv);

void get_command_key(const String& p_arg, String& p_cmd, String& p_key);

class CommonData
{
public:
	CommonData();
	virtual ~CommonData();

	String get(const String &p_key) const;
	String get(const String &p_key, const ats::String& p_default) const;

	bool get_bool(const String &p_key, bool p_default=false) const;

	int get_int(const String &p_key, int p_default=0) const;

	// Description: Adds key "p_key" with value "p_value" to the mapping. By default
	//	if "p_key" already exists in the mapping, then its value will be
	//	overwritten.
	//
	// Return: True is returned if the value was set, and false is returned otherwise.
	bool set(const String &p_key, const String &p_val, bool p_overwrite=true);

	void unset(const String &p_key);

	void set_from_args(int p_argc, char* p_argv[]);

	void set_from_file(const ats::String& p_fname);

	void copy_config(ats::StringMap& p_des) const;

	// Description: General purpose lock/unlock functions which may be used for anything.
	//
	// XXX: CommonData does not use the locks. So it is safe to mix calls to lock with
	//	any other CommonData function.
	void lock_data() const;
	void unlock_data() const;

private:
	ats::StringMap m_config;
	pthread_mutex_t* m_config_mutex;
	pthread_mutex_t* m_data_mutex;

	void lock_config() const;
	void unlock_config() const;

	CommonData(const CommonData&);
	CommonData& operator =(const CommonData&);
};

// Description: Encodes a string into base64.
//
// Return: A string in base64 format.
ats::String base64_encode(const ats::String& p_src);

// Description: Decodes a string from base64 format.
//
// Return: The decoded string.
ats::String base64_decode(const ats::String& p_src);

// Description: Returns the TRULink RDS hardware configuration parameters (as "key=value" pairs) in "p_hw_cfg".
//
// Return: 0 is returned on success, and a negative errno number is returned instead.
int get_hw_config(ats::StringMap& p_hw_cfg);

bool is_trulink_model(const ats::String& p_type);

bool is_trulink_model(const ats::String& p_type, const ats::StringMap& p_hw_cfg);

bool testmode();

// Description: Returns true/false depending if the process (program) was called as "p_program_name".
//
//	"p_argv_entry" is the process name as given in "argv[0]" in the "main" function.
//	"p_program_name" is the process name to test against.
//
// Return: True is returned if the process was called as "p_program_name" and false is returned otherwise.
bool called_as(const char* p_argv_entry, const char* p_program_name);

// Description: Returns the non-negative thread ID of the caller. On error -1 is returned and
//	"errno" will contain the error code.
int gettid();

String& getenv(String& p_des, const char* p_key);

extern const char g_hextable[128];

enum TRULINK_MODEL_REV {
	TRULINK_MODEL_REV_2_0 = 0,
	TRULINK_MODEL_REV_2_00 = 1,
	TRULINK_MODEL_REV_3_0 = 2
};

enum TRULINK_HID {
	TRULINK_HID_UNKNOWN = 0,
	TRULINK_HID_2500 = 1,
	TRULINK_HID_3000 = 2,
	TRULINK_HID_5000 = 3	
};

extern const ats::String g_trulink_hid_2500;
extern const ats::String g_trulink_hid_3000;
extern const ats::String g_trulink_hid_5000;
extern const ats::String g_trulink_hid_error;
extern const ats::String g_trulink_hid_unknown;

// Description: Returns the TRULink model revision. This function queries the TRULink hardware directly to
//	determine the model type.
//
//	XXX: This function performs analog to digital conversions when trying to determine the model type.
//	     Therefore it is possible for it to decode the wrong model on marginal hardware. Use
//	     "get_stored_trulink_model" which is more reliable (since it first checks the system cache
//	     for hardware information and returns that if it is available). If "get_stored_trulink_model" fails
//	     it will fall back to calling "get_trulink_model" which will query the analog hardware as usual.
//
//	If "p_rev" is not NULL, then the model revision is returned in "p_rev".
//
//	If "p_model_code" is not NULL, then the code used to determine the model is returned in "p_model_code".
//
// Return: The TRULink model is returned (one of "g_trulink_hid_*"). On error "g_trulink_hid_error" is returned.
//	If the TRULink model is unknown then "g_trulink_hid_unknown" is returned.
const ats::String& get_trulink_model(TRULINK_MODEL_REV* p_rev=0, int* p_model_code=0);

// Description: Exactly like "get_trulink_model" except that this function checks the reliable/stable system
//	cache before checking the analog hardware for model information.
const ats::String& get_stored_trulink_model(TRULINK_MODEL_REV* p_rev=0, int* p_model_code=0);

// Description: Updates the system cache with the given TRULink hardware model information.
//
// Return: 0 is returned on success and an errno error value is returned otherwise.
int store_trulink_model(TRULINK_HID p_hid, TRULINK_MODEL_REV p_rev, int p_model_code);

// Description: Returns the TRULink firmware version information.
//
//	"p_revision" is the first line of "/version" which contains the SVN revision number. If
//	"p_revision" is not NULL, then it will be assigned with the SVN revision number.
//
//	"p_full_version" is the 3rd line of "/version" which contains the release version specification
//	along with the SVN revision number (example: 1.8-RLS.8923). If "p_full_version" is not NULL,
//	then it will be assigned with the full release version.
//
//	"p_build_Date" is the 2nd line of "/version" which contains the build date. If "p_build_date" is
//	not NULL, then it will be assigned with the build date.
//
// Return: 0 is returned if the version parameters requested are read correctly. A negative number is returned on error.
int trulink_firmware_version(
	ats::String* p_revision,
	ats::String* p_full_version=0,
	ats::String* p_build_date=0);
int mkdir(const ats::String& p_path, bool p_auto_make_parents=false, int p_mode=0755);

int cp(const ats::String& p_src, const ats::String& p_des);
};

EXTERN_C int ats_sprintf(ats::String* p_des, const char* p_format, ...)
	__attribute__ ((format (printf, 2, 3)));

EXTERN_C int ats_ssprintf(std::stringstream* p_des, const char* p_format, ...)
	__attribute__ ((format (printf, 2, 3)));
