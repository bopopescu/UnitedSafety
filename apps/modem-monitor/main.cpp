#include <iostream>
#include <sstream>

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <semaphore.h>
#include <termios.h>
#include <sys/inotify.h>
#include <sys/stat.h>

#include "ats-common.h"
#include "atslogger.h"
#include "socket_interface.h"
#include "command_line_parser.h"
#include "ConfigDB.h"
#include <RedStone_IPC.h>
#include <CharBuf.h>
#include <CSelect.h>

REDSTONE_IPC g_RedStone;

#define EVENT_SIZE (sizeof (struct inotify_event))
#define INOTIFY_BUF_LEN (EVENT_SIZE * 16)

// Description: General application log.
//
//	This will mainly log application/OS interactions.
ATSLogger g_log;

// Description: Modem device logging.
//
//	This will mainly log application/modem-device interactions.
ATSLogger g_log_dev;

// Description: Network system logging.
//
//	This will mainly log general network operations.
ATSLogger g_log_net;

static const ats::String g_ppp0_pid_file("/var/run/ppp0.pid");
static const ats::String g_app_name("modem-monitor");

static bool g_bSentInetNotification_1 = false;
static bool g_bSentInetNotification_2 = false;
static bool g_bSentInetNotification_2_2 = false;
static bool g_bSentInetNotification_3 = false;
// <ISCP-360>: Error 1019
static bool isCellModemResponding = true;
static int cellModemResponseCount = 1;
static int modemPowerFailCont = 1;
//</ISCP-360>


bool bEndThread;
class MyData 
{
public:
	bool bService;  // indicates if modem has connection to a tower
	bool bIsConnectPPPRunning; // indicates that connect-ppp is running.
	
	MyData() : bService(false), bIsConnectPPPRunning(false) {}
};

bool exists(const char *fname)
{
	struct stat buffer;   
  return (stat (fname, &buffer) == 0); 
}

