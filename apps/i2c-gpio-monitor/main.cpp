#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/ioctl.h>

#include "linux/i2c-dev.h"
#include "db-monitor.h"
#include "ats-common.h"
#include "ats-string.h"
#include "atslogger.h"
#include "socket_interface.h"
#include "command_line_parser.h"
#include "RedStone_IPC.h"
#include "GPIOPin.h"
#include "MyData.h"
#include "ConfigDB.h"
#include "BlinkCommand.h"
#include "IRQCommand.h"
#include "LEDCommand.h"
#include "InputsCommand.h"
#include "GPIOCommand.h"
#include "GPIOManager.h"
#include "StatusCommand.h"
#include "TestCommand.h"
#include "ShutdownCommand.h"
#include "CThreadWait.h"

REDSTONE_IPC* g_shmem = 0;

#define MAX_GPIO_INPUTS (6)
static ATSLogger g_log;
static const ats::String g_app_name("i2c-gpio-monitor");

static int g_dbg = 0;
typedef std::map<int, const ats::String> messageTypeMap;
static void* inputLED_blinker();

CThreadWait g_TWaitForInputChange;

class GPIOINPUT
{
public:
	int ActiveHigh;
	int ONMESSAGETYPE;
	int OFFMESSAGETYPE;
	int ONPRIORITY;
	int OFFPRIORITY;
	double Debounce;
	bool bChangingFlag;
	AFS_Timer ChangingTimer;
	
	GPIOINPUT() : ActiveHigh(0), ONMESSAGETYPE(10), OFFMESSAGETYPE(10), ONPRIORITY(20), OFFPRIORITY(20),  Debounce(0.25), bChangingFlag(false) {}
};

class GPIOINPUTCONFIG
{
public:
	GPIOINPUT m_Input[MAX_GPIO_INPUTS];
	int mask;
	
	GPIOINPUTCONFIG() : mask(0x63){}
};

GPIOINPUTCONFIG g_inputConfig;
messageTypeMap g_messageTypeMap;

static unsigned char last_val = 99;  // used in both irq_task and debounce_task

static int get_messageTypeMap_from_db();

void set_led(MyData& p_md, const ats::String* p_owner, int p_expander, int p_byte, int p_pin, int p_val)
{
	write_expander(p_md, p_owner, p_expander, p_byte, p_pin, p_val);
	write_expander_hw(p_md, p_expander, p_byte);
}

MyData* g_md = 0;

static void* on_max_clients(ServerData* p_sd, int p_fd)
{
	ats_logf(ATSLOG_DEBUG, "%s: FATAL: Max clients (%d) reached. p_fd=%d", __FUNCTION__, p_sd->m_max_clients, p_fd);
	send_cmd(p_fd, MSG_NOSIGNAL, "fatal: max clients (%d) reached\n\r", p_sd->m_max_clients);
	exit(2);
	return 0;
}

