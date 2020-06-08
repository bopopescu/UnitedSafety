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

class Led
{
public:
	Led()
	{
	}

	Led(int p_exp, int p_byte, int p_pin, int p_val, int p_act)
	{
		m_expander = p_exp;
		m_byte = p_byte;
		m_pin = p_pin;
		m_val = p_val;
		m_active_hi = p_act;
	}

	int value() const
	{
		return m_active_hi ? ((!m_val) ? 1 : 0) : (m_val ? 1 : 0);
	}

	int m_expander;
	int m_byte;
	int m_pin;
	int m_val;
	int m_active_hi;
};

static struct Led g_pin[10];

static void set_led(Led* p_led, int p_val)
{
	p_led->m_val = p_val;
	bp_set_led(g_bpc, 0, p_led->m_expander, p_led->m_byte, p_led->m_pin, p_led->value());
}

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
	int i = 0;

	for(i = 0; i < 10; ++i)
	{
		set_led(&g_pin[i], 0);
	}

	int g_phase[] = {
		0,0, 0,0, 0,0, 0,0, 0,0,
		1,0, 1,0, 1,0, 1,0, 1,0,
		0,1, 0,1, 0,1, 0,1, 0,1,
		1,1, 1,1, 1,1, 1,1, 1,1,
		1,1, 1,1, 1,1, 1,1, 1,1,
		0,0, 0,0, 0,0, 0,0, 0,0
		};

	int phase = 0;
	i = 0;

	for(;;)
	{

		for(int n = 0; n < 4; ++n)
		{
			set_led(&g_pin[i], n & 1);
			SLEEP(0, 125000);
		}

		set_led(&g_pin[i], g_phase[(phase * 10) + i]);

		++i;

		if(10 == i)
		{
			phase = (phase + 1) % 6;
			i = 0;
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
	const int priority = 999999;
	void* pin[10];
	int i;

	{
		void** p = pin;
		Led* g = g_pin;

		// Activity GPS-G/R, Iridium-G/R, WiFi/Zigbee-G/R, Cell-G/R
		for(i = 0; i < 8; ++i)
		{
			*(p++) = bp_get_gpio_context(g_bpc, 0x20, 2, i, priority);
			*(g++) = Led(0, 2, i, 0, 0);
		}

		// Power-Green
		*(p++) = bp_get_gpio_context(g_bpc, 0x20, 3, 0, priority);
		*(g++) = Led(0, 3, 0, 0, 0);

		// Power-Red
		*(p++) = bp_get_gpio_context(g_bpc, 0x20, 3, 1, priority);
		*(g++) = Led(0, 3, 1, 0, 0);
	}

	run();

	for(i = 0; i < 10; ++i)
	{
		bp_put_gpio_context(pin[i]);
	}

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