#define MAX_CHAR_READ 256
//----------------------------------------------------------------------------------------------------------------------
void* reader_thread(void* p_MyData)
{
	ats_logf(ATSLOG_INFO, "starting reader thread");

	int count = 1;
	CharBuf InputBuf;
	InputBuf.SetSize(1024);
	CSelect comSelect;
	char readBuf[MAX_CHAR_READ];
	char respBuf[MAX_CHAR_READ];
	MyData *pMyData = (MyData *)(p_MyData);
	
//<ISCP-360>
	AFS_Timer t;
	std::string user_data;


	ats_logf(ATSLOG(0), "Try Openning /dev/ttyGPSAT for reading.[%d]..\n",modemPowerFailCont);
	if(++modemPowerFailCont == 4)
	{

			modemPowerFailCont = 5;
			isCellModemResponding = false;
			ats_logf(ATSLOG(0), "Cellular Modem Not Powering UP - Error 1019");
			t.SetTime();
						
			user_data = "1019," + t.GetTimestampWithOS() + ", TGX Cellular Module Error";
			ats_logf(ATSLOG(0), "%s,%d: TGX Cellular Module Error Logged:%s\n",__FILE__, __LINE__, user_data.c_str());
			user_data = ats::to_hex(user_data);
			send_redstone_ud_msg("message-assembler", 0, "msg inet_error msg_priority=9 usr_msg_data=%s\r", user_data.c_str());
		
	}
//</ISCP-360>	
	// open GPSAT for reading

	while (!exists("/dev/ttyGPSAT"))
	{
		
		usleep(100000);
	}
	modemPowerFailCont = 0; // clear the error count

	ats_logf(ATSLOG_INFO, "/dev/ttyGPSAT has been found");
	
	int fdr = open("/dev/ttyGPSAT", O_RDONLY| O_NOCTTY);

	if (fdr < 0)
	{
		ats_logf(ATSLOG_ERROR, "Unable to open GPS AT port: open returned (%d), errno %d", fdr, errno);
	}
	else
		ats_logf(ATSLOG_INFO, "Opened /dev/ttyGPSAT for reading...");
	
	// open ModemAT for writing and set it up to output the service indicator
	int fdw = open("/dev/ttyGPSAT", O_WRONLY | O_NOCTTY);

	if (fdw < 0)
	{
		ats_logf(ATSLOG_ERROR, "Unable to open GPSAT port: open returned (%d), errno %d", fdw, errno);
	}
	else
		ats_logf(ATSLOG_INFO, "Opened /dev/ttyGPSAT for writing...");

	write(fdw, "ATE0\r", 5);
	sleep(1);
	write(fdw, "at^SIND=\"service\",1\r", strlen("at^SIND=\"service\",1\r"));
	sleep(1);

	bool running = true;

	comSelect.SetDelay(0, 100000);
	comSelect.EnableDelay(true);
	comSelect.Add(fdr);
	
	while (running)
	{
		if (bEndThread) // main program wants to force the thread to close.
		{
			running = false;
			continue;
		}

		if ((count % 300) == 0) // request service state every minute at 5 hz
		{
			write(fdw, "at^SIND=\"service\",1\r", strlen("at^SIND=\"service\",1\r"));
			
			ats_logf(ATSLOG(0), "Sending AT^SIND [%d]",cellModemResponseCount);
			count = 1;
//<ISCP-360>
			if(isCellModemResponding == true && (++cellModemResponseCount > 15)) //MAX_CELL_RESPONSE_COUNT
			{
					isCellModemResponding = false;
					ats_logf(ATSLOG(0), "Cellular Modem Not Responding - Error 1019");

						t.SetTime();
						
						user_data = "1019," + t.GetTimestampWithOS() + ", TGX Cellular Module Error";
						ats_logf(ATSLOG(0), "%s,%d: TGX Cellular Module Error Logged:%s\n",__FILE__, __LINE__, user_data.c_str());
						user_data = ats::to_hex(user_data);
						send_redstone_ud_msg("message-assembler", 0, "msg inet_error msg_priority=9 usr_msg_data=%s\r", user_data.c_str());
					
			}
//</ISCP-360>
		}

		comSelect.Select();

		if (comSelect.HasData(fdr))
		{
			readBuf[0] = '\0';
			
			int len = read(fdr, readBuf, MAX_CHAR_READ);
			
			if(len > 0)
			{
				InputBuf.Add(readBuf, len);
				readBuf[len] = '\0';
				
				if (InputBuf.GetLine(respBuf, MAX_CHAR_READ) > 0)
				{
					if (strlen(respBuf) > 5)
					{
						ats_logf(ATSLOG_DEBUG, "Processing:##%s##", respBuf);
						ats_logf(ATSLOG_DEBUG, "bService %d", pMyData->bService);
						if (strstr(respBuf, "+CIEV: service") || strstr(respBuf, "SIND: service") )
						{
								isCellModemResponding = true;//
								cellModemResponseCount = 0;
						}
					}
					if (strstr(respBuf, "+CIEV: service,0") || strstr(respBuf, "SIND: service,1,0")  && pMyData->bService)
					{
						pMyData->bService = false;
						ats::system("killall pppd");
						g_RedStone.ModemState(AMS_NO_CARRIER);
						g_RedStone.pppState(APS_UNCONNECTED);
						ats_logf(ATSLOG(0), "service to tower lost - bService %d", pMyData->bService);
						isCellModemResponding = true;//
						cellModemResponseCount = 0;
					}
					else if ( strstr(respBuf, "+CIEV: service,1") || strstr(respBuf, "SIND: service,1,1") && !pMyData->bService) 
					{
						pMyData->bService = true;
						g_RedStone.ModemState(AMS_NO_PPP);
						count = 1;
						isCellModemResponding = true;//
						cellModemResponseCount = 0;

						if (g_RedStone.pppState() != APS_CONNECTING) // if it is 1 connect-ppp is running but not finished.
						{
							ats_logf(ATSLOG_DEBUG, "Running ppp");
							system("/usr/bin/connect-ppp &");
						}
						else
							ats_logf(ATSLOG_DEBUG, "NOT running ppp");

						ats_logf(ATSLOG(0), "service to tower restored: ModemState %d  pppState %d, bService %d", g_RedStone.ModemState(),g_RedStone.pppState(), pMyData->bService);
					}
					else // not valid response
					{

					}
				}
			}
		}
		else
		{
			usleep(100000); // 1/10th of a second - with another tenth in the ComSelect
			++count;
		}


	}

	ats_logf(ATSLOG_DEBUG, "Reader thread is terminating.");
		
	close(fdr);
	close(fdw);
	
	return 0;
}


