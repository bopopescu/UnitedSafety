#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

#include "gcc.h"
#include "BlinkerProgram.h"
#include "timer-event.h"
#include "RedStone_IPC.h"
#include "atslogger.h"

#define SLEEP(P_sec, P_usec) {if(g_t.is_enabled()) g_t.start_timer_and_wait(P_sec, P_usec); else return;}
extern ATSLogger g_log;

static pthread_mutex_t g_mutex;
static ats::TimerEvent g_t;
static sem_t g_sem;
static struct BlinkerProgramContext* g_bpc = 0;

struct Led
{
	int m_expander;
	int m_byte;
	int m_pin;
};

static struct Led g_wifi;
static void get_led(const char* p_name, struct Led* p_led)
{
	bp_get_led_addr_byte_pin(p_name, &(p_led->m_expander), &(p_led->m_byte), &(p_led->m_pin));
}


static void set_led(struct Led* p_led, int p_val)
{
	bp_set_led(g_bpc, 0, p_led->m_expander, p_led->m_byte, p_led->m_pin, p_val);
}

static REDSTONE_IPC g_RedStoneData;

static void lock()
{
	pthread_mutex_lock(&g_mutex);
}

static void unlock()
{
	pthread_mutex_unlock(&g_mutex);
}

static void run()
{

	for(;;)
	{
		int s = g_RedStoneData.wifiLEDStatus();
		int i;

		for(i = 0; i < 5; ++i)
		{
			if( s == 2) 
			{
				set_led(&g_wifi, 1);
				SLEEP(0,3000000)
				set_led(&g_wifi, 0);
				SLEEP(0,1000000)
				break;
			}
			else if( s == 1)
			{
				set_led(&g_wifi, 1);
				SLEEP(0,1000000)
			}
			else if( s == 0)
			{
				set_led(&g_wifi, 0);
				SLEEP(0,1000000)
			}
			else if( s == 3 )
			{
				set_led(&g_wifi, 0);
				bp_set_named_led(g_bpc, 0, "wifi", 0);
				SLEEP(0,250000)
				set_led(&g_wifi, 1);
				SLEEP(0,250000)
			}
		}

		//ats_logf(ATSLOG(0), "set to  %d ", s);
	}
}

EXTERN_C void blink_init(struct BlinkerProgramContext* p_bpc, bp_StringMap p_arg)
{
	g_bpc = p_bpc;
	pthread_mutex_init(&g_mutex, 0);
	sem_init(&g_sem, 0, 0);
}

EXTERN_C void blink_data(const char* p_data)
{
	fprintf(stderr, "%s,%d:%s:\n", __FILE__, __LINE__, __FUNCTION__);
}

EXTERN_C void blink_start()
{
	const int priority = 0;
	void* wifi_led = bp_get_named_gpio_context(g_bpc, "wifi", priority);
	get_led("wifi", &g_wifi);

	run();

	bp_put_gpio_context(wifi_led);

	sem_post(&g_sem);
}

EXTERN_C void blink_stop()
{
	lock();
	g_t.enable_timer(false);
	g_t.stop_timer(true);
	unlock();
	sem_wait(&g_sem);
}

EXTERN_C void blink_fini()
{
	sem_destroy(&g_sem);
	pthread_mutex_destroy(&g_mutex);
}