// ************************************************************************
// Description: Command server.
//
// Parameters:  p - pointer to ClientData
// Returns:     NULL pointer
// ************************************************************************
static void* client_command_server(void* p)
{
	ClientData* cd = (ClientData*)p;
	MyData& md = *((MyData*)(cd->m_data->m_hook));

	bool command_too_long = false;

	const size_t max_command_length = 1024;
	char cmdline_buf[max_command_length + 1];
	char* cmdline = cmdline_buf;

	ClientDataCache cdc;
	init_ClientDataCache(&cdc);

	CommandBuffer cb;
	init_CommandBuffer(&cb);

	for(;;)
	{
		char ebuf[256];
		const int c = client_getc_cached(cd, ebuf, sizeof(ebuf), &cdc);

		if(c < 0)
		{
			if(c != -ENODATA) ats_logf(ATSLOG_DEBUG, "%s,%d: %s", __FILE__, __LINE__, ebuf);
			break;
		}

		if((c != '\r') && (c != '\n'))
		{
			if(size_t(cmdline - cmdline_buf) >= max_command_length) command_too_long = true;
			else *(cmdline++) = c;

			continue;
		}

		if(command_too_long)
		{
			ats_logf(ATSLOG_DEBUG, "%s,%d: command is too long", __FILE__, __LINE__);
			cmdline = cmdline_buf;
			command_too_long = false;
			ClientData_send_cmd(cd, MSG_NOSIGNAL, "%s,%d: command is too long\r", __FILE__, __LINE__);
			continue;
		}

		{
			const char* err = gen_arg_list(cmdline_buf, cmdline - cmdline_buf, &cb);
			cmdline = cmdline_buf;

			if(err)
			{
				ats_logf(ATSLOG_DEBUG, "%s,%d: gen_arg_list failed (%s)", __FILE__, __LINE__, err);
				ClientData_send_cmd(cd, MSG_NOSIGNAL, "%s,%d: gen_arg_list failed: %s\r", __FILE__, __LINE__, err);
				continue;
			}

		}

		if(cb.m_argc <= 0)
		{
			continue;
		}

		const ats::String cmd(cb.m_argv[0]);
		CommandMap::const_iterator i = md.m_cmd.find(cmd);

		if(i != md.m_cmd.end())
		{
			ClientData_send_cmd(cd, MSG_NOSIGNAL, "%s: ", cmd.c_str());
			Command& c = *(i->second);
			c.fn(md, *cd, cb);
			ClientData_send_cmd(cd, MSG_NOSIGNAL, "\r");
		}
		else if("get-gpio-status" == cmd)
		{
			ClientData_send_cmd(cd, MSG_NOSIGNAL, "%s: 0x%02X\n\r", cmd.c_str(), g_shmem->GPIO());
		}
		else if(("debug" == cmd) || ("dbg" == cmd))
		{

			if(cb.m_argc >= 2)
			{
				g_dbg = strtol(cb.m_argv[1], 0, 0);
				ClientData_send_cmd(cd, MSG_NOSIGNAL, "%s: debug=%d\n\r", cmd.c_str(), g_dbg);
			}
			else
			{
				ClientData_send_cmd(cd, MSG_NOSIGNAL, "%s: debug=%d\n\r", cmd.c_str(), g_dbg);
			}

		}
		else if(("uds" == cmd))
		{
		  ats_logf(ATSLOG_INFO, "received uds message: %s, %s", cb.m_argv[0], cb.m_argv[1] );

		  if(cb.m_argc < 2)
		  {
			ClientData_send_cmd(cd, MSG_NOSIGNAL, "error\n\tusage: <app name> <socket name>\n\r");
			return 0;
		  }

		  const ats::String socket_name(cb.m_argv[1]);
		  md.m_udsSet.insert(socket_name);

		}
		else
		{
			ClientData_send_cmd(cd, MSG_NOSIGNAL, "Invalid command \"%s\"\n\r", cmd.c_str());
		}

	}

	return 0;
}

static void* on_irq(void* p)
{
	MyData& md = *((MyData*)p);

	for(;;)
	{
		sem_wait(&md.m_irq_sem);
		ats::StringList run;
		md.lock_irq();
		run.reserve(md.m_irq_task.size());
		ats::StringMap::const_iterator i = md.m_irq_task.begin();

		while(i != md.m_irq_task.end())
		{
			run.push_back(i->second);
			++i;
		}

		md.unlock_irq();

		{
			ats::StringList::const_iterator i = run.begin();

			while(i != run.end())
			{
				ats::system(*i);
				++i;
			}

		}

	}

	return 0;
}

static const char* g_irq_fname = "/dev/i2c-gpio-expander-irq";
static FILE* g_irq_file = 0;
static bool g_is_2500 = false;
static bool g_is_3000 = false;
static bool g_is_5000 = false;

