#include <iostream>

#include <stdio.h>
#include <stdlib.h>

#include "ats-common.h"
#include "adc2volt.h"

#define ARRAY_SIZE(a) \
	((sizeof(a) / sizeof(*(a))) / \
	static_cast<size_t>(!(sizeof(a) % sizeof(*(a)))))

static bool g_is_trulink_2500 = false;

static const ats::battery::ADC2Volt g_volts[] =
	{ // {Voltage, ADC value}
		{0.0,  0},
		{11.0, 1000},
		{12.0, 1093},
		{13.0, 1185},
		{14.0, 1275},
		{15.0, 1369},
		{16.0, 1458},
	};

int ats::battery::get_start_index(int p_adc)
{
	int i;

	for(i = 0; i < int(ARRAY_SIZE(g_volts) - 1); ++i)
	{

		if(!(p_adc >= (g_volts[i + 1].m_adc)))
		{
			break;
		}

	}

	return i;
}

int ats::battery::calc_stop_index(int p_index)
{
	const int end_index = ARRAY_SIZE(g_volts) - 1;
	return (p_index < end_index) ? (p_index + 1) : end_index;
}

void ats::battery::init_adc2volt()
{
	// XXX: "ats::is_trulink_model" is an expensive call, therefore only do it once. It
	//	is unreasonable to expect the TRULink hardware model to change at runtime. Also,
	//	if by some chance it does change, then the caller can re-run this init function
	//	to pick up the change.
	g_is_trulink_2500 = ats::is_trulink_model("2500");
}

float ats::battery::get_voltage(int p_i, int p_f, int p_adc)
{
	const float vi = g_volts[p_i].m_volt;
	const float vf = g_volts[p_f].m_volt;
	const float v_diff = vf - vi;

	const int adc_i = g_volts[p_i].m_adc;
	const int adc_f = g_volts[p_f].m_adc;
	const int adc_diff = adc_f - adc_i;

	float voltage = (adc_diff ? (vi + ((float(p_adc - adc_i) / adc_diff) * v_diff)) : vi);

	if(g_is_trulink_2500)
	{
		const float rds_2500_voltage_offset_due_to_diode = -0.2f;
		voltage += rds_2500_voltage_offset_due_to_diode;
	}

	return voltage;
}

float ats::battery::ADC2Voltage(int adc)
{
	float voltage = 0.0f;

	if(adc >= 0)
	{
		const int i = get_start_index(adc);
		const int f = calc_stop_index(i);
		voltage = get_voltage(i, f, adc);
	}

	return voltage;
}
