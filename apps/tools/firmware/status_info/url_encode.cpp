#include "ats-string.h"

int main()
{
	ats::String s;

	for(;;)
	{
		char buf[1024];
		const size_t nread = fread(buf, 1, sizeof(buf), stdin);

		if(!nread)
		{
			break;
		}

		s.append(buf, nread);
	}

	printf("%s", (ats::urlencode(s)).c_str());
	return 0;
}
