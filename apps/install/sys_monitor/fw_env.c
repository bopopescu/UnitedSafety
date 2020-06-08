#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <mtd/mtd-user.h>

#include "fw_env.h"

typedef struct envdev_s {
	char  devname[16];		/* Device name */
	ulong devoff;			/* Device offset */
} envdev_t;

static envdev_t envdevices[2];
#define DEVNAME(i)    envdevices[(i)].devname
#define DEVOFFSET(i)  envdevices[(i)].devoff

static int curdev;
static env_t environment;

extern int g_print_def;
extern int g_set_def;
static int g_using_default_environment = 0;
static int g_ignore_crc = 0;

/*
 * s1 is either a simple 'name', or a 'name=value' pair.
 * i2 is the environment index for a 'name=value' pair.
 * If the names match, return the value of s2, else NULL.
 */

static int envmatch (unsigned char* s1, int i2)
{

	while(*s1 == environment.data[i2++])
	{

		if (*s1++ == '=')
		{
			return (i2);
		}

	}

	if((*s1 == '\0') && (environment.data[i2-1] == '='))
	{
		return i2;
	}

	return -1;
}

#if defined(CONFIG_FILE)
static int get_config(char* fname)
{
	FILE* fp;
	int i = 0;
	int rc;

	if((fp = fopen (fname, "r")) == NULL)
	{
		return errno;
	}

	while ((i < 2) && ((rc = fscanf (fp, "%s %lx",
				  DEVNAME (i), &DEVOFFSET (i) )) != EOF)) {

		/* Skip incomplete conversions and comment strings */
		char dump[128];

		if ((rc < 1) || (*DEVNAME (i) == '#'))
		{
			fgets (dump, sizeof (dump), fp); /* Consume till end */
			continue;
		}

		i++;
	}
	fclose (fp);

	if(i<2) /* No enough valid entries found */
	{
		return (errno = EINVAL);
	}

	return 0;
}
#endif

static int readenv (unsigned char * buf)
{
	int fd;

	if ((fd = open(DEVNAME(curdev), O_RDONLY)) < 0) {
		fprintf (stderr, "Can't open %s: %s\n", DEVNAME(curdev), strerror(errno));
		return 1;
	}
		if (lseek(fd, DEVOFFSET(curdev), SEEK_SET) == -1) {
		fprintf (stderr, "seek error on %s: %s\n", DEVNAME(curdev), strerror(errno));
		goto exit;
	}
	if (read(fd, buf, CFG_ENV_SIZE) != CFG_ENV_SIZE) {
		fprintf (stderr, "read error on %s: %s\n", DEVNAME(curdev), strerror (errno));
		goto exit;
	}

	if (close(fd)) {
		fprintf (stderr, "I/O error on %s: %s\n", DEVNAME(curdev), strerror (errno));
		return 1;
	}

	/* everything ok */
	return 0;

exit:
	if (close (fd)) {
		fprintf (stderr, "I/O error on %s: %s\n", DEVNAME(curdev), strerror(errno));
	}

	return 1;
}

static int parse_config(int p_default_env)
{
	struct stat st;

	if(p_default_env)
	{
		strcpy(DEVNAME(0), "/dev/mtd3");
		DEVOFFSET(0) = 0;
	}
	else
	{
		const int ret = get_config(CONFIG_FILE);

		if(ret)
		{

			if(ENOENT != ret)
			{
				fprintf(stderr, "Cannot parse config file: %s\n", strerror(ret));
			}

			strcpy (DEVNAME(0), DEVICE1_NAME);
			DEVOFFSET(0) = DEVICE1_OFFSET;
			strcpy (DEVNAME(1), DEVICE2_NAME);
			DEVOFFSET(1) = DEVICE2_OFFSET;
		}

	}

	if(stat(DEVNAME(0), &st))
	{
		fprintf(stderr, "Cannot access MTD device %s: %s\n", DEVNAME(0), strerror(errno));
		return 1;
	}

	if(!p_default_env)
	{

		if(stat(DEVNAME(1), &st))
		{
			fprintf(stderr, "Cannot access MTD device %s: %s\n", DEVNAME(1), strerror (errno));
			return 1;
		}

	}

	return 0;
}

