#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_ip(unsigned int ip)
{
	printf("%u.%u.%u.%u", ip>>24, (ip>>16) & 0xff, (ip>>8)&0xff, ip & 0xff);
}

int main(int argc, char *argv[])
{
	if(argc < 3) {
		return 1;
	}

	unsigned int subnet = atoi(argv[1]);
	subnet <<= 4;
	subnet |= (172<<24) | (16<<16);

	if(!strcmp("subnet", argv[2]))
		print_ip(subnet);

	// 76543210 76543210 76543210 76543210
	// 10110000 0001#### ######## ####SSSS
	int ip = subnet + 1;
	if(!strcmp("ip", argv[2]))
		print_ip(ip);

	int dhcp_start = subnet + 2;
	int last = subnet + 8;
	if(!strcmp("dhcp", argv[2])) {
		print_ip(dhcp_start);
		printf(" ");
		print_ip(last);
	}

	return 0;
}
