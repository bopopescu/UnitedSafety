#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define NOTHING 0
#define CONTINUE 1
#define BREAK 2

static int next_step(int p_and, int p_done, int p_any, int p_i, int p_argc)
{

	if(p_and && (!p_done))
	{

		if(p_any && ((p_i + 1) < p_argc))
		{
			return CONTINUE;
		}

		return BREAK;
	}

	return NOTHING;
}

static int check_up_broadcast_running_multicast(int p_fd, struct ifreq* p_ifr, int p_up, int p_broadcast, int p_running, int p_multicast, int p_and)
{
	if(!(p_up || p_broadcast || p_running || p_multicast))
	{
		return 1;
	}

	const int ret = ioctl(p_fd, SIOCGIFFLAGS, p_ifr);

	if(!ret)
	{
		return p_and ?
			( // All selected flags must be true together
				(p_up ? (p_ifr->ifr_flags & IFF_UP) : 1)
				&& (p_broadcast ? (p_ifr->ifr_flags & IFF_BROADCAST) : 1)
				&& (p_running ? (p_ifr->ifr_flags & IFF_RUNNING) : 1)
				&& (p_multicast ? (p_ifr->ifr_flags & IFF_MULTICAST) : 1)
			)
			:
			( // At least one of the selected flags must be true
				(p_up ? (p_ifr->ifr_flags & IFF_UP) : 0)
				|| (p_broadcast ? (p_ifr->ifr_flags & IFF_BROADCAST) : 0)
				|| (p_running ? (p_ifr->ifr_flags & IFF_RUNNING) : 0)
				|| (p_multicast ? (p_ifr->ifr_flags & IFF_MULTICAST) : 0)
			)
			;
	}

	return 0;
}

static int check_mac(int p_fd, struct ifreq* p_ifr, int p_mac)
{

	if(!p_mac)
	{
		return 1;
	}

	if(!ioctl(p_fd, SIOCGIFHWADDR, p_ifr))
	{
		return 1;
	}

	return 0;
}

static int check_ip(int p_fd, struct ifreq* p_ifr, int p_ip)
{
	if(!p_ip)
	{
		return 1;
	}

	if(!ioctl(p_fd, SIOCGIFADDR, p_ifr))
	{
		return 1;
	}

	return 0;
}
 