//-----------------------------------------------------------------------------
static void turn_on_modem( )
{
	ats::write_file("/dev/set-gpio", "GJ");
}


//-----------------------------------------------------------------------------
static void turn_off_modem( )
{
	// kill the reader thread
	ats_logf(ATSLOG(0), "%s,%d: turn_off_modem called - shutting down reader thread and powering off modem", __FILE__, __LINE__);
	bEndThread = true; // signal the reader thread to shut down the ports
	sleep(1);  // let reader thread die
	bEndThread = false; // reset so thread doesn't immediatly die again
	g_RedStone.ModemState(AMS_NO_COMM); // Indicate that no comms with modem at this time.
	ats::write_file("/dev/set-gpio", "gj");
	ats::system("killall pppd");
	g_RedStone.pppState(APS_POWER_OFF);  // indicates modem is powered off - won't restart reader thread til connect-ppp changes this when udev calls it after bringing up the ports
	sleep(5); // let the modem shut down and udev wipe out the ports.
}


//-----------------------------------------------------------------------------
static bool is_pppd_running(MyData &p_md)
{
	ats::String pid;

	if(ats::get_file_line(pid, g_ppp0_pid_file, 1) > 0)
	{
		ats_logf(ATSLOG(0), "%s,%d: pppRunning set to false:Unable to find pid in %s", __FILE__, __LINE__, g_ppp0_pid_file.c_str());
		p_md.bIsConnectPPPRunning =	false;
		return false;
	}

	ats::String proc;
	ats_sprintf(&proc, "/proc/%s/comm", pid.c_str());

	ats::String name;
	if (ats::get_file_line(name, proc, 1) > 0)
	{
		ats_logf(ATSLOG_DEBUG, "in ppp is not running - %s cannot be read!.", proc.c_str());
		p_md.bIsConnectPPPRunning =	(!strcmp("pppd", name.c_str()));
	}
	else
	{
		if (strcmp(name.c_str(), "pppd"))
		{
			ats_logf(ATSLOG_DEBUG, "ppp is not running - %s contains %s (should be 'pppd').", proc.c_str(), name.c_str());
			p_md.bIsConnectPPPRunning =	false;
		}
		else
			p_md.bIsConnectPPPRunning =	true;
	}
	if (!p_md.bIsConnectPPPRunning)	
		ats_logf(ATSLOG_DEBUG, "ppp is not running!.");
	return  p_md.bIsConnectPPPRunning;
}


//-----------------------------------------------------------------------------
ats::String setup_modem_serial_port(const ats::String& p_port)
{
	const int fd = open(p_port.c_str(), O_RDWR);

	if(fd < 0)
	{
		return strerror(errno);
	}

	struct termios t;

	if(-1 == tcgetattr(fd, &t))
	{
		close(fd);
		return strerror(errno);
	}

	t.c_iflag &= ~BRKINT;
	t.c_iflag &= ~ICRNL;
	t.c_iflag &= ~IMAXBEL;
	t.c_oflag &= ~OPOST;
	t.c_lflag &= ~ISIG;
	t.c_lflag &= ~ICANON;
	t.c_lflag &= ~IEXTEN;
	t.c_lflag &= ~ECHO;

	if(-1 == tcsetattr(fd, 0, &t))
	{
		close(fd);
		return strerror(errno);
	}

	close(fd);
	return ats::g_empty;
}