//-------------------------------------------------------------------------------------------------------------------------------
// task to check for an input staying changed for the debounce time.  If it has then a message is generated.
//
static void* debounce_task(void* p)
{
	MyData& md = *((MyData*)p);
	ats_logf(ATSLOG_INFO, "Running debounce task");

	if(get_messageTypeMap_from_db()<0)
		ats_logf(ATSLOG_DEBUG, "Fail to get Message Type from messages.db");

	bool useDefaultMessageType = false;
	if( !g_messageTypeMap.size())
	{
		useDefaultMessageType = true;
	}

	for(;;)
	{
		bool bStillDebouncing = false;
		for ( short i = 0; i < MAX_GPIO_INPUTS; i++)  // check if currently debouncing
		{
			md.lock();

		  if (!g_inputConfig.m_Input[i].bChangingFlag)
		  {
				md.unlock();
		  	continue;
		  }

			if (g_inputConfig.m_Input[i].ChangingTimer.DiffTimeMS() > g_inputConfig.m_Input[i].Debounce * 1000)
			{
				char on = (last_val>>(i))&0x01;  // is the input switching to on or off?
				g_inputConfig.m_Input[i].bChangingFlag = false;
				g_shmem->GPIO(last_val);

				if (on)
				{
					const ats::String& typeStr = (useDefaultMessageType)?"sensor":g_messageTypeMap[g_inputConfig.m_Input[i].OFFMESSAGETYPE];
					const int pri = g_inputConfig.m_Input[i].OFFPRIORITY;
					send_redstone_ud_msg("message-assembler", 0, "msg %s msg_priority=%d\r", typeStr.c_str(), pri);
					ats_logf(ATSLOG_INFO, "%s,%d: input %d is now OFF", __FILE__, __LINE__, i);
				}
				else
				{
					const ats::String& typeStr = (useDefaultMessageType)?"sensor":g_messageTypeMap[g_inputConfig.m_Input[i].ONMESSAGETYPE];
					const int pri = g_inputConfig.m_Input[i].ONPRIORITY;
					ats_logf(ATSLOG_INFO, "%s,%d: input %d is now ON", __FILE__, __LINE__, i);

					send_redstone_ud_msg("message-assembler", 0, "msg %s msg_priority=%d\r", typeStr.c_str(), pri);
				}

				inputLED_blinker();				// now light the LED for the input
				
				{
				  std::set<std::string>::const_iterator it = md.m_udsSet.begin();
				  for(; it != md.m_udsSet.end(); ++it)
				  {
						send_trulink_ud_msg((*it).c_str(), 0, "sensor\r");
						ats_logf(ATSLOG_DEBUG, "%s,%d: send sensor message to %s", __FILE__, __LINE__, (*it).c_str());
				  }
				}
				send_redstone_ud_msg("SER_GPS", 0, "input 1\r");
			}
			bStillDebouncing = true;
			md.unlock();
		}
		if (bStillDebouncing)
			usleep(50 * 1000);  // 1/20th of a second.
		else
		{
			g_TWaitForInputChange.Wait();
		}
//		usleep(100 * 1000);  // 1/10th of a second.
		
	}

	return 0;
}
// ------------------------------------------------------------------------------------------------------------------------
// add_to_blink - appends 'ledname,ledname.r' to the appropriate string On/Off depending on inputs in and bit abit
//
void add_to_blink(const char in, const char abit, const char *ledname,
								 char *strBlinkOn, char *comma)
{
	char name[16];
	sprintf(name, "%s,%s.r", ledname, ledname);
	
	if (in & abit)
	{
		strcat(strBlinkOn, comma);
		strcat(strBlinkOn, name);
		comma[0] = ',';
	}
}

// ------------------------------------------------------------------------------------------------------------------------
void build_input_blink_script(char *strBlinkOn, int inputs)
{
	char in = (char)inputs;
	in = ~in & 0x3f;  // invert the bits and get the last 6
	
	strBlinkOn[0] = '\0';

  if (in == 0)  // no inputs so remove inputs script 
  {
  	return;
  }

	sprintf(strBlinkOn,  "blink add name=TheInputs script=\"1,100000\" priority=9 led=");
	char comma[8];
	comma[0] = comma[1] = '\0';
	
	add_to_blink(in, 0x01, "cell", strBlinkOn, comma);
	add_to_blink(in, 0x02, "gps", strBlinkOn, comma);
	add_to_blink(in, 0x04, "sat", strBlinkOn, comma);
	add_to_blink(in, 0x08, "wifi", strBlinkOn, comma);
	add_to_blink(in, 0x10, "zigbee", strBlinkOn, comma);
	add_to_blink(in, 0x20, "inp6", strBlinkOn, comma);

	strcat(strBlinkOn, "\r\0");
}

