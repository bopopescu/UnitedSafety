// LENSUploadMode - places the LENS in upload mode for programming the radio.

#include <signal.h>
#include <unistd.h>
#include <INET_IPC.h>
#include "INetConfig.h"
#include "lens.h"
static const ats::String g_app_name("LENSUploadMode");

INetConfig *g_pLensParms;  // read the db-config parameters
INET_IPC g_INetIPC;  // common data.
ATSLogger g_log;

void WaitForValidLensStartup(Lens &lens);

//-----------------------------------------------
// catch Ctrl-c and turn of leds.
void handleInterrupt(int s)
{
	// set the LED to off - never actually called in usual operation - added for testing
	Lens::reset();
	g_INetIPC.LensConnectionStatus(false);
	exit(0);
}
//=======================================================================================================================================
//
int main(int argc, char* argv[])
{
	g_INetIPC.initialize();  // clear out any old stuff (won't be needed after debugging is completed)
	g_INetIPC.LensConnectionStatus(false);// set the LED to yellow until paired to a unit
	// handler to turn off LED if this is interrupted.
	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler =handleInterrupt;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);

	INetConfig parms;  // read the db-config parameters
	g_pLensParms = &parms;
	g_INetIPC.MaxPeers(g_pLensParms->MaxPeers());

	printf("LENSUploadMode started: \n  Hit Ctrl-C to terminate\n");
	
//	Lens::reset(); // should clear out the backlog of messages

	Lens lens;
	lens.reset();
	usleep(100 * 1000);
	//printf("Network status: %d (Line %d)\n", lens.GetRadioNetworkStatus(), __LINE__);
	lens.NetworkDisconnect();
	usleep(100 * 1000);
	//printf("Network status: %d (Line %d)\n", lens.GetRadioNetworkStatus(), __LINE__);
	int m_urgent_fd = open("/dev/lens-urgent", O_RDONLY);
	lens.Wakeup(m_urgent_fd);
	close(m_urgent_fd);
	usleep(100 * 1000);
	//printf("Network status: %d (Line %d)\n", lens.GetRadioNetworkStatus(), __LINE__);

	lens.SetUploadMode();
	usleep(100 * 1000);
	//printf("Network status: %d (Line %d)\n", lens.GetRadioNetworkStatus(), __LINE__);
/*
	while (1)
	{
		printf("Network status: %d\r", lens.GetRadioNetworkStatus());
		fflush(0);
		sleep(1);
	}
*/
	return 0;
}


void WaitForValidLensStartup(Lens &lens)
{
	if (! lens.IsRadioInitialized())
	{
		// set the led to fast flashing red.
		send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=badlens.g led=inp6 script=\";0,1000000\" priority=10\r");
		send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=badlens.r led=inp6.r script=\"1,125000;0,125000\" priority=10 \r");

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
		}
		// unset the flashing red.
		send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink del badlens.g \r");
		send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink del badlens.r \r");
	}	
}
