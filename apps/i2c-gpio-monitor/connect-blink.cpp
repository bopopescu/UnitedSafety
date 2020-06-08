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

static struct Led g_leds_green;
static struct Led g_leds_red;

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
//	1. [**..]
//	2. [..**]
//	3. [**..]
//	4. [..**]
//	5. Go back to 1
//	
static void run()
{
	short i;

         //ISCP-328
        set_led(&g_leds_red, LED_OFF);
	set_led(&g_leds_green, LED_OFF);
    
	for(i = 0; i < 10; i++)
	{
		set_led(&g_leds_green, LED_ON);
		set_led(&g_leds_red, LED_ON);
		usleep(500 * 1000);		
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
	void* wifi_led   = bp_get_named_gpio_context(g_bpc, "wifi", priority);
	void* Rwifi_led   = bp_get_named_gpio_context(g_bpc, "wifi.r", priority);

	get_led("wifi", &g_leds_green);
	get_led("wifi.r", &g_leds_red);

	run();

	bp_put_gpio_context(wifi_led);
	bp_put_gpio_context(Rwifi_led);

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