static int def_env_init()
{
	curdev = 0;
	int error = 1;
	env_t* env = calloc(1, CFG_ENV_SIZE);

	if(!env)
	{
		error = errno;
		fprintf (stderr, "Not enough memory for environment (%d bytes)\n", CFG_ENV_SIZE);
		return error;
	}

	if(readenv((unsigned char*)env) == 0)
	{

		if(crc32(0, (uint8_t*)env->data, ENV_SIZE) == env->crc)
		{
			memcpy(&environment, env, CFG_ENV_SIZE);
			error = 0;
		}
		else if(g_ignore_crc)
		{
			env->data[0] = '\0';
			error = 0;
		}
		else
		{
			fprintf (stderr, "Warning: Bad CRC, no valid environment\n");
		}

	}

	free(env);
	return error;
}

/*
 * Prevent confusion if running from erased flash memory
 */
static int env_init(void)
{
	int crc1_ok = 0;
	int crc2_ok = 0;
	env_t* tmp_env1 = 0;
	env_t* tmp_env2 = 0;
	g_using_default_environment = g_print_def || g_set_def;

	if(parse_config(g_using_default_environment)) /* should fill envdevices */
	{
		return 1;
	}

	g_ignore_crc = (getenv("SYS_MONITOR_IGNORE_CRC") != NULL);

	if(g_using_default_environment)
	{
		return def_env_init();
	}

	if ((tmp_env1 = calloc(1, CFG_ENV_SIZE)) == NULL)
	{
		fprintf (stderr, "Not enough memory for environment 1 (%d bytes)\n", CFG_ENV_SIZE);
		return (errno);
	}

	if((tmp_env2 = calloc(1, CFG_ENV_SIZE)) == NULL)
	{
		fprintf (stderr, "Not enough memory for environment 2 (%d bytes)\n", CFG_ENV_SIZE);
		return (errno);
	}

	/* read environment from FLASH to local buffer */
	curdev = 0;

	if (readenv((unsigned char*) tmp_env1) == 0)
	{
		crc1_ok = crc32 (0, (uint8_t *) tmp_env1->data, ENV_SIZE) == tmp_env1->crc;
	}

	curdev = 1;

	if (readenv((unsigned char*) tmp_env2) == 0)
	{
		crc2_ok = crc32 (0, (uint8_t *) tmp_env2->data, ENV_SIZE) == tmp_env2->crc;
	}

	if (!crc1_ok && !crc2_ok)
	{
		curdev = 0;

		if(!g_ignore_crc)
		{
			fprintf (stderr, "Error: Bad CRC, no valid environment\n");
			free(tmp_env1);
			free(tmp_env2);
			return 1;
		}

	}
	else if (crc1_ok && !crc2_ok)
	{
		curdev = 0;
	}
	else if (!crc1_ok && crc2_ok)
	{
		curdev = 1;
	}
	else
	{ // both ok - check flags
		if (tmp_env1->flags == 0 && tmp_env2->flags == 0xFF)
			curdev = 0;
		else if (tmp_env1->flags == 0xFF && tmp_env2->flags == 0)
			curdev = 1;
		else if (tmp_env1->flags > tmp_env2->flags)
			curdev = 0;
		else if (tmp_env2->flags > tmp_env1->flags)
			curdev = 1;
		else /* tmp_env1->flags == tmp_env2->flags */
			curdev = 0;
		//printf("flag1 %d, flag2 %d, curdev %d\n", tmp_env1->flags, tmp_env2->flags, curdev);
	}

	if(curdev)
	{
		memcpy(&environment, tmp_env2, CFG_ENV_SIZE);
	}
	else
	{
		memcpy(&environment, tmp_env1, CFG_ENV_SIZE);
	}

	free(tmp_env1);
	free(tmp_env2);
	return 0;
}


