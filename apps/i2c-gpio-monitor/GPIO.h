#pragma once
#include <vector>

#include "ats-common.h"
#include "GPIOContext.h"

class GPIOExpander;

// Description: Class "GPIO" represents a single GPIO pin. The class also supports "stacking"
//	pin values for this single "GPIO" by priority. So a GPIO can have N pin values stored
//	internally, however when a caller requests the pin value they will only get the value
//	for the highest priority (which will return true or false, ON or OFF). This means
//	that "GPIO" supports callers with a higher priority "seamlessly" overriding the GPIO
//	pin value, without affecting the pin value assignments from lower priority callers.
//
// XXX: The time-complexity used by "GPIO" is O(N), instead of O(log N) or better, because
//	it is likely that N will only be 3 or 4 max, and it is easier/faster to implement
//	an O(N) solution.
class GPIO
{
public:

	typedef std::vector <GPIOContext*> GPIOVector;
	GPIOVector m_stack;
	GPIOExpander* m_exp;

	GPIO()
	{
		add_gpio(0, -1);
	}

	~GPIO()
	{
		remove_all_gpios();
	}

	// Description: Adds a new GPIO context to the GPIO pin stack for owner "p_owner".
	//
	//	If "p_owner" was already added to this stack, then nothing is done.
	//
	//	"p_priority" gives the priority of the GPIO pin setting where the higher the
	//	integer value, the higher the priority.
	//
	// Return: The GPIOContext for owner "p_owner" is always returned (this function never
	//	fails).
	GPIOContext* add_gpio(const ats::String* p_owner, int p_priority)
	{
		GPIOContext* g = get_context_by_owner(p_owner);

		if(g)
		{
			return g;
		}

		g = new GPIOContext(this, p_owner, p_priority);

		GPIOVector::iterator i = m_stack.begin();

		while(i != m_stack.end())
		{

			if(p_priority <= (*i)->m_priority)
			{
				m_stack.insert(i, g);
				return g;
			}

			++i;
		}

		m_stack.push_back(g);
		return g;
	}

	void remove_gpio(GPIOContext* p_g)
	{

		if(p_g)
		{
			remove_gpio(p_g->m_owner);
		}

	}

	void remove_gpio(const ats::String* p_owner);

	GPIOContext* get_context_by_owner(const ats::String* p_owner) const
	{
		GPIOVector::const_iterator i = m_stack.begin();

		while(i != m_stack.end())
		{
			GPIOContext& c = *(*i);
			++i;

			if(c.m_owner == p_owner)
			{
				return &c;
			}

		}

		return 0;
	}

	// Description: Return the active/effective GPIO value (in other words, get the
	//	highest priority setting).
	//
	// XXX: This only gets the value within this hardware GPIO abstraction class.
	//	The returned value is not the current value of the hardware GPIO.
	int get_value() const
	{
		return m_stack.empty() ? 0 : (*(--(m_stack.end())))->m_state;
	}

	// Description: Set the active/effective GPIO value (in other words, set the
	//	value of the highest priority setting).
	//
	// XXX: This only sets the value within this hardware GPIO abstraction class.
	//	The hardware GPIO is not touched.
	int set_value(int p_val)
	{
		return m_stack.empty() ? 0 : ((*(--(m_stack.end())))->m_state = p_val & 0x1);
	}

	// Description: Sets the GPIO value for the specified owner.
	//
	// XXX: The GPIO for the specified owner is abstracted from the actual hardware
	//	(so this function does not touch the hardware GPIO).
	int set_value(const ats::String* p_owner, int p_val)
	{
		GPIOContext* g = get_context_by_owner(p_owner);

		if(g)
		{
			return g->m_state = p_val & 0x1;
		}

		return 0;
	}

private:

	void remove_all_gpios()
	{
		GPIOVector::iterator i = m_stack.begin();

		while(i != m_stack.end())
		{
			GPIOContext& c = *(*i);
			delete &c;
			++i;
		}

		m_stack.clear();
	}

};
