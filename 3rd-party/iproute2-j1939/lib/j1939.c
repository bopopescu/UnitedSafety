#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <endian.h>
#include <linux/can/j1939.h>

#include "utils.h"

#ifndef htobe64
# if __BYTE_ORDER == __LITTLE_ENDIAN
#  define htobe64(x) __bswap_64 (x)
#  define htole64(x) (x)
#  define be64toh(x) __bswap_64 (x)
#  define le64toh(x) (x)
# else
#  define htobe64(x) (x)
#  define htole64(x) __bswap_64 (x)
#  define be64toh(x) (x)
#  define le64toh(x) __bswap_64 (x)
# endif
#endif
/*
 * print J1939 name
 * for use from rt_addr_n2a
 */
const char *j1939_ntop(int af, const void *vaddr, size_t vlen,
		char *str, size_t len)
{
	struct rtattr *tb[IFA_J1939_MAX];
	int strdone = 0;

	/* cast vaddr to non-const pointer */
	parse_rtattr(tb, IFA_J1939_MAX-1, (void *)vaddr, vlen);
	if (tb[IFA_J1939_ADDR]) {
		strdone += sprintf(&str[strdone], "0x%02x",
				*(uint8_t *)RTA_DATA(tb[IFA_J1939_ADDR]));
		if (tb[IFA_J1939_NAME])
			str[strdone++] = ' ';
	}
	if (tb[IFA_J1939_NAME])
		strdone += sprintf(&str[strdone], "name %016llx",
				(unsigned long long)be64toh(*(uint64_t *)RTA_DATA(tb[IFA_J1939_NAME])));
	errno = 0;
	return str;
}

/*
 * fill an ifaddr message from program arguments
 */
int j1939_addr_args(int argc, char *argv[], struct nlmsghdr *msg, int msg_size)
{
	int saved_argc = argc;
	struct ifaddrmsg *ifa = (void *)&msg[1];
	struct rtattr *local;

	if (ifa->ifa_family == AF_UNSPEC)
		ifa->ifa_family = AF_CAN;
	else {
		fprintf(stderr, "j1939 only allowed for AF_CAN\n");
		return -1;
	}
	if (!ifa->ifa_prefixlen)
		ifa->ifa_prefixlen = CAN_J1939;
	else {
		fprintf(stderr, "CAN protocol %i already specified",
				ifa->ifa_prefixlen);
		return -1;
	}
	NEXT_ARG();
	/* j1939 SA & NAME never need to be specified together */
	if (matches(*argv, "name") == 0) {
		uint64_t name;

		NEXT_ARG();
		name = htobe64(strtoull(*argv, 0, 16));
		if (!name) {
			fprintf(stderr, "0 name is not valid\n");
			return -1;
		}
		local = addattr_nest(msg, msg_size, IFA_LOCAL);
		addattr_l(msg, msg_size, IFA_J1939_NAME, &name, sizeof(name));
		addattr_nest_end(msg, local);
	} else {
		unsigned int laddr;
		uint8_t addr;

		addr = laddr = strtoul(*argv, 0, 0);
		if (laddr >= 0xfe) {
			fprintf(stderr, "address '%s' not valid\n", *argv);
			return -1;
		}
		local = addattr_nest(msg, msg_size, IFA_LOCAL);
		addattr_l(msg, msg_size, IFA_J1939_ADDR, &addr, sizeof(addr));
		addattr_nest_end(msg, local);
	}

	return saved_argc - argc;
}

/*
 * fill an link_af message from program arguments
 */
int j1939_link_args(int argc, char *argv[], struct nlmsghdr *msg, int msg_size)
{
	int saved_argc = argc;
	struct rtattr *afspec, *can, *j1939;
	uint8_t enable;

	NEXT_ARG();
	if (strcmp(*argv, "on") == 0) {
		enable = 1;
	} else if (strcmp(*argv, "off") == 0) {
		enable = 0;
	} else {
		enable = 1;
		/* revert arguments */
		++argc;
		--argv;
	}

	afspec = addattr_nest(msg, msg_size, IFLA_AF_SPEC);
	can = addattr_nest(msg, msg_size, AF_CAN);
	j1939 = addattr_nest(msg, msg_size, CAN_J1939);
	addattr_l(msg, msg_size, IFLA_J1939_ENABLE, &enable, sizeof(enable));
	addattr_nest_end(msg, j1939);
	addattr_nest_end(msg, can);
	addattr_nest_end(msg, afspec);

	return saved_argc - argc;
}

/*
 * process the returned IFLA_AF_SPEC/AF_CAN/CAN_J1939 attribute
 */
const char *j1939_link_attrtop(struct rtattr *nla)
{
	static char str[32];
	int pos;
	struct rtattr *tb[IFLA_J1939_MAX];

	pos = 0;
	str[0] = 0;
	parse_rtattr_nested(tb, IFLA_J1939_MAX-1, nla);
	if (tb[IFLA_J1939_ENABLE]) {
		uint8_t *u8ptr;

		u8ptr = RTA_DATA(tb[IFLA_J1939_ENABLE]);
		pos += sprintf(&str[pos], "j1939 %s", *u8ptr ? "on" : "off");
	}
	return str;
}
