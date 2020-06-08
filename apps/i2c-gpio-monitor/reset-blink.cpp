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

// Description: Performs the reboot blink pattern:
//	Power light is orange on/off at 1/4 second interval
static void run()
{
	int i=0;
	
	ats::write_file("/dev/set-gpio", "Tu");  // Power LED to off.

	for(;;)
	{
		i++;
		if (i % 2)
			ats::write_file("/dev/set-gpio", "tU");  // Power LED to off.
		else
			ats::write_file("/dev/set-gpio", "Tu");  // Power LED to off.
		SLEEP(0, 250 * 1000);
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
	run();
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