//-------------------------------------------------------------------------------------------------------------------------------
// SendLocalCommand - send the input string to the local socket command controller without using send_redstone_ud_msg
//
void SendLocalCommand(const char * strCmd, Command *cmd)
{
	CommandBuffer cb;
	init_CommandBuffer(&cb);
	ClientData cd;
	cd.m_sockfd = 0;
	const char* err;

	if ( (err = gen_arg_list(strCmd, strlen(strCmd), &cb)))
	{
		ats_logf(ATSLOG_DEBUG, "SendLocalCommand gen_arg_list failed (%s). Cmd line was", err);
		ats_logf(ATSLOG_DEBUG, strCmd);
		return;
	}
 	cmd->fn(*g_md, cd, cb);
}



//-------------------------------------------------------------------------------------------------------------------------------
static void* inputLED_blinker()
{
	char strBlinkOn[256];
	int inputs;

	// get the BlinkCommand structure to send the messages to.
	CommandMap::const_iterator i = g_md->m_cmd.find("blink");

	Command *blinkCmd =  NULL;
	if(i != g_md->m_cmd.end())
		blinkCmd = (Command *)(i->second);
	else
		ats_logf(ATSLOG_DEBUG, "------ BLINK Command not found");

	inputs = g_shmem->GPIO();
	    // trigger the LEDS for the Inputs
  	  // you have to turn all the current leds off with the input.mask script.  Then you build an inputs.on and an inputs.off script so that
  	  // changes to the inputs will turn off a light that was on
 	build_input_blink_script(strBlinkOn, inputs);

	g_md->remove_blinker("TheInputs");

	if (strlen(strBlinkOn))
	{
		ats_logf(ATSLOG_INFO, "inputLED_blinker - on = ##%s##", strBlinkOn);
		send_redstone_ud_msg("i2c-gpio-monitor", 0, strBlinkOn);  // FIXME:: this is a workaround for a bug that wipes out all scripts if we send via localCommand.
	}
	return 0;
}



//-------------------------------------------------------------------------------------------------------------------------------
static int get_messageTypeMap_from_db()
{
	const ats::String db_name("messages_db");
	const ats::String db_file("/mnt/update/database/messages.db");
	db_monitor::DBMonitorContext db(db_name, db_file);
	{
		g_messageTypeMap.clear();
		const ats::String query("SELECT name,cantel_id FROM message_types;");
		const ats::String& err = db.query(query);

		if(!err.empty())
		{
			ats_logf(ATSLOG_DEBUG, "%s,%d: %s", __FILE__, __LINE__, err.c_str());
			return -1;
		}

		if(db.Table().size() > 0)
		{

			for(size_t i = 0; i < db.Table().size(); ++i)
			{
				db_monitor::ResultRow& row = db.Table()[i];

				if(row.size() < 2) continue;

				const ats::String& str = row[0];
				int id = strtol(row[1].c_str(),0,0);
				g_messageTypeMap.insert(std::pair<int, const ats::String>(id,str));
			}

			return 1;
		}

	}

	return -1;
}

