#include "state_machine.h"

#define SET_NEXT_STATE(P_STATE) m_state_fn = &StateMachine::state_ ## P_STATE; m_state = P_STATE

const int StateMachine::m_invalid_state;

void StateMachine::run_state_machine(StateMachine& p_state_machine)
{
	pthread_create(
			p_state_machine.m_state_machine_thread,
			(pthread_attr_t*)0,
			h_run_state_machine,
			&p_state_machine);
}

void* StateMachine::h_run_state_machine(void* p_statemachine)
{
	StateMachine& sm = *((StateMachine*)p_statemachine);
	sm.start();
	return 0;
}

StateMachine::StateMachine(StateMachineData& p_data)
{
	m_log_level = 1;
	m_data = &p_data;
	m_state_machine_thread = new pthread_t;
	m_state = m_invalid_state;
}

StateMachine::~StateMachine()
{
	delete m_state_machine_thread;
}

void StateMachine::start()
{
}
