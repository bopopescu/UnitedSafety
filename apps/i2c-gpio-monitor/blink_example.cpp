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
		// ==================================================================
		// = Example of blinking a named LED
		// ==================================================================
		int i;

		for(i = 0; i < 5; ++i)
		{
			bp_set_named_led(g_bpc, 0, "act", 0);
			SLEEP(0,125000)
			bp_set_named_led(g_bpc, 0, "act", 1);
			SLEEP(0,125000)
		}

		SLEEP(3,0)

		// ==================================================================
		// = Example of blinking arbitrary LEDs
		// ==================================================================
		const int exp = 0;
		const int byte = 2;
		const int pin = 2;

		for(i = 0; i < 10; ++i)
		{
			bp_set_led(g_bpc, 0, exp, byte, pin, 1);
			bp_set_led(g_bpc, 0, exp, byte, pin + 1, 0);
			SLEEP(0,125000)

			bp_set_led(g_bpc, 0, exp, byte, pin, 0);
			bp_set_led(g_bpc, 0, exp, byte, pin + 1, 1);
			SLEEP(0,125000)
		}

		SLEEP(3,0)
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
	const int priority = 2;
	void* activity_led = bp_get_named_gpio_context(g_bpc, "act", priority);
	void* p1 = bp_get_gpio_context(g_bpc, 0, 2, 2, priority);
	void* p2 = bp_get_gpio_context(g_bpc, 0, 2, 3, priority);

	run();

	bp_put_gpio_context(p1);
	bp_put_gpio_context(p2);
	bp_put_gpio_context(activity_led);

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
