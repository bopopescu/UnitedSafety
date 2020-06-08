#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
 
int main(int argc , char* argv[])
{

	if(argc < 3)
	{
		return 2;
	}

	const char* hostname = argv[1];
	const int delay = strtol(argv[2], 0, 0);

	for(;;)
	{
		struct hostent* he;

		if((he = gethostbyname(hostname)) != NULL)
		{
			return 0;
		}

		sleep(delay);

	}

	return 1;
}
