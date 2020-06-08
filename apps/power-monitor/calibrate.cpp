#include <iostream>
#include <deque>

#include <unistd.h>

#include "ats-common.h"

int main(int argc, char* argv[])
{
	ats::StringMap arg;
	arg.set("fname", "/dev/battery");
	arg.set("avg_size", "100");
	arg.set("read_usleep", "10000");
	arg.from_args(argc - 1, argv + 1);

	const ats::String& fname = arg.get("fname");
	FILE* f = fopen(fname.c_str(), "r");

	if(!f)
	{
		fprintf(stderr, "Failed to open \"%s\"\n", fname.c_str());
		return 1;
	}

	ats::ReadDataCache_FILE rdc(f);

	ats::String value;
	int adc;

	const size_t avg_size = arg.get_int("avg_size");

	if(!avg_size)
	{
		fprintf(stderr, "avg_size cannot be zero\n");
		return 1;
	}

	int count = 0;

	std::deque <int> adc_values;

	for(;;)
	{
		usleep(arg.get_int("read_usleep"));
		const int c = rdc.getc();

		if(c < 0)
		{
			break;
		}

		if('\n' == c)
		{
			adc = strtol(value.c_str(), 0, 0);
			value.clear();
		}
		else
		{
			value += c;
			continue;
		}

		if(adc_values.size() == avg_size)
		{
			adc_values.pop_front();
		}

		adc_values.push_back(adc);

		count = (count + 1) % avg_size;

		if(!count)
		{
			printf("\n");
		}

		int sum = 0;
		std::deque<int>::const_iterator i = adc_values.begin();

		while(i != adc_values.end())
		{
			sum += *i;
			++i;
		}

		const int avg = sum / float(adc_values.size());

		printf("ADC=%d, AVG[%zu,%02d]=%d\n", adc, adc_values.size(), count, avg);
	}

	return 0;
}
