#pragma once

#include "AFFDevice.h"
#include "time.h"
#include "AFS_Timer.h"
#include "tinyxml.h"
#include <RedStone_IPC.h>

#define MAX_DATA_LEN (256)

enum
{
  GSM_NONE,
  GSM_ROGERS,
  GSM_BELL,
  GSM_ETHERNET,  // used if plugged in to ethernet
  GSM_NUM_PROVIDERS
};

class EmailParms
{
public:
  short m_gsm;

  EmailParms(){m_gsm = GSM_NONE;};
};

class AFF_Email : public AFFDevice
{
  EmailParms myParms;
  char m_bufOutgoing[MAX_DATA_LEN];  // latest data to be sent out
  short m_lenOutgoing;
  short m_CurSendStatus;  // indicates where we are in the sending process
  time_t m_MarkTime;  // used for timeing out on interface commands
  short nRetries;
  unsigned short mySeqNum;  // sequence number for messages sent (increment every message)
  int m_nValid, m_nInvalid;
  short m_SecsSinceSent;
  REDSTONE_IPC m_redStoneData;

public:
  AFF_Email();

  bool OpenPort();
  bool SendData(const char *data, short lenData);  // first call to send data to AFF port
  void Send_Queued_Messages_If_Any_Otherwise_Sleep();

  int GetFD(){return m_fdAFF; }

  void Setup();  // request user Setup of device

private:
  int Input_LogRate();
  void SetPPP();
  bool FoundEX_OK();
  bool is_pppd_running();
  void MailOneMessage();
  void RemoveLogFile();
};