//-------------------------------------------------------------------------------------------------------------------------------
// irq_task - reads the value of the current I/Os
//            result is inverted (1 is off 0 is on)
//
static void* irq_task(void* p)
{
	MyData& md = *((MyData*)p);

	{
		int ival;
		unsigned char val;

		last_val = 0xC0;
		last_val |= ~g_inputConfig.mask;  // force unwatched bits to off
		g_shmem->GPIO(last_val);
		bool startup=true;  // need this to catch inputs that are on at startup.

		for(;;)
		{
			char c;
			if (!startup)
			{
				const size_t nread = fread(&c, 1, 1, g_irq_file);

				if(!nread)
				{
					ats_logf(ATSLOG_DEBUG, "%s: fread failed with %s", __FUNCTION__, feof(g_irq_file) ? "EOF" : (ferror(g_irq_file) ? "ERROR" : "UNKNOWN"));
					exit(1);
				}
			}
			if('i' == c || startup)
			{
				read_expander_hw(md, EXP_2, 0, ival);
				val = (unsigned char)ival;


				for (short i = 0; i < MAX_GPIO_INPUTS; i++)  // switch bits that are active low
				{
				  if (g_inputConfig.m_Input[i].ActiveHigh == 0)  // if an input is active low we toggle the bit
				    val = ( val ^ (1L << i) );
				}

				val |= 0xC0;  // force upper 2 bits to off
				val |= ~g_inputConfig.mask;  // force unwatched bits to off
				
				startup = false;

				if(val != last_val) // one of the bits has changed.
				{
					ats_logf(ATSLOG_INFO, "Input changed from %d to %d", last_val, val);
					unsigned char diff = (val ^ last_val); // find the bits that changed
					
					for ( short i = 0; i < MAX_GPIO_INPUTS; i++)  // check if currently debouncing
					{
						if ( (diff & (1 << i)) != 0)
						{
						  md.lock();
							if (g_inputConfig.m_Input[i].bChangingFlag)
								g_inputConfig.m_Input[i].bChangingFlag = false;
							else
							{
								g_inputConfig.m_Input[i].bChangingFlag = true;
								g_inputConfig.m_Input[i].ChangingTimer.SetTime();  // start the debounce clock.
								g_TWaitForInputChange.Signal();  // signal that we have an input change to track

							}
						  md.unlock();
						}
					}
					if (diff)
					{
						g_TWaitForInputChange.Signal();  // signal that we have an input change to track
					}

					last_val = val;
					sem_post(&md.m_irq_sem);
				}
			}
		}
	}

	return 0;
}

//-------------------------------------------------------------------------------------------------------------------------------
// ATS FIXME: Bug #612: support function for turning off LEDs on graceful program termination.
static int h_write_expander_hw(MyData& p_md, int p_expander, int p_byte, int p_val)
{
	const int exp_addr = p_md.m_exp_addr[p_expander];

	if(ioctl(p_md.m_fd, I2C_SLAVE, exp_addr))
	{
		ats_logf(ATSLOG_ERROR, "%s,%d: ERR (%d): %s", __FILE__, __LINE__, errno, strerror(errno));
		return 1;
	}

	i2c_smbus_write_byte_data(p_md.m_fd, p_byte, p_val);

	return 0;
}

//-------------------------------------------------------------------------------------------------------------------------------
// ATS FIXME: Bug #612: Hack to turn off LEDs that are kept on by hardware capacitance.
//
// 1. Lock LEDs
// 2. Turn off LEDs
// 3. Close I2C file to prevent LEDs from being turned on again
// 4. Unlock LEDs
// 5. Terminate and sleep for 2 seconds to prevent from turning on LEDs again before system shutdown.
static void term(int)
{
	g_md->lock();
	h_write_expander_hw(*g_md, EXP_1, 2, 0);
	h_write_expander_hw(*g_md, EXP_1, 3, 0);
	h_write_expander_hw(*g_md, EXP_1, 6, 0);
	h_write_expander_hw(*g_md, EXP_1, 7, 0);
	close(g_md->m_fd);
	g_md->m_fd = -1;
	g_md->unlock();
	execl("/bin/sleep", "/bin/sleep", "2", NULL);
	exit(1);
}

