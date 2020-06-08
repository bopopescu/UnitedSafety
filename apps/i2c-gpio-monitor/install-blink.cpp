#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

#include "gcc.h"
#include "BlinkerProgram.h"
#include "timer-event.h"

#define SLEEP(P_sec, P_usec) {if(g_t.is_enabled()) g_t.start_timer_and_wait(P_sec, P_usec); else return;}

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

static struct Led g_leds_green[6];
static struct Led g_leds_red[6];

static void get_led(const char* p_name, struct Led* p_led)
{
	bp_get_led_addr_byte_pin(p_name, &(p_led->m_expander), &(p_led->m_byte), &(p_led->m_pin));
}

static void set_led(struct Led* p_led, int p_val)
{
	bp_set_led(g_bpc, 0, p_led->m_expander, p_led->m_byte, p_led->m_pin, p_val);
}

static void lock()
{
	pthread_mutex_lock(&g_mutex);
}

static void unlock()
{
	pthread_mutex_unlock(&g_mutex);
}

// Description: Performs the following blink pattern:
//	1. [*...]
//	2. [.*..]
//	3. [..*.]
//	4. [...*]
//	5. [..*.]
//	6. [.*..]
//	7. Go back to 1
//	
static void run()
{
	int i,j;
	
	for(i = 0; i < 6; i++)
	{
		set_led(&g_leds_green[i], LED_OFF);
		set_led(&g_leds_red[i], LED_OFF);
	}

	for(;;)
	{
		for (i = 0; i < 6; i++)
		{
		  for (j = 0; j < 6; j++)
		  {
		  	if (i == j)
					set_led(&g_leds_green[j], LED_ON);
		  	else
					set_led(&g_leds_green[j], LED_OFF);
			}
			SLEEP(0, 125000);
		}

		for (i = 5; i >= 1; i--)
		{
		  for (j = 5; j >= 0; j--)
		  {
		  	if (i == j)
					set_led(&g_leds_green[j], LED_ON);
		  	else
					set_led(&g_leds_green[j], LED_OFF);
			}
			if (i != 5 && j != 5)  //	this prevents the last LED from staying on for 2 beats instead of 1
				SLEEP(0, 125000);
		}
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
}

EXTERN_C void blink_start()
{
	const int priority = 20;
	void* cell_led   = bp_get_named_gpio_context(g_bpc, "cell", priority);
	void* gps_led    = bp_get_named_gpio_context(g_bpc, "gps", priority);
	void* sat_led    = bp_get_named_gpio_context(g_bpc, "sat", priority);
	void* wifi_led   = bp_get_named_gpio_context(g_bpc, "wifi", priority);
	void* zigbee_led = bp_get_named_gpio_context(g_bpc, "zigbee", priority);
	void* inp6_led   = bp_get_named_gpio_context(g_bpc, "inp6", priority);
	void* Rcell_led   = bp_get_named_gpio_context(g_bpc, "cell.r", priority);
	void* Rgps_led    = bp_get_named_gpio_context(g_bpc, "gps.r", priority);
	void* Rsat_led    = bp_get_named_gpio_context(g_bpc, "sat.r", priority);
	void* Rwifi_led   = bp_get_named_gpio_context(g_bpc, "wifi.r", priority);
	void* Rzigbee_led = bp_get_named_gpio_context(g_bpc, "zigbee.r", priority);
	void* Rinp6_led   = bp_get_named_gpio_context(g_bpc, "inp6.r", priority);

	get_led("cell", &g_leds_green[0]);
	get_led("gps", &g_leds_green[1]);
	get_led("sat", &g_leds_green[2]);
	get_led("wifi", &g_leds_green[3]);
	get_led("zigbee", &g_leds_green[4]);
	get_led("inp6", &g_leds_green[5]);
	get_led("cell.r", &g_leds_red[0]);
	get_led("gps.r", &g_leds_red[1]);
	get_led("sat.r", &g_leds_red[2]);
	get_led("wifi.r", &g_leds_red[3]);
	get_led("zigbee.r", &g_leds_red[4]);
	get_led("inp6.r", &g_leds_red[5]);

	run();

	bp_put_gpio_context(wifi_led);
	bp_put_gpio_context(sat_led);
	bp_put_gpio_context(gps_led);
	bp_put_gpio_context(cell_led);
	bp_put_gpio_context(zigbee_led);
	bp_put_gpio_context(inp6_led);
	bp_put_gpio_context(Rwifi_led);
	bp_put_gpio_context(Rsat_led);
	bp_put_gpio_context(Rgps_led);
	bp_put_gpio_context(Rcell_led);
	bp_put_gpio_context(Rzigbee_led);
	bp_put_gpio_context(Rinp6_led);

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
