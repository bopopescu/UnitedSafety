#include "MyData.h"
#include "GPIOExpander.h"

GPIOExpander::GPIOExpander(int p_id) : m_id(p_id)
{
	memset(m_dirty, 0, sizeof(m_dirty));
	int i;

	for(i = 0; i < m_gpio_size; ++i)
	{
		m_gpio[i].m_exp = this;
	}

}

void GPIOExpander::flush(MyData& p_md)
{
	int i;

	for(i = 0; i < m_bank_count; ++i)
	{

		if(m_dirty[i])
		{
			m_dirty[i] = 0;
			write_expander_hw(p_md, m_id, i);
		}

	}

}