//-------------------------------------------------------------------------------------------------
// OpenGlobalLogger - Use this function to open log (defined as ATSLogger log) in 
//                    the 'main.cpp' source file (usually as g_log in global space).  
//										Call this function immediately in main as 
//				OpenGlobalLogger(log, argv[0]) 
//                    to pull the current logging level from 
//				db-config <process name> LogLevel
//										You can then use ats_logf(ATSLOG(x)... to log data.  Only values of x less
//                    than the value of LogLevel will be logged to the the log file.
//
void OpenGlobalLogger(ATSLogger &log, char * command_name)
{
	db_monitor::ConfigDB db;
	int LogLevel = db.GetInt(command_name, "LogLevel", 0);

	log.open_testdata(command_name);
	log.set_global_logger(&log);
	log.set_level(LogLevel);
}
//------------------------------------------------------------------------------------------------------------------------
// Description: Starts the I2C GPIO monitor.
//
// Procedure:
//	1. Parse parameters
//	2. Connect to the I2C devices
//	3. Initialize the I2C devices
//	4. Connect to and enable the I2C interrupts
//	5. Apply test-mode or normal operation settings
//	6. Start command server thread
//	7. Wait forever for commands
int main(int argc, char *argv[])
{
	AFS_Timer t;
	std::string user_data;
	
	OpenGlobalLogger(g_log, argv[0]);

	ats_logf(ATSLOG_DEBUG, "-----------------I2C-GPIO-Monitor started");

//	ats::StringMap hw_config;
//	ats::get_hw_config(hw_config);
	g_is_2500 = ats::is_trulink_model("2500");
	g_is_3000 = ats::is_trulink_model("3000");
	g_is_5000 = ats::is_trulink_model("5000");

	if(!(g_is_2500 || g_is_3000 || g_is_5000))
	{
		ats_logf(ATSLOG_ERROR, "ERR: Device is not a recognized TRULink hardware revision. Device=\"%s\" is not supported.", (ats::get_trulink_model()).c_str());
		ats_logf(ATSLOG_ERROR, "     Defaulting to TruLink 5000");
		g_is_5000 = true;
	}
	else
		ats_logf(ATSLOG_ERROR, "Device=\"%s\" found.", (ats::get_trulink_model()).c_str());
	

	MyData& md = *(g_md = new MyData());
	ats::StringMap &config = md.m_config;
	config.set("gpio-expander-address", "0x20");
	config.set("gpio-expander-address2", "0x21");
	config.set("gpio-dev-address", "/dev/i2c-0");
	config.set("user", "applet");
	config.from_args(argc - 1, argv + 1);

	{  // scoping is required to force closure of db-config database
		db_monitor::ConfigDB db;
		g_dbg = db.GetInt(g_app_name, "LogLevel", 0);  // 0:None 1:Error 2:Debug 3:Info
		g_log.set_level(g_dbg);
		ats::write_file("/dev/set-gpio", "C");  // turn on the serial port power (
	
		if(!ats::testmode())
		{
			// get the Input Active Hi/Low status from db-config
			//
			// ATS FIXME:
			// DRH - include this code so that inputs are set to active HIGH by default.  We will be using the inputs later
			//       but we want the defaults to be set now.
			for(int i = 1; i <= MAX_GPIO_INPUTS; i++)
			{
				char inputName[32];
				sprintf(inputName, "input%d", i);
				g_inputConfig.m_Input[i-1].ActiveHigh = db.GetInt("i2c-gpio-monitor", inputName, 1);  // Set default to active high (value 1) in database
				sprintf(inputName, "inputOnType%d", i);
				g_inputConfig.m_Input[i-1].ONMESSAGETYPE = db.GetInt("i2c-gpio-monitor", inputName, 10);//sensor is default message type
				sprintf(inputName, "inputOffType%d", i);
				g_inputConfig.m_Input[i-1].OFFMESSAGETYPE = db.GetInt("i2c-gpio-monitor", inputName, 10);
				sprintf(inputName, "inputOnPriority%d", i);
				g_inputConfig.m_Input[i-1].ONPRIORITY = db.GetInt("MSGPriority", "sensor", 20);//cell priority is default value.
				sprintf(inputName, "inputOffPriority%d", i);
				g_inputConfig.m_Input[i-1].OFFPRIORITY = db.GetInt("MSGPriority", "sensor", 20);
				sprintf(inputName, "inputDebounce%d", i);
				g_inputConfig.m_Input[i-1].Debounce = db.GetDouble("i2c-gpio-monitor", inputName, 0.25);  // Set default to .25 seconds
			}

			g_inputConfig.mask = db.GetInt("i2c-gpio-monitor", "inputMask", 0x3F);//0x3F is default mask.
		}
	}

	ats_logf(ATSLOG_INFO, "Connecting to I2C...");
	g_shmem = new REDSTONE_IPC;
	g_shmem->GPIO(0);

	{
		const ats::String gpio_dev_address(config.get("gpio-dev-address"));
		int& fd = md.m_fd = open(gpio_dev_address.c_str(), O_RDWR);

		if(fd < 0)
		{
			ats_logf(ATSLOG_ERROR, "ERR: Failed to open %s: (%d) %s", gpio_dev_address.c_str(), errno, strerror(errno));
			return 1;
		}

		if(ats::su(config.get("user")))
		{
			ats_logf(ATSLOG_ERROR, "%s,%d: Could not become user \"%s\": ERR (%d): %s", __FILE__, __LINE__, config.get("user").c_str(), errno, strerror(errno));
			return 1;
		}

		if(ioctl(fd, I2C_SLAVE, (md.m_exp_addr[EXP_1] = config.get_int("gpio-expander-address"))))
		{
			ats_logf(ATSLOG_ERROR, "%s,%d: ERR (%d): %s", __FILE__, __LINE__, errno, strerror(errno));
			return 1;
		}

		if(ioctl(fd, I2C_SLAVE, (md.m_exp_addr[EXP_2] = config.get_int("gpio-expander-address2"))))
		{
			ats_logf(ATSLOG_ERROR, "%s,%d: ERR (%d): %s", __FILE__, __LINE__, errno, strerror(errno));
			return 1;
		}

		ats_logf(ATSLOG_INFO, "Connected to I2C");

		// Configure all pins as input (except for the floating ones).
		write_expander(md, 0, EXP_2, 6, 0, 1);
		write_expander(md, 0, EXP_2, 6, 1, 1);
		write_expander(md, 0, EXP_2, 6, 2, 1);
		write_expander(md, 0, EXP_2, 6, 3, 1);
		write_expander(md, 0, EXP_2, 6, 4, 1);
		write_expander(md, 0, EXP_2, 6, 5, 1);
		write_expander(md, 0, EXP_2, 5, 0, 1);
		write_expander(md, 0, EXP_2, 5, 1, 1);
		write_expander(md, 0, EXP_2, 5, 2, 1);
		write_expander(md, 0, EXP_2, 5, 3, 1);
		write_expander(md, 0, EXP_2, 5, 4, 1);
		write_expander(md, 0, EXP_2, 5, 5, 1);
		// Apply the pin configuration by updating the I2C-GPIO-Expander hardware.
		//
		// XXX: The default pin value is zero. This means "write_expander_hw" will write zeros
		//	for all pins except for pins set to 1 via the "write_expander" function.
		write_expander_hw(md, EXP_1, 2);
		write_expander_hw(md, EXP_1, 3);
		write_expander_hw(md, EXP_1, 6);
		write_expander_hw(md, EXP_1, 7);
		write_expander_hw(md, EXP_2, 2);
		write_expander_hw(md, EXP_2, 3);
		write_expander_hw(md, EXP_2, 5);
		write_expander_hw(md, EXP_2, 6);
		write_expander_hw(md, EXP_2, 7);

		if(!(g_irq_file = fopen(g_irq_fname, "r")))
		{
			ats_logf(ATSLOG_ERROR, "Failed to open IRQ monitor file %s", g_irq_fname);
			//ISCP-163
			t.SetTime();
			user_data = "1017," + t.GetTimestampWithOS() + ", TGX i2C Error";
			ats_logf(ATSLOG_DEBUG, "TGX I2C Error:%s\n",user_data.c_str());
			user_data = ats::to_hex(user_data);
			send_redstone_ud_msg("message-assembler", 0, "msg inet_error msg_priority=9 usr_msg_data=%s\r", user_data.c_str());
			return 1;
		}

	}

	// ATS FIXME: Bug #612: Setup hack function to turn off LEDs on graceful program termination.
	if(!ats::testmode())
	{
		struct sigaction action;
		memset(&action, 0, sizeof(struct sigaction));
		action.sa_handler = term;
		sigaction(SIGTERM, &action, NULL);
	}

	init_post_command();

	if(ats::testmode())
	{
		md.m_cmd.insert(CommandPair("blink", new BlinkCommand()));
		md.m_cmd.insert(CommandPair("irq", new IRQCommand()));
		md.m_cmd.insert(CommandPair("led", new LEDCommand()));
		md.m_cmd.insert(CommandPair("inputs", new InputsCommand()));
		md.m_cmd.insert(CommandPair("gpio", new GPIOCommand()));
		md.m_cmd.insert(CommandPair("status", new StatusCommand()));
		md.m_cmd.insert(CommandPair("test", new TestCommand()));
		md.m_cmd.insert(CommandPair("shutdown", new ShutdownCommand()));

		int priority = 99999;

		for(int i_byte = 0; i_byte < 8; ++i_byte)
		{
			for(int i_pin = 0; i_pin < 8; ++i_pin)
			{
				md.m_gpio->get_gpio_context(&(TestCommand::m_testmode), GPIOManager::I2C_EXPANDER0, i_byte, i_pin, priority);
				md.m_gpio->get_gpio_context(&(TestCommand::m_testmode), GPIOManager::I2C_EXPANDER1, i_byte, i_pin, priority);
			}
		}

		TestCommand::start_test_server(md);
	}
	else
	{
		write_expander_hw(md, EXP_1, 3);
		write_expander_hw(md, EXP_1, 6);
		write_expander_hw(md, EXP_1, 7);

		write_expander(md, 0, EXP_2, 7, 0, 0);
		write_expander(md, 0, EXP_2, 7, 1, 0);
		write_expander(md, 0, EXP_2, 7, 2, 0);
		write_expander(md, 0, EXP_2, 7, 3, 0);
		write_expander(md, 0, EXP_2, 7, 4, 0);
		write_expander(md, 0, EXP_2, 7, 5, 0);
		write_expander_hw(md, EXP_2, 7);
		write_expander_hw(md, EXP_2, 2);
		write_expander_hw(md, EXP_2, 3);

		md.set_led("gps.r", false);
		md.set_led("sat.r", false);
		md.set_led("cell.r", false);
		md.set_led("wifi.r", false);
		md.set_led("zigbee.r", false);
		md.set_led("inp6.r", false);

		md.set_led("gps", false);
		md.set_led("sat", false);
		md.set_led("cell", false);
		md.set_led("wifi", false);
		md.set_led("zigbee", false);
		md.set_led("inp6", false);
		
		md.m_cmd.insert(CommandPair("blink", new BlinkCommand()));
		md.m_cmd.insert(CommandPair("irq", new IRQCommand()));
		md.m_cmd.insert(CommandPair("led", new LEDCommand()));
		md.m_cmd.insert(CommandPair("inputs", new InputsCommand()));
		md.m_cmd.insert(CommandPair("gpio", new GPIOCommand()));
		md.m_cmd.insert(CommandPair("status", new StatusCommand()));
		md.m_cmd.insert(CommandPair("shutdown", new ShutdownCommand()));

		pthread_create(&md.m_irq_thread, 0, irq_task, &md);
		pthread_create(&md.m_irq_task_thread, 0, on_irq, &md);
		pthread_create(&md.m_debounce_thread, 0, debounce_task, &md);
	}

	ServerData sd;
	init_ServerData(&sd, 64);
	sd.m_hook = &md;
	sd.m_max_client_callback = on_max_clients;
	sd.m_cs = client_command_server;
	::start_redstone_ud_server(&sd, "i2c-gpio-monitor", 1);
	signal_app_unix_socket_ready(g_app_name.c_str(), g_app_name.c_str());
	signal_app_ready(g_app_name.c_str());
 	ats::write_file("/dev/set-gpio", "Tu");  // Power LED to GREEN

	if(!ats::testmode())
	{
		post_command("inputs");
	}

	ats::infinite_sleep();
	ats_logf(ATSLOG_DEBUG, "I2C-GPIO-Monitor finished");
	return 1;
}
