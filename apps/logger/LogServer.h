#pragma once

#include "pthread.h"

#include "ats-common.h"
#include "atslogger.h"

class ClientSocket;

class LogServer
{
public:
	class LogRequest
	{
	};

	typedef bool (*LogFn)(LogServer&, LogRequest& p_log_request);

	// Description: A map of [Log Type]---to--->[Log String Generator Function].
	typedef std::map <const ats::String, LogFn> LogFnMap;
	typedef std::pair <const ats::String, LogFn> LogFnPair;

	typedef std::map <const ats::String, ats::StringMap> LogRequestMap;
	typedef std::pair <const ats::String, ats::StringMap> LogRequestPair;

	LogServer();

	//===================================================================
	// Logging request functions:
	//===================================================================
	ats::String log_on_time(const ats::StringMap&);

	ats::String log_once(const ats::StringMap&);

	ats::String log_on_changed(const ats::StringMap&);

	ats::String log_on_new(const ats::StringMap&);

	//===================================================================
	// Logging function registration:
	//===================================================================
	ats::String set_logger(const ats::String&, LogFn);
	//ats::String unset_logger(const ats::String&);

	//===================================================================
	// Logging notification:
	//===================================================================
	void log_new_event(const ats::String& p_log_name);
	void log_changed_event(const ats::String& p_log_name);

	ClientSocket* m_log_server_cs;
	pthread_t m_log_server;
	void start_log_server();

	std::vector <char> m_log_buf;

	void* m_data;
private:
	// Description: A map of all supported log types and their string generator functions.
	LogFnMap m_onlog;

	// Description: A list of all logs that should generate "on new" events.
	LogRequestMap m_on_new;

	// Description: A list of all logs that should generate "on changed" events.
	LogRequestMap m_on_changed;

	void log_notify(const ats::String& p_log_name, LogRequestMap& p_lrm);

	pthread_mutex_t m_mutex;
	pthread_mutex_t m_mutex_send_log;

	void lock();
	void unlock();

	void lock_send_log();
	void unlock_send_log();

	LogServer(const LogServer&);
	LogServer& operator=(const LogServer&);

	static void* log_server(void*);
	static void h_log_server(LogServer& p_log, ClientSocket*& p_cs);

	void send_log(
		const char* p_log_key,
		const char* p_log_des,
		const char* p_log_time);

	void update_log_request_map(LogRequestMap& p_lrm, const ats::String& p_log_name, const ats::String& p_des, bool p_remove);
};
