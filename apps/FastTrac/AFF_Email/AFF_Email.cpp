// AFF_Email - handles sending data via email - bouncing off the gmail account to the server.
//  Used by cell systems and when the system is hooked up to a network via ethernet.
//
//  When a message comes in it is queued.
#include <string>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <time.h>

#include "AFF_Email.h"
#include "utility.h"
#include "ats-common.h"
#include <ConfigDB.h>  // gets the destination e-mail flag
#include "atslogger.h"

#define MAIL_LOG_FILE_NAME ("/var/log/AFF_Email-mail.log")

extern ATSLogger g_log;

std::string g_email = "cell@absolutetrac.com";
static size_t g_emails_sent = 0;
static size_t g_email_sequence_number = 0;
static size_t g_emails_failed = 0;
static int g_poll_period_seconds;
ats::String UnitID;
ats::String imei;

AFF_Email::AFF_Email()
{
  strcpy(m_DevName, "Email");
  m_fdAFF = -1;
  strcpy(m_ParmFileName, "/mnt/nvram/config/AFF_Email.prm");
  m_CurSendStatus = AFF_STATUS_DONE;
  mySeqNum = 0;
  m_SecsSinceSent = 0;
  
  db_monitor::ConfigDB db;

  ats::read_file("/mnt/nvram/rom/UnitID.txt", UnitID);
  imei = db.GetValue("RedStone", "IMEI", "123451234512345");
    
  ats::String value;
  value = db.GetValue("AFF_Email", "Dest", g_email);
  g_poll_period_seconds = db.GetInt("AFF_Email", "poll_period", 10);
  g_email = value;
  m_redStoneData.SendingEmail(false);

}

//-----------------------------------------------------------------------
// open the port at 9600
//
bool AFF_Email::OpenPort( )
{
  return true;
}

//----------------------------------------------------------
//  if (notSending) and queued messages
//    remove the mail log file /var/log/AFF_Email-mail.log
//    send a message over msmtp
//  else (if it is sending)
//    look for the log file
//    if it is there
//      does it contain EX_OK?
//        yes - delete message from queue and go to the next one
//        no - warn user that email failed and try again
//
void AFF_Email::Send_Queued_Messages_If_Any_Otherwise_Sleep()
{
  if (m_OutgoingMessages.GetNumElements() == 0)
  {
    sleep(g_poll_period_seconds);
    return;
  }

  if (!is_pppd_running() )  // are we resetting PPP???
  {
    m_SecsSinceSent++;
    return;
  }

  if (IsSending())
  {
    if (FileExists(MAIL_LOG_FILE_NAME))
    {
      if (FoundEX_OK())
      {
        m_OutgoingMessages.Delete(0, 1);  // get rid of the message in the queue
        ++g_emails_sent;
        std::stringstream s;
        s << "Emails sent to \"" << g_email << "\": " << g_emails_sent << " (" << m_OutgoingMessages.GetNumElements() << " in queue, seq# is " << g_email_sequence_number << ")\n"
          << "Emails failed: " << g_emails_failed << "\n";
        ats_logf(&g_log, "%s", s.str().c_str());
      }
      else
      {
        ++g_emails_failed;
        std::stringstream o;
        ats::system("cat /var/log/AFF_Email-mail.log", &o);
        ats_logf(ATSLOG(0), "Failed to send Email:\n[BEGIN LOG FILE (%zu bytes)]\n%s\n[END]", o.str().size(), o.str().c_str());
      }

      if (m_OutgoingMessages.GetNumElements() > 0)
        MailOneMessage();
      else
      {
        m_isSending = false;
        m_redStoneData.SendingEmail(false);
      }
        
      RemoveLogFile();
      m_SecsSinceSent = -10;
    }
    else
    {
      m_SecsSinceSent++;

      if (m_SecsSinceSent > 100)
      {
        ats_logf(&g_log, "10 seconds since sent - trying again");
        system("killall msmtp");
        m_isSending = false;
      }
    }
    return;
  }
  else // send it out
  {
    MailOneMessage();
  }
  return;
}

