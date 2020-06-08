#include <signal.h>
#include <unistd.h>
#include <atslogger.h>
#include <INET_IPC.h>
#include "socket_interface.h"
#include "INetConfig.h"
#include "MyData.h"
#include "SocketData.h"

#define ISC_LENS_FW_VERSION 3

// #include "messageFormatter.h"

ATSLogger g_log;
static const ats::String g_app_name("isc-lens");

INetConfig *g_pLensParms;  // read the db-config parameters
INET_IPC g_INetIPC;  // common data.
MyData *g_pMyData;
static void* socketCallback(void* p);
void WaitForValidLensStartup(Lens &lens);
int UploadLensBootloader(Lens &lens, std::string strExistingVer, bool bIsLensInitialized); // See Upload.cpp for implementation.
void StartCommandServer(ServerData &socketServer, MyData *pMyData);
void Shutdown();

//-----------------------------------------------
// catch Ctrl-c and turn of leds.
void handleInterrupt(int s)
{
	// set the LED to off - never actually called in usual operation - added for testing
	g_INetIPC.LensConnectionStatus(false);
	printf("Interrupt received - shutting down\n");
	Shutdown();
	ats_logf(ATSLOG_ERROR, "isc-lens is disconnecting and shutting down.");
	usleep(250000);
	exit(1);  // exit with 1 so that the run-app doesn't stop
}
//=======================================================================================================================================
//
int main(int argc, char* argv[])
{
	
	wait_for_app_ready("i2c-gpio-monitor");

	g_INetIPC.initialize();  // clear out any old stuff (won't be needed after debugging is completed)
	g_INetIPC.LensConnectionStatus(false);// set the LED to yellow until paired to a unit
	//g_INetIPC.LENSInitFailure(false); // removed these lines ISCP-322
	//g_INetIPC.LENSUARTFailure(false); // removed these lines ISCP-322
	// handler to turn off LED if this is interrupted.
	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler =handleInterrupt;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);
	sigaction(SIGTERM, &sigIntHandler, NULL);

	INetConfig parms;  // read the db-config parameters
	g_pLensParms = &parms;
	g_INetIPC.MaxPeers(g_pLensParms->MaxPeers());

	//g_INetIPC.Dump();
	// start the logging.
	g_log.set_global_logger(&g_log);
	g_log.set_level(parms.LogLevel());
	g_log.open_testdata(g_app_name);
	ats_logf(ATSLOG_NONE, GREEN_ON "================ ISC Lens started [%d] ================" RESET_COLOR,ISC_LENS_FW_VERSION);
	
	Lens::reset(); // should clear out the backlog of messages

	Lens lens;

	WaitForValidLensStartup(lens);
	
	// 2.1.14.6 PRIN-FRS_INS-39 Verify OS Version meets Minimum Version
	// this is a critical failure - set the led to fast flash red and stop lens functions.
	if (lens.GetLensRegisters().rawRadioOSVersion() < 2)
	{
		
		g_INetIPC.ValidLensOS(false); //<ISCP-163>
		ats::infinite_sleep();  // you can never leave!
	}

	std::string version;
	UploadLensBootloader(lens, lens.GetLensRegisters().rawRadioProtocolVersion_Upload(version), true);
	lens.reset();
	WaitForValidLensStartup(lens);
	MyData md(&lens);  // starts up all the threads.
	g_pMyData = &md;

	ServerData socketServer;
	StartCommandServer(socketServer, &md);
	md.LensSetup();  // this won't return until the radio has set the initialize flag to 0xA5 in shared memory.
	ats::infinite_sleep();
	g_INetIPC.LensConnectionStatus(false); // this will set the LED to off - never actually called
	return 0;
}

//=======================================================================================================================================
void WaitForValidLensStartup(Lens &lens)
{

	if (! lens.IsRadioInitialized())
	{
		// set the led to fast flashing red.
		g_INetIPC.LensScriptUploading(true);
		int count = 0;

		while (! lens.IsRadioInitialized())
		{
			printf("Resetting...\n");
			lens.reset();
			usleep(2000000); // 5Hz recheck
			int m_urgent_fd = open("/dev/lens-urgent", O_RDONLY);
			lens.Wakeup(m_urgent_fd);
			close(m_urgent_fd);
			usleep(200000); // 5Hz recheck
			lens.ReadRegisters();  // loads the Registers into m_LensRegisters.
			if (++count == 5)  // only do this once
			{
				std::string version;
				ats_logf(ATSLOG_NONE, RED_ON "Running UploadLensBootloader due to uninitialized LENS." RESET_COLOR);
				UploadLensBootloader(lens, lens.GetLensRegisters().rawRadioProtocolVersion_Upload(version), false);
				ats_logf(ATSLOG_NONE, GREEN_ON "UploadLensBootloader complete." RESET_COLOR);
			}
			else if (count > 5)
			{
				g_INetIPC.LensScriptUploading(false);
				//<ISCP-163>
				ats_logf(ATSLOG_NONE, GREEN_ON "---------LENSInitFailure TRUE.--------" RESET_COLOR);				
				g_INetIPC.LENSInitFailure(true);
				usleep(2000000);
				sleep(5);
				//<ISCP-163>
				exit(0);  //exit with 0 so the run-app isc-lens shuts down and doesn't restart.
			}
		}
		// unset the flashing yellow.
		g_INetIPC.LensScriptUploading(false);
	}	
}


