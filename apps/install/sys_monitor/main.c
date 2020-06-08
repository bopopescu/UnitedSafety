/*
 * Implements:
 * print_env [[ -n name ] | [ name ... ]]
 * set_env name [ value ... ]
 * sys_monitor
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fw_env.h"

extern int sys_monitor(void);

int g_print_def = 0;
int g_set_def = 0;

int main(int argc, char* argv[])
{
	char* cmdname = *argv;

	{
		char* p;

		if((p = strrchr (cmdname, '/')) != NULL)
		{
			cmdname = p + 1;
		}

	}

	if((strcmp(cmdname, "print_env") == 0) || (g_print_def = (strcmp(cmdname, "print_def") == 0)))
	{

		if(print_env(argc, argv) != 0)
		{
			return (EXIT_FAILURE);
		}

		return (EXIT_SUCCESS);
	}
	else if((strcmp(cmdname, "set_env") == 0) || (g_set_def = (strcmp(cmdname, "set_def") == 0)) )
	{

		if(set_env(argc, argv) != 0)
		{
			return (EXIT_FAILURE);
		}

		return (EXIT_SUCCESS);
	}
	else
	{
		if (sys_monitor() == 0)
			return (EXIT_SUCCESS);
	}

	return (EXIT_FAILURE);
}