void AFF_Email::MailOneMessage()
{
  m_isSending = true;
  char cmdBuf[256];

  RemoveLogFile();

  char data[128];
  short lenData;
  AFFMessage msg;

  msg = m_OutgoingMessages.Peek(0);  // peek at the first one.  Delete it on confirmation
  lenData = msg.len;
  memcpy(data, msg.buf, lenData);
  data[lenData] = '\0';

  char fname[128];
  mySeqNum++;  // unique sequence number
  snprintf(fname, sizeof(fname) - 1, "/var/log/email%d.txt", mySeqNum % 5);
  fname[sizeof(fname) - 1] = '\0';

  FILE *fp = fopen (fname, "w");

  if (fp != NULL)
  {
    fprintf(fp, "Subject:[%s,%s] - EVT\r\n\r\n%s\r\n", ats::rtrim_newline(UnitID).c_str(), ats::rtrim_newline(imei).c_str(), data);
    fclose(fp);
    snprintf(cmdBuf, sizeof(cmdBuf) - 1, ("msmtp -X%s " + g_email + " < %s &").c_str(), MAIL_LOG_FILE_NAME, fname);
 	  cmdBuf[sizeof(cmdBuf) - 1] = '\0';

		const int ret = system(cmdBuf);

  	if (ret == -1)
  		ats_logf(ATSLOG(0), "%s,%d: (%d, %s) Unable to run system command: %s", __FILE__, __LINE__, errno, strerror(errno), cmdBuf);
	  else if(WEXITSTATUS(ret))
		  ats_logf(ATSLOG(0), "%s,%d: ret=%d, for system(%s)", __FILE__, __LINE__, WEXITSTATUS(ret), cmdBuf);
			  
 		m_SecsSinceSent = 0;
  }
}

void AFF_Email::RemoveLogFile()
{
  char cmdBuf[256];
  if (FileExists(MAIL_LOG_FILE_NAME))  // remove the existing LOG file so we don't get an immediate OK
  {
    snprintf(cmdBuf, sizeof(cmdBuf) - 1, "rm %s", MAIL_LOG_FILE_NAME);
    cmdBuf[sizeof(cmdBuf) - 1] = '\0';

    if (system(cmdBuf) == -1)  // remove the mail log file
      ats_logf(&g_log, "AFF_Email::Send_Queued_Messages_If_Any_Otherwise_Sleep() Unable to run system command - %s", cmdBuf);
  }
}

//----------------------------------------------------------
// Start sending a data packet out the port.
//  Store the data packet locally and begin the sending
//  procedure.  If the procedure fails you retry twice.
//  and then stop.
// return true if data being sent
//        false if there is already something being sent.
//
bool AFF_Email::SendData
(
  const char *data,
  short lenData
)
{
  short lenOut = UUEncode((unsigned char *)data, lenData);
  char bufOutgoing[128];

  memcpy(bufOutgoing, GetUUString(), lenOut);
  bufOutgoing[lenOut] = '\0';

  AFFMessage msg;
  if (msg.Add(bufOutgoing, lenOut, mySeqNum))
  {
    g_email_sequence_number = (unsigned char)data[2];
    return m_OutgoingMessages.Add(msg);
  }

  return false;
}

void AFF_Email::Setup()
{
}

// returns true if it finds EX_OK in the mail log file
//
bool AFF_Email::FoundEX_OK()
{
	FILE *fp = fopen (MAIL_LOG_FILE_NAME, "r");
	if (!fp)
		return false;

	bool ret = false;
	char buf[256];
	while (fgets(buf, 255, fp))
	{
		if (strstr(buf, "EX_OK"))
		{
			ret = true;
			break;
		}
	}

	fclose(fp);
	return ret;
}

bool AFF_Email::is_pppd_running()
{
	const int ret = system("ifconfig ppp0 | grep \"inet addr.* P-t-P\" > /dev/null");
	return ((WIFEXITED(ret) && 0 == WEXITSTATUS(ret)));
}
