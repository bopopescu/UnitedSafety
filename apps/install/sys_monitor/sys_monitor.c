//#include <errno.h>
//#include <fcntl.h>
//#include <stdio.h>
#include <stdlib.h>
//#include <stddef.h>
//#include <string.h>
//#include <sys/types.h>
//#include <sys/ioctl.h>
//#include <sys/stat.h>
//#include <unistd.h>
#include "fw_env.h"

int sys_monitor(void)
{
	char *argv[4] = {"setenv", "fail_start", "no", NULL};
	return set_env(3, argv);
}