/*
 * Print the current definition of one, or more, or all
 * environment variables
 */
int print_env (int argc, char *argv[])
{
	unsigned char *env, *nxt;
	int i, n_flag;
	int rc = 0;

	if (env_init ())
		return (-1);

	if (argc == 1) {		/* Print all env variables  */
		for (env = environment.data; *env; env = nxt + 1) {
			for (nxt = env; *nxt; ++nxt) {
				if (nxt >= &environment.data[ENV_SIZE]) {
					fprintf (stderr, "## Error: environment not terminated\n");
					return (-1);
				}
			}

			printf ("%s\n", env);
		}
		return (0);
	}

	if (strcmp (argv[1], "-n") == 0) {
		n_flag = 1;
		++argv;
		--argc;
		if (argc != 2) {
			fprintf (stderr, "## Error: `-n' option requires exactly one argument\n");
			return (-1);
		}
	} else {
		n_flag = 0;
	}

	for (i = 1; i < argc; ++i) {	/* print single env variables   */
		char *name = argv[i];
		unsigned char* env_data = environment.data;
		int val = -1;

		for (env=env_data; *env; env=nxt+1) {
			for (nxt=env; *nxt; ++nxt)
				;
			val = envmatch ((unsigned char *)name, env-env_data);
			if (val >= 0) {
				if (!n_flag) {
					fputs (name, stdout);
					putc ('=', stdout);
				}
				puts ((char *)env);
				break;
			}
		}
		if (!val) {
			fprintf (stderr, "## Error: \"%s\" not defined\n", name);
			rc = -1;
		}
	}

	return (rc);
}

static int saveenv (void)
{
	int fd;
	erase_info_t erase;

	if (DEVICE_ESIZE != CFG_ENV_SIZE)
	{
		fprintf(stderr, "Device size not equal ENV_SIZE\n");
		return 1;
	}

	environment.flags ++;

	if(!g_using_default_environment)
	{
		curdev = !curdev; /* switch to next partition for writing */
	}

	if((fd = open(DEVNAME(curdev), O_RDWR)) < 0)
	{
		fprintf(stderr, "Can't open %s: %s\n", DEVNAME(curdev), strerror(errno));
		return 1;
	}

	printf ("Unlocking flash...");
	erase.length = DEVICE_ESIZE;
	erase.start = DEVOFFSET(curdev);
	ioctl (fd, MEMUNLOCK, &erase);

	printf("Done\nErasing flash...");
	erase.length = DEVICE_ESIZE;
	erase.start = DEVOFFSET(curdev);
	ioctl (fd, MEMERASE, &erase);
	printf ("Done\nWriting environment to %s...", DEVNAME(curdev));

	if (lseek (fd, DEVOFFSET(curdev), SEEK_SET) == -1)
	{
		fprintf (stderr, "seek error on %s: %s\n", DEVNAME(curdev), strerror(errno));
		goto lockexit;
	}

	if (write (fd, &environment, CFG_ENV_SIZE) != CFG_ENV_SIZE)
	{
		fprintf (stderr, "error on %s\n", strerror(errno));
		goto lockexit;
	}

	printf("Done\nLocking ...");
	erase.length = DEVICE_ESIZE;
	erase.start = DEVOFFSET(curdev);
	ioctl (fd, MEMLOCK, &erase);
	printf("Done\n");
	return 0;

lockexit:
	erase.length = DEVICE_ESIZE;
	erase.start = DEVOFFSET(curdev);
	ioctl (fd, MEMLOCK, &erase);

	if(close (fd))
	{
		fprintf(stderr, "I/O error on %s: %s\n", DEVNAME(curdev), strerror(errno));
	}

	return 1;
}


