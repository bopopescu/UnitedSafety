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

// Description: Initializes the ADC2Volt conversion system. It is safe to call this function more than one
//	time (for re-calibration purposes).
//
//	XXX: This function is NOT thread safe. So make sure there are no concurrent calls to it.
void init_adc2volt();

int get_start_index(int p_adc);

int calc_stop_index(int p_index);

float get_voltage(int p_i, int p_f, int p_adc);

float ADC2Voltage(int adc);

} // namespace battery

} // namespace ats