//=======================================================================================================================================
// defined here so it doesn't get multiply defined in the header.
#include "rapidjson.h"
ats::String DocToHex(const Document& dom)
{
	StringBuffer buffer;
	Writer<StringBuffer> writer(buffer);
	dom.Accept(writer);
	ats::String rs = buffer.GetString();
	boost::replace_all(rs, "\"$$", "");
	boost::replace_all(rs, "$$\"", "");
	ats_logf(ATSLOG_DEBUG, "%s, %d: %s", __FILE__, __LINE__, rs.c_str());
	return ats::to_hex(rs);
}

//=======================================================================================================================================
static void* socketCallback(void* p)
{
	const size_t max_cmd_length = 2048;
	ClientData& cd = *((ClientData*)p);
	MyData *pMyData = (MyData *)(cd.m_data->m_hook);

	CommandBuffer cb;
	init_CommandBuffer(&cb);
	alloc_dynamic_buffers(&cb, 256, 65536);

	bool command_too_long = false;
	ats::String cmd;

	ClientDataCache cache;
	init_ClientDataCache(&cache);

	for(;;)
	{
		char ebuf[256];
		const int c = client_getc_cached(&cd, ebuf, sizeof(ebuf), &cache);

		if(c < 0)
		{
			if(c != -ENODATA)
			{
				ats_logf(ATSLOG_ERROR, "Client %p: client_getc_cached failed: %s", &cd, ebuf);
			}

			break;
		}

		if((c != '\r') && (c != '\n'))
		{
			if(cmd.length() >= max_cmd_length) command_too_long = true;
			else cmd += c;
			continue;
		}

		if(command_too_long)
		{
			ats_logf(ATSLOG_ERROR, "Client %p: command too long (%64s...)", &cd, cmd.c_str());
			cmd.clear();
			command_too_long = false;
			continue;
		}

		const char* err;

		if((err = gen_arg_list(cmd.c_str(), cmd.length(), &cb)))
		{
			ats_logf(ATSLOG_ERROR, "Client %p: gen_arg_list failed (%s)", &cd, err);
			break;
		}

		const ats::String full_command(cmd);
		cmd.clear();

		if(cb.m_argc < 1)
		{
			continue;
		}

		const ats::String cmd(cb.m_argv[0]);

		if("shutdown" == cmd) 
		{
			ats_logf(ATSLOG_ERROR, "%s,%d: " MAGENTA_ON "Shutdown received - disconnecting Lens" RESET_COLOR, __FILE__, __LINE__);
			//g_INetIPC.RunState(SHUTDOWN); //ISCP-346
			if (g_INetIPC.PeerCount() > 0)
			{
				for (short i = 0; i < 3; i++)
				{
					pMyData->UpdateInstrumentStatus(*pMyData);
				
					while (pMyData->m_pLens->GetStatusDataRead() == false)
						usleep(10 * 1000);
				}
			}
			pMyData->m_pLens->NetworkDisconnect();
			ats_logf(ATSLOG_ERROR, "%s,%d: " MAGENTA_ON "Shutdown complete." RESET_COLOR, __FILE__, __LINE__);
		}
		else if ( "test" == cmd)
		{
			send_cmd(cd.m_sockfd, MSG_NOSIGNAL, "isc-lens socket server is working!\n");
		}
		else if ("help" == cmd)
		{
			send_cmd(cd.m_sockfd, MSG_NOSIGNAL, "isc-lens socket server command list:\n   test - simple response\n   shutdown - initiate lens shutdown\n   help - this help\n");
		}
	}

	free_dynamic_buffers(&cb);
	shutdown(cd.m_sockfd, SHUT_WR);
	// FIXME: Race-condition exists because "shutdown" does not gaurantee that the receiver will receive all the data.
	close_client(&cd);
	return 0;
}

//=======================================================================================================================================
void StartCommandServer(ServerData &socketServer, MyData *pMyData)
{
		// Set up a server socket for receiving messages from other processes - specifically the shutdown message from power-monitor.
	init_ServerData(&socketServer, 32);

	socketServer.m_cs = socketCallback;
	socketServer.m_hook = pMyData;

	if( ::start_redstone_ud_server(&socketServer, "isc-lens", 1))
	{
		ats_logf(ATSLOG_ERROR, "%s,%d: " RED_ON "Error starting socket server: %s" RESET_COLOR, __FILE__, __LINE__, socketServer.m_emsg);
		return;
	}
}


void Shutdown()
{
	ats_logf(ATSLOG_ERROR, "%s,%d: " MAGENTA_ON "Shutdown received - disconnecting Lens" RESET_COLOR, __FILE__, __LINE__);
	//g_INetIPC.RunState(SHUTDOWN); //ISCP-346

	if (g_INetIPC.PeerCount() > 0)
	{
		for (short i = 0; i < 3; i++)
		{
			g_pMyData->UpdateInstrumentStatus(*g_pMyData);
			
			while (g_pMyData->m_pLens->GetStatusDataRead() == false)
				usleep(10 * 1000);
		}
	}
	g_pMyData->m_pLens->NetworkDisconnect();
	ats_logf(ATSLOG_ERROR, "%s,%d: " MAGENTA_ON "Shutdown complete." RESET_COLOR, __FILE__, __LINE__);
}