int main(int argc , char* argv[])
{

	if(argc < 3)
	{
		return 2;
	}

	int and = 0;
	int any = 0;
	int up = 0;
	int running = 0;
	int broadcast = 0;
	int multicast = 0;
	int mac = 0;
	int ip = 0;
	int retry = -1;
	const int delay = strtol(argv[1], 0, 0);

	const int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

	if(-1 == fd)
	{
		return 3;
	}

	struct Interface
	{
		struct ifreq m_ifr;
		int m_valid;
	};

	static const int max_ifr = 16;
	struct Interface ifr_list[max_ifr];

	{
		int i;

		for(i = 0; i < max_ifr; ++i)
		{
			ifr_list[i].m_valid = 0;
		}

	}

	int if_start = 0;
	int done = 0;

	for(;;)
	{
		int i;

		for(i = (if_start ? if_start : 2); i < argc; ++i)
		{
			const char* arg = argv[i];

			if('-' == arg[0])
			{
				const char* s = arg + 1;

				while(*s)
				{
					const char option = *(s++);

					switch(option)
					{
					case 'A': and = 1; break;
					case 'a': any = 1; break;
					case 'u': up = 1; break;
					case 'r': running = 1; break;
					case 'b': broadcast = 1; break;
					case 'm': multicast = 1; break;
					case 'M': mac = 1; break;
					case 'i': ip = 1; break;
					case 'R':

						if(*s)
						{
							retry = strtol(s, 0, 0);

							while(*(++s))
							{
							}

						}
						else if((i+1) < argc)
						{
							retry = strtol(argv[i+1], 0, 0);
							++i;
						}
						else
						{
							fprintf(stderr, "Option 'R' requires a <retry> argument\n");
							return 4;
						}

						break;
					default:
						fprintf(stderr, "Invalid option \"%c\"\n", option);
						return 5;
					}
				}
			
				continue;
			}

			if(!if_start)
			{
				if_start = i;
			}

			const int if_num = i - if_start;

			if(if_num >= max_ifr)
			{
				fprintf(stderr, "Too many interfaces, max=%d\n", max_ifr);
				return 6;
			}

			struct Interface* iface = ifr_list + if_num;
			iface->m_valid = 0;

			struct ifreq* ifr = &(iface->m_ifr);
			ifr->ifr_name[IFNAMSIZ - 1] = '\0';

// ATS FIXME: "ifconfig" example system calls for reference. Remove this block once development is complete.
#if 0
ioctl(4, SIOCGIFFLAGS, {ifr_name="eth0", ifr_flags=IFF_UP|IFF_BROADCAST|IFF_RUNNING|IFF_MULTICAST}) = 0
ioctl(4, SIOCGIFHWADDR, {ifr_name="eth0", ifr_hwaddr=aa:bb:cc:11:22:33}) = 0
ioctl(4, SIOCGIFMETRIC, {ifr_name="eth0", ifr_metric=0}) = 0
ioctl(4, SIOCGIFMTU, {ifr_name="eth0", ifr_mtu=1500}) = 0
ioctl(4, SIOCGIFMAP, {ifr_name="eth0", ifr_map={mem_start=0, mem_end=0, base_addr=0x8000, irq=0, dma=0, port=0}}) = 0
ioctl(4, SIOCGIFMAP, {ifr_name="eth0", ifr_map={mem_start=0, mem_end=0, base_addr=0x8000, irq=0, dma=0, port=0}}) = 0
ioctl(4, SIOCGIFTXQLEN, {ifr_name="eth0", ifr_qlen=1000}) = 0
ioctl(4, SIOCGIFADDR, {ifr_name="eth0", ifr_addr={AF_INET, inet_addr("192.168.1.94")}}) = 0
ioctl(4, SIOCGIFDSTADDR, {ifr_name="eth0", ifr_dstaddr={AF_INET, inet_addr("192.168.1.94")}}) = 0
ioctl(4, SIOCGIFBRDADDR, {ifr_name="eth0", ifr_broadaddr={AF_INET, inet_addr("192.168.1.255")}}) = 0
ioctl(4, SIOCGIFNETMASK, {ifr_name="eth0", ifr_netmask={AF_INET, inet_addr("255.255.255.0")}}) = 0
#endif
			strncpy(ifr->ifr_name, arg, IFNAMSIZ - 1);

			// Wait for the named interface to be UP, BROADCAST, RUNNING or MULTICAST.
			done |= check_up_broadcast_running_multicast(fd, ifr, up, broadcast, running, multicast, and);

			int step;

			if((step = next_step(and, done, any, i, argc)))
			{

				if(CONTINUE == step)
				{
					continue;
				}

				break;
			}

			done |= check_mac(fd, ifr, mac);

			if((step = next_step(and, done, any, i, argc)))
			{

				if(CONTINUE == step)
				{
					continue;
				}

				break;
			}

			done |= check_ip(fd, ifr, ip);

			if((step = next_step(and, done, any, i, argc)))
			{

				if(CONTINUE == step)
				{
					continue;
				}

				break;
			}

			if(done)
			{
				iface->m_valid = 1;
			}

			// The next interface must also pass
			if((!any) && ((i+1) < argc))
			{
				done = 0;
			}
			else if(any && done)
			{
				break;
			}

		}

		if(done)
		{
			break;
		}

		if(retry >= 0)
		{

			if(!(retry--))
			{
				break;
			}

		}

		usleep(delay * 1000);
	}

	if(done)
	{
		int i;

		for(i = 0; i < max_ifr; ++i)
		{

			struct Interface* iface = ifr_list + i;

			if(!(iface->m_valid))
			{
				continue;
			}

			struct ifreq* ifr = &(iface->m_ifr);

			printf("%s%s", ifr->ifr_name, (mac || ip) ? "" : "\n");

			if(mac)
			{
				printf( ",mac=%02x:%02x:%02x:%02x:%02x:%02x%s",
					(unsigned char)(ifr->ifr_hwaddr.sa_data[0]),
					(unsigned char)(ifr->ifr_hwaddr.sa_data[1]),
					(unsigned char)(ifr->ifr_hwaddr.sa_data[2]),
					(unsigned char)(ifr->ifr_hwaddr.sa_data[3]),
					(unsigned char)(ifr->ifr_hwaddr.sa_data[4]),
					(unsigned char)(ifr->ifr_hwaddr.sa_data[5]),
					ip ? "" : "\n");
			}

			if(ip)
			{
				printf(",ip=%s,%s\n",
					inet_ntoa(((struct sockaddr_in *)&(ifr->ifr_addr))->sin_addr),
					(ifr->ifr_addr.sa_family == AF_INET) ? "AF_INET" :
						((ifr->ifr_addr.sa_family == AF_INET6) ? "AF_INET6" : "UNKNOWN"));
			}

		}

		return 0;
	}

	return 1;
}
