#pragma once

//-------------------------------------------------------------------------------------------------
//
// CThreadWait - my class to handle some threading functions.
//
//	Wait - causes a thread to wait till a signal condition is set
//  Signal - signals the 'Wait'ed condition to continue.
#include <pthread.h>

class CThreadWait
{
	bool m_bHelper;
	pthread_mutex_t m_Lock;
	pthread_cond_t m_cond;

public:
	//---------------------------------------------------------------------------	
	CThreadWait() : m_bHelper(false)
	{
		pthread_mutex_init(&m_Lock, NULL);
	}
	
	//---------------------------------------------------------------------------	
	void Wait()  // wait for a condition to be set (using Signal() ) 
	{
		pthread_mutex_lock(&m_Lock);

		// We wait for helper to change (which is the true indication we are
		// ready) and use a condition variable so we can do this efficiently.
		while (!m_bHelper)
		{
    	pthread_cond_wait(&m_cond, &m_Lock);
		}
		m_bHelper = false;
		pthread_mutex_unlock(&m_Lock);
	}

	//---------------------------------------------------------------------------	
	void Signal()  // Signal the Wait function to carry on.
	{
		pthread_mutex_lock(&m_Lock);

		pthread_cond_signal(&m_cond);
		m_bHelper = true;
		pthread_mutex_unlock(&m_Lock);
	}
};
