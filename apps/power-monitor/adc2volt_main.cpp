#include <stdio.h>
#include <stdlib.h>

#include "ats-common.h"
#include "adc2volt.h"

int main(int argc, char* argv[])
{

	if(argc < 2)
	{
		fprintf(stderr, "usage: %s <ADC value>\n", argv[0]);
		return 1;
	}

	const int adc = strtol(argv[1], 0, 0);

	float voltage = 0.0f;

	if(adc >= 0)
	{
		const int i = ats::battery::get_start_index(adc);
		const int f = ats::battery::calc_stop_index(i);
		voltage = ats::battery::get_voltage(i, f, adc);
	}

	printf("%3.1f\n", voltage);

	return 0;
}

