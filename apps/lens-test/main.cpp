#include <signal.h>
#include <unistd.h>

#include "lens.h"
#include "MyData.h"
// #include "messageFormatter.h"

ATSLogger g_log;
MyData* g_md = NULL;

static int g_dbg = 20;
static bool g_disconnect = false;
static const ats::String g_app_name("isc-lens");
int g_defaultperiodictime = DEFAULTPERIODICTIME;
int g_alarmperiodictime = ALARMPERIODICTIME;


//-----------------------------------------------
// catch Ctrl-c and turn of leds.
void handleInterrupt(int s)
{
	// set the LED to off - never actually called
	send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=lens.g led=inp6 script=\"0,1000000;\" priority=30\r");
	send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=lens.r led=inp6.r script=\"0,1000000;\" priority=30\r");
	exit(0);
}


//=======================================================================================================================================
//
// Usage: lens-test  --channel [0-14] --time [seconds] (--transmit / --receive )  (--carrier / --modulated)
// first of --transmit or --receive will be used
// one of --carrier or --modulated only available if --transmit is used.

int main(int argc, char* argv[])
{
	if (argc == 1)
	{
		fprintf(stderr, "Usage:: lens-test channel=ch time=t transmit/receive carrier/modulated\n");
		fprintf(stderr, "           where ch 0-14\n");
		fprintf(stderr, "                 t is the time in seconds\n");
		fprintf(stderr, "                 only one of transmit or receive\n");
		fprintf(stderr, "                 if transmit then only one of carrier or modulated\n\n");
		exit(0);
	}
	
	// set the LED to yellow until paired to a unit
	send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=lens.g led=inp6 script=\"1,1000000;\" priority=30\r");
	send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=lens.r led=inp6.r script=\"1,1000000;\" priority=30\r");
	
	// handler to turn off LED if this is interrupted.
	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler =handleInterrupt;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);
	
	{
		db_monitor::ConfigDB db;
		g_dbg = db.GetInt("isc-lens", "LogLevel", 0);
		g_defaultperiodictime = db.GetInt("isc-lens", "DefaultPeriodicSeconds", DEFAULTPERIODICTIME);
		g_alarmperiodictime = db.GetInt("isc-lens", "AlaramPeriodicSeconds", ALARMPERIODICTIME);
	}

	g_log.set_global_logger(&g_log);
	g_log.set_level(g_dbg);
	g_log.open_testdata(g_app_name);

	
	static MyData md;  // starts up all the threads.
	g_md = &md;

	ats::StringMap &config = md.m_config;

	config.set("isc-dev-address", "/dev/spidev1.0");
	config.set("user", "applet");
	ats::StringMap opts;
	config.from_args(argc - 1, argv + 1, opts);

	// check arguments
	int data_mode = 0;
	int transmit = 0;
	if ( config.has_key("transmit") )
		data_mode = 1;

	if ( config.has_key("modulated") )
		data_mode = 1;

	fprintf(stderr, "channel %d\n", config.has_key("channel") ? config.get_int("channel"): -1);
	fprintf(stderr, "time %d\n", config.has_key("time")? config.get_int("time") : -1);
	fprintf(stderr, "transmit %d\n", config.has_key("transmit")? 1: 0);
	fprintf(stderr, "receive %d\n", config.has_key("receive")? 1: 0); 
	fprintf(stderr, "carrier %d\n", config.has_key("carrier")? 1: 0); 
	fprintf(stderr, "modulated %d\n", config.has_key("modulated")? 1: 0);

	if (!config.has_key("channel") || !config.has_key("time") || (config.has_key("transmit") && config.has_key("receive")) ||
			(!config.has_key("transmit") && !config.has_key("receive")) || 
			(config.has_key("transmit") && ((config.has_key("carrier") && config.has_key("modulated")) || (config.has_key("carrier") && config.has_key("modulated")))))
	{
		fprintf(stderr, "Usage:: lens-test --channel=ch --time=t --transmit --receive --carrier --modulated\n");
		fprintf(stderr, "           where ch 0-14\n");
		fprintf(stderr, "                 t is the time in seconds\n");
		fprintf(stderr, "                 only one of --transmit or --receive\n");
		fprintf(stderr, "                 if --transmit then only one of --carrier or --modulated\n\n");
		exit(0);
	}
	
	g_disconnect = config.get_bool("disconnect", false);

	ats_logf(ATSLOG_NONE, "================ ISC Lens started ================");

	md.lens = new Lens();
	if (md.lens->spi_init(config.get("isc-dev-address")) < 0)
	{
		ats_logf(ATSLOG_ERROR, "Can't open spi device");
		exit(1);
	}
	if (g_disconnect == true)
	{
		md.disconnect();
		return 0;
	}
	md.LensSetup();
	
	sleep(2);
	
	fprintf(stderr, "Disconnecting Network\n");
	md.lens->NetworkDisconnect();
	fprintf(stderr, "Network Disconnected\n");
	sleep(2);
	fprintf(stderr, "NetworkStatus: %d\n", md.lens->GetRadioNetworkStatus());
	md.WakeupRadio();
	sleep(2);
	fprintf(stderr, "NetworkStatus: %d\n", md.lens->GetRadioNetworkStatus());	
	
	md.lens->SetTestMode(config.get_int("channel"), data_mode, transmit);
	fprintf(stderr, "Testmode is running for %d seconds.\n", config.get_int("time"));
	sleep(config.get_int("time"));
	fprintf(stderr, "Disconnecting Network\n");
	md.lens->NetworkDisconnect();
	
	
	// set the LED to off - never actually called
	send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=lens.g led=inp6 script=\"0,1000000;\" priority=30\r");
	send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=lens.r led=inp6.r script=\"0,1000000;\" priority=30\r");

	return 0;
}

