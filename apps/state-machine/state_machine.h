#pragma once

#include <pthread.h>

#include "ats-common.h"
#include "atslogger.h"

#define SET_TO_STOP_STATE m_state_fn = 0
/*
 The above define is deprecated as it is not clear what the STOP_STATE is.  The following 
 define is an attempt to be more clear.  The define is used to catch coding errors
 and unexpected behaviour in the state machine
*/
#define SET_TO_STATE_MACHINE_FAILURE \
	{ \
		ats_logf(ATSLOG(0), "%s: State machine failed in state %d. Exiting with error code 1.", __PRETTY_FUNCTION__, m_state); \
		exit(1);\
	}

#define COMMON_STATE_MACHINE_START_FUNCTION_DEFINITION(P_TYPE) \
	void P_TYPE::start() \
	{ \
		SET_NEXT_STATE(0); \
	\
		while(m_state_fn) \
		{ \
			ats_logf(ATSLOG(log_level()), "%s: state %d", #P_TYPE, m_state); \
			(this->*m_state_fn)(); \
		} \
	\
		ats_logf(ATSLOG(1), "%s: State machine is stopping", #P_TYPE); \
	}

class StateMachineData;

class StateMachine
{
public:

	StateMachine(StateMachineData& p);
	virtual~ StateMachine();

	virtual void start();

	StateMachineData& my_data() const
	{
		return *m_data;
	}

	static void run_state_machine(StateMachine& p_state_machine);

	static const int m_invalid_state = -1;

	void set_log_level(int p_level)
	{
		m_log_level = p_level;
	}

	int log_level() const
	{
		return m_log_level;
	}

protected:
	int m_state;

private:
	int m_log_level;

	StateMachineData* m_data;
	pthread_t* m_state_machine_thread;

	static void* h_run_state_machine(void* );
};
