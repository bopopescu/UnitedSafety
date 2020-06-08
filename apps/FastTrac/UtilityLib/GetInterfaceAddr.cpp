#include <string>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <string.h>
#include <unistd.h>

std::string GetInterfaceAddr( const std::string &iface_name)
{
 int fd;
 struct ifreq ifr;

 fd = socket(AF_INET, SOCK_DGRAM, 0);

 /* I want to get an IPv4 IP address */
 ifr.ifr_addr.sa_family = AF_INET;

 /* I want IP address attached to "eth0" */
 strncpy(ifr.ifr_name, iface_name.c_str(), IFNAMSIZ-1);

 ioctl(fd, SIOCGIFADDR, &ifr);

 close(fd);

 /* display result */
 char buf[128];
 sprintf(buf, "%s\n", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
 return (std::string(buf));
}

std::string ReplaceLastOctet(const std::string strIPAddr, const int octet)
{
	char buf[64], *p;
	strcpy(buf, strIPAddr.c_str());
	p = strrchr(buf, '.');
	
	if (p == NULL)
	  return strIPAddr;
	*(++p) = '\0';
	sprintf(buf, "%s%d", buf, octet);
	std::string ret(buf);
	return ret;
}
