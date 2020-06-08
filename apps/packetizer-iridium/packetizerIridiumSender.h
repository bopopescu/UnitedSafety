#pragma once
#include "ats-common.h"
#include "packetizerSender.h"

class PacketizerIridiumSender : public PacketizerSender
{
public:
	PacketizerIridiumSender(MyData& p_mdata);

	void waitForNetworkAvailable();
	void waitForSBDReady();
	bool sendMessage(std::vector< char> &p_msgBuf);
	bool waitforack();
	ats::String errorStr();
	int error(){return m_sbd_error;}
private:
	std::vector < ats::String > m_config;
	MyData *m_data;
	IRIDIUM m_pIridium;
	uint m_sbd_retries;
	int m_sbd_error;
	ats::String m_error;
};

