#include <stdio.h>

int main()
{

	for(;;)
	{
		FILE* f = fopen("/dev/watchdog", "r+");

		if(f)
		{
			fwrite("\n", 1, 1, f);
			fclose(f);
		}

		sleep(1);
	}

	return 0;
}
