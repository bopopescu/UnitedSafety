#pragma once

#include "ats-common.h"

class GPIO;

class GPIOContext
{
public:
	GPIO* m_gpio;
	const ats::String* m_owner;
	int m_priority;
	int m_state;

	GPIOContext(GPIO* p_gpio, const ats::String* p_owner, int p_priority)
	{
		m_gpio = p_gpio;
		m_owner = p_owner;
		m_priority = p_priority;
		m_state = 0;
	}

private:
	GPIOContext(const GPIOContext&);
	GPIOContext& operator =(const GPIOContext&);
};
