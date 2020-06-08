#pragma once

namespace ats
{

namespace battery
{

struct ADC2Volt
{
	float m_volt;
	int m_adc;
};

int get_start_index(int p_adc);

int calc_stop_index(int p_index);

float get_voltage(int p_i, int p_f, int p_adc);

float ADC2Voltage(int adc);

} // namespace battery

} // namespace ats
