#pragma once
#include <stdio.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <vector>

#include <CharBuf.h>
#include <AFS_Timer.h>
#include <atslogger.h>
#include "ats-string.h"

//---------------------------------------------
// Iridium class - used to send messages via the iridium modem
//
// We also generate a thread for reading the modem data.
//

#define IRIDIUM_PORT "/dev/ttySP4"


class IRIDIUM
{

public:
	enum RESPONSE_CODES
	{
		OK,
		ERROR,
		READY,
		UNKNOWN,
		EMPTY,
		ZERO,
		ONE,
		TWO,
		THREE,
		NONE
	};

	enum STATUS_CODES
	{
		NOT_READY,
		PASSED,
		FAILED
	};

	struct SBD_Status
	{
		unsigned char mo_status;
		ushort momsn;
		unsigned char mt_status;
		ushort mtmsn;
		uint mt_length;
		uint mt_queued;
	};

private:
	static int m_cmd_pipe[2];
	pthread_t m_reader_thread;
	pthread_t m_proccessMessageThread;

	static pthread_mutex_t m_cmdBufferMutex; //mutex for writing AT commands to Iridium port.
	static pthread_mutex_t m_respBufferMutex; //mutex for reading and writing to response buffer.
	static pthread_mutex_t m_statusMutex; //mutex for status

	static STATUS_CODES m_status;
	ats::String m_strIMEI;

	bool m_bNetworkAvailable;
	short m_rssi;  // indicated signal strength (0-5 bars)

public:
	static bool bTxInProgress; //<ISCP-238>
// 298	
	char m_MessageRecieved[40];
	
// 298
	static ats::StringList m_ResponseBuf;  //array of responses excluding the ones watched by the reader thread
	std::vector<char> m_MTMessageBuf;  // Binary byte array for storing MT message

	IRIDIUM();
	virtual ~IRIDIUM();

	void init();
	//298
	//void GetResponse(std::vector< char> &p_msgBuf);	
	//void ProcessIridiumResponse(std::vector<char>&  iridiumResponse);
	void GetResponse(char * p_msgBuf);	
	void ProcessIridiumResponse(std::string iridiumResponse);
	
	//298
	bool PrepareToSendMessage(char *msg, short len);
	int SendMessage();
	bool ClearMessageBuffer();

	bool IsNetworkAvailable(){return (m_bNetworkAvailable && (m_rssi > 2));}
	int ManualNetworkRegistration();  //Returns the status code for +SBDREG command
	bool WaitForResponse(const ats::String buf, const char *expected);
	bool SendSBD(const char *buf, short len);
	static unsigned short ComputeCheckSum(char *msg, short len);
	static unsigned short ComputeCheckSum(std::vector<char> &data);

	static void LockResponseBuf() {pthread_mutex_lock(&m_respBufferMutex);}
	static void UnlockResponseBuf() {pthread_mutex_unlock(&m_respBufferMutex);}

	static void LockCmdBuf() {pthread_mutex_lock(&m_cmdBufferMutex);}
	static void UnlockCmdBuf() {pthread_mutex_unlock(&m_cmdBufferMutex);}

	static void LockStatus() {pthread_mutex_lock(&m_statusMutex);}
	static void UnlockStatus() {pthread_mutex_unlock(&m_statusMutex);}

	static int ReadATResponses(IRIDIUM* p_iridium, CharBuf& p_buf, int p_fdr, int fdw);
	bool ProcessSBDIXResponse(const ats::String& response, struct SBD_Status& status);
	bool ProcessMTMessage(std::vector<char>& msg);

	static STATUS_CODES getStatus() {
		STATUS_CODES status;
		pthread_mutex_lock(&m_statusMutex);
		status = m_status;
		pthread_mutex_unlock(&m_statusMutex);
		return status;
	}
private:
	//*************************************************
	// SendATCommand()
	// Sends data to the serial port from outside the class without giving direct access to m_fd
	// Locking is not implemented in this command so that multiple AT commands can be sent in a
	// single locked AT port session.
	static void SendATCommand(const ats::String buf);
	bool IridiumSendMessage(const char *msg, short len);
	STATUS_CODES Setup();
	void SetPower(bool);
	static RESPONSE_CODES SendRespond(const ats::String buf);  // true if OK was returned - false for all others
	static ats::String GetLineFromResponseBuf();
	ats::String ReadImei();
	static void* reader_thread(void* p_iridium);
	bool DecodeCommandMessage(std::vector<char>msg);
};