//-----------------------------------------------------------------------------
static void term(int signum)
{
	ats::system("killall pppd");
	ats_logf(ATSLOG(0), "%s,%d: Catch SIGTERM signal, stop pppd and exit......", __FILE__, __LINE__);

	//executing default action.
	signal(SIGTERM, SIG_DFL);
	raise(SIGTERM);
}


//-----------------------------------------------------------------------------
int main(int argc, char *argv[])
{

	ATSLogger::set_global_logger(&g_log);
	g_log.open_testdata(g_app_name);
	AWARE_MODEM_STATES lastModemState = AMS_UNSET;
	short count = 0;
	short count2 = 0;
	pthread_t m_reader_thread;
	
	AFS_Timer t;
	std::string user_data;

	MyData md;
	db_monitor::ConfigDB db;

	g_log.set_level(db.GetInt("RedStone", "LogLevel", 0));

	ats_logf(ATSLOG(0), "-------------- %s", (g_app_name + " started").c_str());
	ats_logf(ATSLOG(0), "On startup: ModemState %d  pppState %d", g_RedStone.ModemState(),g_RedStone.pppState());

	{
		struct sigaction action;
		memset(&action, 0, sizeof(struct sigaction));
		action.sa_handler = term;
		sigaction(SIGTERM, &action, NULL);
	}

	// create the reader thread to read the port and load the buffer.
	pthread_create(&(m_reader_thread),	(pthread_attr_t*)0, reader_thread, &md);

	wait_for_app_ready("i2c-gpio-monitor");
	wait_for_app_ready("telit-monitor");

	bool pPPPisSet = false;
	bEndThread = false;
	
	for(;;)
	{
		if (md.bService)
		{
			if (g_RedStone.pppState() == APS_UNCONNECTED) // might happen at startup
			{
				ats_logf(ATSLOG_ERROR, "pppd has been lost (1) - restarting connection. pppState %d", g_RedStone.pppState());
				g_RedStone.pppState(APS_UNCONNECTED);
				g_RedStone.ModemState(AMS_NO_PPP);
				system("/usr/bin/connect-ppp &");
			}
			else if (g_RedStone.pppState() == APS_CONNECTING) // connect-ppp is running - do nothing
			{
			}
			else if (g_RedStone.pppState() == APS_CONNECTED) // check that ppp is still running
			{
				if (!is_pppd_running(md) )   // if ppp0 is taken down this will eventually recognize that
				{
					ats_logf(ATSLOG_ERROR, "pppd has been lost (2) - restarting connection.pppState %d", g_RedStone.pppState() );
					g_RedStone.pppState(APS_UNCONNECTED);
					g_RedStone.ModemState(AMS_NO_PPP);
					system("/usr/bin/connect-ppp &");	// this should drive pppState to 1, then 2
				}
				else if (g_RedStone.pppState() > APS_CONNECTING )
					g_RedStone.ModemState(AMS_PPP_ESTABLISHED);
			}
		}
		else // no service - 5 minutes without service and we force the modem to repower
		{
			if (++count >=60)
			{
				ats_logf(ATSLOG(0), "No connection for 5 minutes. Repowering modem.");
				turn_off_modem();
				md.bService = false;
				pPPPisSet = false;
				turn_on_modem();
						
				ats_logf(ATSLOG_ERROR, "Modem is back up now - restarting reader thread.");
				// the modem is back up now - so recreate the reader thread to read the port and load the buffer.
				pthread_create(&(m_reader_thread),	(pthread_attr_t*)0, reader_thread, &md);
				count = 0;
			}
		}
		
		if (lastModemState != g_RedStone.ModemState())
		{
			ats_logf(ATSLOG_DEBUG, "State change: ModemState %d  pppState %d", g_RedStone.ModemState(),g_RedStone.pppState());
			lastModemState = g_RedStone.ModemState();
			switch (lastModemState)
			{
				case AMS_NO_COMM:  // no comms with modem at all
					if(isCellModemResponding == false)
					{
					send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink kick name=cell led=cell script=\"0,1000000\"\r");
					send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink kick name=cellr led=cell.r script=\"1,1000000\"\r");
					}
					//ISCP-163
					if(!g_bSentInetNotification_1)
					{					

						//.t.SetTime();
						g_bSentInetNotification_1 = true;
						//.user_data = "991," + t.GetTimestampWithOS() + ", TGX Cellular Module Error";
						ats_logf(ATSLOG(0), "%s,%d: No Communication with TGX Cellular:\n",__FILE__, __LINE__);
						//.user_data = ats::to_hex(user_data);
						//.send_redstone_ud_msg("message-assembler", 0, "msg inet_error msg_priority=9 usr_msg_data=%s\r", user_data.c_str());
					}
					
					break;
				case AMS_NO_SIM:  // SIM undetected
     				//ISCP-248				
					send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink kick name=cell led=cell script=\"0,1000000\"\r");
					send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink kick name=cellr led=cell.r script=\"1,1000000\"\r");
						g_bSentInetNotification_2 = true;

					break;
				case AMS_NO_CARRIER:  // SIM detected - no carrier (wrong apn?)
				    //ISCP-248
					send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink kick name=cell led=cell script=\"0,1000000\"\r");
					send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink kick name=cellr led=cell.r script=\"1,1000000\"\r");
					ats_logf(ATSLOG(0), "%s,%d: No Cellular Carrier:\n",__FILE__, __LINE__);
					break;
				case AMS_NO_PPP:  // service - no ppp - missing antenna?
					send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink kick name=cell led=cell script=\"0,1000000\"\r");
					send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink kick name=cellr led=cell.r script=\"1,1000000;0,200000;1,200000;0,200000;1,200000;0,200000\"\r");
					//ISCP-163
					if(!g_bSentInetNotification_3)
					{					
						//.t.SetTime();						
						g_bSentInetNotification_3 = true;
						//.user_data = "1008," + t.GetTimestampWithOS() + ", TGX Cellular Communication Error";
						ats_logf(ATSLOG(0), "%s,%d: TGX Cellular Communication NO PPP:\n",__FILE__, __LINE__);
						//.user_data = ats::to_hex(user_data);
						//.send_redstone_ud_msg("message-assembler", 0, "msg inet_error msg_priority=9 usr_msg_data=%s\r", user_data.c_str());
					}
					break;
				case AMS_PPP_ESTABLISHED:  // ppp established
					g_bSentInetNotification_1 = false; // reset flags to generate cellular errors again after recovery
					g_bSentInetNotification_2 = false;
					g_bSentInetNotification_2_2 = false;
					g_bSentInetNotification_3 = false;
					count2 = 0;
					
					send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink kick name=cell led=cell script=\"1,1000000\"\r");
					send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink kick name=cellr led=cell.r script=\"0,1000000\"\r");
					break;
				case AMS_UNSET:
					break;
			}			
		}

		if(g_bSentInetNotification_2 && !g_bSentInetNotification_2_2) // for SIM Card Error, this needs to be sent with some delay
		{
			if(count2++ > 10)
			{
			//ISCP-163
						t.SetTime();						
						g_bSentInetNotification_2 = false;
						g_bSentInetNotification_2_2 = true;
						user_data = "982," + t.GetTimestampWithOS() + ", SIM Card Error";
						ats_logf(ATSLOG(0), "%s,%d: TGX SIM Card Error Logged:%s\n",__FILE__, __LINE__, user_data.c_str());
						user_data = ats::to_hex(user_data);
						send_redstone_ud_msg("message-assembler", 0, "msg inet_error msg_priority=9 usr_msg_data=%s\r", user_data.c_str());
						count2 = 0;
			}
		

		}
		
		sleep(5);
	}

	pthread_cancel(m_reader_thread);
	return 0;
}