/*
 * Deletes or sets environment variables. Returns errno style error codes:
 * 0	  - OK
 * EINVAL - need at least 1 argument
 * EROFS  - certain variables ("ethaddr", "serial#") cannot be
 *	    modified or deleted
 *
 */
int set_env (int argc, char *argv[])
{
	int i, len, oldval;
	unsigned char *env, *nxt = NULL;
	char *name;
	unsigned char *env_data;
	const char* env_fname = getenv("SYS_MONITOR_DEF_ENV_FILE");

	if((argc < 2) && (!env_fname))
	{
		return EINVAL;
	}

	if(env_init())
	{
		return (errno);
	}

	env_data = environment.data;

	if(env_fname)
	{
		memset(env_data, 0xff, ENV_SIZE);
		char buf[16384];
		char* line = buf;
		char* s = (char*)env_data;
		FILE* f = fopen(env_fname, "r");

		for(;f;)
		{
			char c;
			const size_t nread = fread(&c, 1, 1, f);

			if(!nread)
			{
				break;
			}

			if(('\r' == c) || ('\n' == c))
			{

				if(buf[0])
				{
					*line = '\0';
					strcpy(s, buf);
					s += ((line + 1) - buf);
				}

				line = buf;
				line[0] = '\0';
				continue;
			}

			*(line++) = c;
		}

		s[0] = '\0';

		if(f)
		{
			fclose(f);
		}

		goto WRITE_FLASH;
	}

	name = argv[1];

	if(strchr(name, '='))
	{
		fprintf (stderr, "## Error: illegal character '=' in variable name \"%s\"\n", name);
		return 1;
	}

	/*
	 * search if variable with this name already exists
	 */
	oldval = -1;
	for (env=env_data; *env; env=nxt+1) {
		for (nxt=env; *nxt; ++nxt)
			;
		if ((oldval = envmatch((unsigned char *)name, env-env_data)) >= 0)
			break;
	}

	/*
	 * Delete any existing definition
	 */
	if (oldval >= 0) {
		/*
		 * Ethernet Address and serial# can be set only once
		 */
		if ((strcmp (name, "ethaddr") == 0) ||
			(strcmp (name, "serial#") == 0)) {
			fprintf (stderr, "Can't overwrite \"%s\"\n", name);
			return (EROFS);
		}

		if (*++nxt == '\0') {
			if (env > env_data) {
				env--;
			} else {
				*env = '\0';
			}
		} else {
			for (;;) {
				*env = *nxt++;
				if ((*env == '\0') && (*nxt == '\0'))
					break;
				++env;
			}
		}
		*++env = '\0';
	}

	/* Delete only ? */
	if (argc < 3)
		goto WRITE_FLASH;

	/*
	 * Append new definition at the end
	 */
	for (env = env_data; *env || *(env+1); ++env);
	if (env > env_data)
		++env;
	/*
	 * Overflow when:
	 * "name" + "=" + "val" +"\0\0"  > CFG_ENV_SIZE - (env-environment)
	 */
	len = strlen (name) + 2;
	/* add '=' for first arg, ' ' for all others */
	for (i=2; i<argc; ++i) {
		len += strlen (argv[i]) + 1;
	}

	if (len > (&env_data[ENV_SIZE]-env))
	{
		fprintf (stderr, "Error: environment overflow, \"%s\" deleted\n", name);
		return -1;
	}

	while ((*env = *name++) != '\0')
	{
		env++;
	}

	for (i=2; i<argc; ++i)
	{
		char *val = argv[i];

		*env = (i==2) ? '=' : ' ';
		while ((*++env = *val++) != '\0');
	}

	/* end is marked with double '\0' */
	*++env = '\0';

  WRITE_FLASH:

	/* Update CRC */
	environment.crc = crc32 (0, (uint8_t*) environment.data, ENV_SIZE);

	/* write environment back to flash */
	if(saveenv())
	{
		fprintf (stderr, "Error: can't write fw_env to flash\n");
		return -1;
	}

	return 0;
}
