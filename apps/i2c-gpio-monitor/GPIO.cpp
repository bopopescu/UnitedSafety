#include "GPIOManager.h"
#include "GPIO.h"

void GPIO::remove_gpio(const ats::String* p_owner)
{

	if(!p_owner || (m_stack.size() <= 1))
	{
		return;
	}

	GPIOVector::iterator i = m_stack.begin();

	while(i != m_stack.end())
	{
		GPIOContext& c = *(*i);

		if(c.m_owner == p_owner)
		{
			delete &c;
			m_stack.erase(i);
			m_exp->set_dirty(this);
			return;
		}

		++i;
	}

}
