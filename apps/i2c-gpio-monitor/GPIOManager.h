#pragma once

#include "GPIOExpander.h"

class GPIOManager
{
public:
	static const int I2C_EXPANDER0 = 0x20;
	static const int I2C_EXPANDER1 = 0x21;

	GPIOExpander m_20;
	GPIOExpander m_21;

	GPIOManager() : m_20(0), m_21(1)
	{
	}

	GPIOExpander& get_expander(int p_addr)
	{
		return (p_addr == I2C_EXPANDER1) ? m_21 : m_20;
	}

#if 0
	void get_gpio(std::vector<GPIO*>& p_gpio, int p_addr, int p_byte)
	{
		p_gpio.resize(8);
		GPIOExpander& exp = get_expander(p_addr);
		GPIO* p = &(exp.m_gpio[(p_byte % 8) * 8]);
		p_gpio[0] = p;
		p_gpio[1] = p + 1;
		p_gpio[2] = p + 2;
		p_gpio[3] = p + 3;
		p_gpio[4] = p + 4;
		p_gpio[5] = p + 5;
		p_gpio[6] = p + 6;
		p_gpio[7] = p + 7;
	}
#endif

	// Description: Returns a pointer to the bank of 8 GPIO pins at I2C expander address "p_addr" and byte/offset "p_byte".
	//
	//	"p_addr" selects the I2C expander. Valid values are I2C_EXPANDER0 and I2C_EXPANDER1 (invalid values are interpreted as I2C_EXPANDER0).
	//
	//	"p_byte" selects the byte/offset of the bank of 8 GPIO pins.
	//
	// Return: A pointer to 8 consecutive GPIO pins (pointer starts at least significant GPIO pin) at I2C expander address "p_addr"
	//	and byte/offset "p_byte". This function never fails.
	GPIO* get_gpio(int p_addr, int p_byte)
	{
		GPIOExpander& exp = get_expander(p_addr);
		return &(exp.m_gpio[(p_byte % 8) * 8]);
	}

	// Description: Gets a new or existing GPIOContext for owner "p_owner". The GPIOContext returned will be for the single
	//	specific GPIO pin identified by I2C expander "p_addr", byte/offset "p_byte" and pin "p_pin". The GPIOContext
	//	priority will be set to "p_priority".
	//
	//	The returned GPIOContext pointer/resource can be freed by calling "put_gpio_context".
	GPIOContext* get_gpio_context(const ats::String* p_owner, int p_addr, int p_byte, int p_pin, int p_priority)
	{
		return ((get_gpio(p_addr, p_byte))[7 - (p_pin & 0x7)]).add_gpio(p_owner, p_priority);
	}

	// Description: Frees the previously created GPIOContext "p_g".
	void put_gpio_context(GPIOContext* p_g)
	{
		p_g->m_gpio->remove_gpio(p_g);
	}

	void flush(MyData& p_md)
	{
		m_20.flush(p_md);
		m_21.flush(p_md);
	}

};
