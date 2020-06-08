#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/inotify.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <pthread.h>
#include <semaphore.h>
#include <linux/if.h>
#include <linux/wireless.h>

const char* get_ra0_essid()
{
	const int fd = socket(AF_INET, SOCK_DGRAM, 0);
	struct iwreq wrq;
	static char essid[32];
	memset(&wrq, 0, sizeof(struct iwreq));
	strcpy(wrq.ifr_ifrn.ifrn_name, "ra0");
	wrq.u.data.length = 32;
	wrq.u.data.pointer = essid;
	wrq.u.data.flags = 0;
	ioctl(fd, SIOCGIWESSID, &wrq);
	close(fd);
	return essid;
}
