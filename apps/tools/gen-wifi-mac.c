#include <stdio.h>

int main()
{
	FILE *f = fopen("/dev/urandom", "r");
	if(f) {
		unsigned char mac[6];
		unsigned char *p = mac;
		int i;
		for(i = 0; i < 6; ++i) {
			fread((char *)p, 1, 1, f);
			++p;
		}
		fclose(f);
		printf("%02X:%02X:%02X:%02X:%02X:%02X",
			mac[0],
			mac[1],
			mac[2],
			mac[3],
			mac[4],
			mac[5]);
		return 0;
	}

	return 1;
}
