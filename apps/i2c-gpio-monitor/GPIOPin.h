#pragma once

class GPIOPin
{
public:
	int m_addr;
	int m_byte;
	int m_pin;

	GPIOPin()
	{
		m_addr = 0;
		m_byte = 0;
		m_pin = 0;
	}

	GPIOPin(int p_addr, int p_byte, int p_pin)
	{
		m_addr = p_addr;
		m_byte = p_byte;
		m_pin = p_pin;
	}

};
