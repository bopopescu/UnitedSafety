#pragma once
#include <stdio.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <vector>

#include <CharBuf.h>
#include <AFS_Timer.h>
#include <atslogger.h>

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

  enum IRIDIUM_MODE
  {
    AT_MODE,
    SBD_MODE
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
  int m_cmd_pipe[2];
  pthread_t m_reader_thread;
  pthread_t m_proccessMessageThread;

  pthread_mutex_t m_atPortMutex; //mutex for writing AT commands to Iridium port.
  pthread_mutex_t m_respBufferMutex; //mutex for reading and writing to response buffer.


  bool m_bNetworkAvailable;
  bool m_bWaitingForSendToComplete;
  short m_rssi;  // indicated signal strength (0-5 bars)

public:
  std::vector<ats::String> m_ResponseBuf;  //array of responses excluding the ones watched by the reader thread
  std::vector<char> m_MTMessageBuf;  // Binary byte array for storing MT message
  IRIDIUM_MODE m_mode; //

  IRIDIUM();
  virtual ~IRIDIUM();
  bool PrepareToSendMessage(char *msg, short len);
  bool ClearMessageBuffer();

  bool IsNetworkAvailable(){return (m_bNetworkAvailable && (m_rssi > 0));}
  int ManualNetworkRegistration();  //Returns the status code for +SBDREG command
  int SendMessage();
  bool WaitForResponse(const ats::String buf, const char *expected);
  bool SendSBD(const char *buf, short len);
  static unsigned short ComputeCheckSum(char *msg, short len);
  static unsigned short ComputeCheckSum(std::vector<char> &data);

  void LockResponseBuf() {pthread_mutex_lock(&m_respBufferMutex);}
  void UnlockResponseBuf() {pthread_mutex_unlock(&m_respBufferMutex);}

  void LockATPort() {pthread_mutex_lock(&m_atPortMutex);}
  void UnlockATPort() {pthread_mutex_unlock(&m_atPortMutex);}

  static int ReadATResponses(IRIDIUM* p_iridium, CharBuf& p_buf, int p_fdr, int fdw);

  //*************************************************
  // SendATCommand()
  // Sends data to the serial port from outside the class without giving direct access to m_fd
  // Locking is not implemented in this command so that multiple AT commands can be sent in a
  // single locked AT port session.
  void SendATCommand(const ats::String buf);

  bool ProcessSBDIXResponse(const ats::String& response, struct SBD_Status& status);
  bool ProcessMTMessage(std::vector<char>& msg);

private:
  bool IridiumSendMessage(const char *msg, short len);
  void Setup();
  void SetPower(bool);
  RESPONSE_CODES SendRespond(const ats::String buf);  // true if OK was returned - false for all others
  ats::String GetLineFromResponseBuf();

  static void* reader_thread(void* p_iridium);

};

