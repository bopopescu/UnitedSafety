#pragma once
#include "ats-common.h"
#include "AFS_Timer.h"

#include "Iridium.h"

class IridiumSender
{
public:
	typedef enum {
		MESSAGE_BUFFER_ERROR = -3,
		IRIDIUM_OVERLIMIT_REACHED = -2,
		SBDIX_CMD_TIMEOUT = -1

	} MessageStatus;
	IridiumSender();

	void start();
//298
	void IridiumGetResponse(char * p_msgBuf);
//298
	bool isNetworkAvailable();
	bool isSBDReady();
	void waitForNetworkAvailable();
	void waitForSBDReady();
	bool sendMessage(std::vector< char> &p_msgBuf);
	bool sendMessageWithDataLimit(std::vector<char> &p_msgBuf);
	ats::String errorStr() { return errorStr(m_sbd_error);}
	ats::String errorStr(int err);
	static bool isMessageSent(int resp);
	int error(){return m_sbd_error;}
private:
	void setSBDDelay();
	std::vector < ats::String > m_config;

	const ats::String m_appName;
	static int m_byteCount;
	static int m_byteLimit;
	static int m_elapsedTime;
	static bool m_dataLimitBreached;
	static AFS_Timer m_dataLimitTimer;
	static int m_dataLimitTimeout;

	IRIDIUM m_pIridium;
	AFS_Timer m_sbd_timer;
	int m_sbd_delay;
	int m_sbd_retries;
	int m_sbd_error;
	ats::String m_error;

	void resetByteCount();
};

