#pragma once
#include "GPIO.h"

class MyData;

class GPIOExpander
{
public:
	static const int m_gpio_size = 64;
	static const int m_bank_count = m_gpio_size / 8;
	GPIO m_gpio[m_gpio_size];

	// Description: Stores the modification (dirty) state of each GPIO pin.
	unsigned char m_dirty[m_bank_count];

	MyData* m_md;

	// Description: Expander ID/index. The first I2C GPIO expander has index 0.
	int m_id;

	GPIOExpander(int p_id);

	size_t size() const
	{
		return m_gpio_size;
	}

	void flush(MyData& p_md);

	void set_dirty(GPIO* p_gpio)
	{
		const int i = p_gpio - m_gpio;

		if((i >= 0) && (i < m_gpio_size))
		{
			const int byte = i >> 3;
			const int pin = 7 - (byte & 0x7);
			m_dirty[i >> 3] |= 0x1 << pin;
		}

	}

};
