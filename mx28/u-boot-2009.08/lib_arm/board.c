/*
 * (C) Copyright 2002-2006
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/*
 * To match the U-Boot user interface on ARM platforms to the U-Boot
 * standard (as on PPC platforms), some messages with debug character
 * are removed from the default U-Boot build.
 *
 * Define DEBUG here if you want additional info as shown below
 * printed upon startup:
 *
 * U-Boot code: 00F00000 -> 00F3C774  BSS: -> 00FC3274
 * IRQ Stack: 00ebff7c
 * FIQ Stack: 00ebef7c
 */

#include <common.h>
#include <command.h>
#include <malloc.h>
#include <stdio_dev.h>
#include <timestamp.h>
#include <version.h>
#include <net.h>
#include <serial.h>
#include <nand.h>
#include <onenand_uboot.h>
#include <mmc.h>

#ifdef CONFIG_DRIVER_SMC91111
#include "../drivers/net/smc91111.h"
#endif
#ifdef CONFIG_DRIVER_LAN91C96
#include "../drivers/net/lan91c96.h"
#endif

#ifdef CONFIG_SICONIX_UPGRADE_SYS
#include <environment.h>
#include <watchdog.h>
#endif /* CONFIG_SICONIX_UPGRADE_SYS */

DECLARE_GLOBAL_DATA_PTR;

ulong monitor_flash_len;

#ifdef CONFIG_HAS_DATAFLASH
extern int  AT91F_DataflashInit(void);
extern void dataflash_print_info(void);
#endif

#ifdef CONFIG_SICONIX_UPGRADE_SYS
extern int do_reset (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);
#endif

#ifndef CONFIG_IDENT_STRING
#define CONFIG_IDENT_STRING ""
#endif

const char version_string[] =
	U_BOOT_VERSION" (" U_BOOT_DATE " - " U_BOOT_TIME ")"CONFIG_IDENT_STRING;

#ifdef CONFIG_DRIVER_CS8900
extern void cs8900_get_enetaddr (void);
#endif

#ifdef CONFIG_DRIVER_RTL8019
extern void rtl8019_get_enetaddr (uchar * addr);
#endif

#if defined(CONFIG_HARD_I2C) || \
    defined(CONFIG_SOFT_I2C)
#include <i2c.h>
#endif

/*
 * Begin and End of memory area for malloc(), and current "brk"
 */
static ulong mem_malloc_start = 0;
static ulong mem_malloc_end = 0;
static ulong mem_malloc_brk = 0;

static
void mem_malloc_init (ulong dest_addr)
{
	mem_malloc_start = dest_addr;
	mem_malloc_end = dest_addr + CONFIG_SYS_MALLOC_LEN;
	mem_malloc_brk = mem_malloc_start;

	memset ((void *) mem_malloc_start, 0,
			mem_malloc_end - mem_malloc_start);
}

void *sbrk (ptrdiff_t increment)
{
	ulong old = mem_malloc_brk;
	ulong new = old + increment;

	if ((new < mem_malloc_start) || (new > mem_malloc_end)) {
		return (NULL);
	}
	mem_malloc_brk = new;

	return ((void *) old);
}


/************************************************************************
 * Coloured LED functionality
 ************************************************************************
 * May be supplied by boards if desired
 */
void inline __coloured_LED_init (void) {}
void coloured_LED_init(void)__attribute__((weak, alias("__coloured_LED_init")));
void inline __red_LED_on (void) {}
void red_LED_on(void) __attribute__((weak, alias("__red_LED_on")));
void inline __red_LED_off(void) {}
void red_LED_off(void)       __attribute__((weak, alias("__red_LED_off")));
void inline __green_LED_on(void) {}
void green_LED_on(void) __attribute__((weak, alias("__green_LED_on")));
void inline __green_LED_off(void) {}
void green_LED_off(void) __attribute__((weak, alias("__green_LED_off")));
void inline __yellow_LED_on(void) {}
void yellow_LED_on(void) __attribute__((weak, alias("__yellow_LED_on")));
void inline __yellow_LED_off(void) {}
void yellow_LED_off(void) __attribute__((weak, alias("__yellow_LED_off")));
void inline __blue_LED_on(void) {}
void blue_LED_on(void) __attribute__((weak, alias("__blue_LED_on")));
void inline __blue_LED_off(void) {}
void blue_LED_off(void) __attribute__((weak, alias("__blue_LED_off")));

/************************************************************************
 * Init Utilities							*
 ************************************************************************
 * Some of this code should be moved into the core functions,
 * or dropped completely,
 * but let's get it working (again) first...
 */

#if defined(CONFIG_ARM_DCC) && !defined(CONFIG_BAUDRATE)
#define CONFIG_BAUDRATE 115200
#endif
static int init_baudrate (void)
{
	char tmp[64];	/* long enough for environment variables */
	int i = getenv_r ("baudrate", tmp, sizeof (tmp));
	gd->bd->bi_baudrate = gd->baudrate = (i > 0)
			? (int) simple_strtoul (tmp, NULL, 10)
			: CONFIG_BAUDRATE;

	return (0);
}

static int display_banner (void)
{
	printf ("\n\n%s\n\n", version_string);
	debug ("U-Boot code: %08lX -> %08lX  BSS: -> %08lX\n",
	       _armboot_start, _bss_start, _bss_end);
#ifdef CONFIG_MODEM_SUPPORT
	debug ("Modem Support enabled\n");
#endif
#ifdef CONFIG_USE_IRQ
	debug ("IRQ Stack: %08lx\n", IRQ_STACK_START);
	debug ("FIQ Stack: %08lx\n", FIQ_STACK_START);
#endif

	return (0);
}

/*
 * WARNING: this code looks "cleaner" than the PowerPC version, but
 * has the disadvantage that you either get nothing, or everything.
 * On PowerPC, you might see "DRAM: " before the system hangs - which
 * gives a simple yet clear indication which part of the
 * initialization if failing.
 */
static int display_dram_config (void)
{
	int i;

#ifdef DEBUG
	puts ("RAM Configuration:\n");

	for(i=0; i<CONFIG_NR_DRAM_BANKS; i++) {
		printf ("Bank #%d: %08lx ", i, gd->bd->bi_dram[i].start);
		print_size (gd->bd->bi_dram[i].size, "\n");
	}
#else
	ulong size = 0;

	for (i=0; i<CONFIG_NR_DRAM_BANKS; i++) {
		size += gd->bd->bi_dram[i].size;
	}
	puts("DRAM:  ");
	print_size(size, "\n");
#endif

	return (0);
}

#ifndef CONFIG_SYS_NO_FLASH
static void display_flash_config (ulong size)
{
	puts ("Flash: ");
	print_size (size, "\n");
}
#endif /* CONFIG_SYS_NO_FLASH */

#if defined(CONFIG_HARD_I2C) || defined(CONFIG_SOFT_I2C)
static int init_func_i2c (void)
{
	puts ("I2C:   ");
	i2c_init (CONFIG_SYS_I2C_SPEED, CONFIG_SYS_I2C_SLAVE);
	puts ("ready\n");
	return (0);
}
#endif

#if defined(CONFIG_CMD_PCI) || defined (CONFIG_PCI)
#include <pci.h>
static int arm_pci_init(void)
{
	pci_init();
	return 0;
}
#endif /* CONFIG_CMD_PCI || CONFIG_PCI */

/*
 * Breathe some life into the board...
 *
 * Initialize a serial port as console, and carry out some hardware
 * tests.
 *
 * The first part of initialization is running from Flash memory;
 * its main purpose is to initialize the RAM so that we
 * can relocate the monitor code to RAM.
 */

/*
 * All attempts to come up with a "common" initialization sequence
 * that works for all boards and architectures failed: some of the
 * requirements are just _too_ different. To get rid of the resulting
 * mess of board dependent #ifdef'ed code we now make the whole
 * initialization sequence configurable to the user.
 *
 * The requirements for any new initalization function is simple: it
 * receives a pointer to the "global data" structure as it's only
 * argument, and returns an integer return code, where 0 means
 * "continue" and != 0 means "fatal error, hang the system".
 */
typedef int (init_fnc_t) (void);

int print_cpuinfo (void);

init_fnc_t *init_sequence[] = {
#if defined(CONFIG_ARCH_CPU_INIT)
	arch_cpu_init,		/* basic arch cpu dependent setup */
#endif
	board_init,		/* basic board dependent setup */
#if defined(CONFIG_USE_IRQ)
	interrupt_init,		/* set up exceptions */
#endif
	timer_init,		/* initialize timer */
	env_init,		/* initialize environment */
	init_baudrate,		/* initialze baudrate settings */
	serial_init,		/* serial communications setup */
	console_init_f,		/* stage 1 init of console */
	display_banner,		/* say that we are here */
#if defined(CONFIG_DISPLAY_CPUINFO)
	print_cpuinfo,		/* display cpu info (and speed) */
#endif
#if defined(CONFIG_DISPLAY_BOARDINFO)
	checkboard,		/* display board info */
#endif
#if defined(CONFIG_HARD_I2C) || defined(CONFIG_SOFT_I2C)
	init_func_i2c,
#endif
	dram_init,		/* configure available RAM banks */
#if defined(CONFIG_CMD_PCI) || defined (CONFIG_PCI)
	arm_pci_init,
#endif
	display_dram_config,
	NULL,
};

#ifdef CONFIG_SICONIX_UPGRADE_SYS
static int get_env_val(char * name, ulong *value)
{
	char tmp[64];
	int i;

	/* evalute env name */
	i = getenv_r (name, tmp, sizeof (tmp));
	if (i <= 0) /* env name not set */
		return (1);

	*value = simple_strtoul(tmp, NULL, 0);
	return (0);
}

#ifdef CONFIG_SYS_USE_NANDFLASH
static int erase_nand(nand_info_t *nand, ulong off, ulong length)
{
	nand_erase_options_t opts;

	memset(&opts, 0, sizeof(opts));
	opts.offset = off;
	opts.length = length;
	if (nand_erase_opts(nand, &opts))
		return 1;

	return 0;
}
#endif /* CONFIG_SYS_USE_NANDFLASH */

static void system_recovery(void)
{
#ifdef CONFIG_SYS_USE_NANDFLASH
	ulong fs_add=0,fs_size=0, def_fs_add=0, def_fs_size=0;
	ulong linux_add=0, linux_size=0, def_linux_add=0, def_linux_size=0;
	nand_info_t *nand = &nand_info[nand_curr_device];
	size_t size;
#endif /* CONFIG_SYS_USE_NANDFLASH */

	puts("####   WARNING: SYSTEM FAILURE   ####\n");
	puts("#### system recovery in progress ####\n");
	puts("####     DO NOT REMOVE POWER     ####\n");

#ifdef CONFIG_LCD
	/* display upgrade warning to LCD, WATCHDOG_RESET inside */
	drv_lcd_warning();
#endif

#ifdef CONFIG_SYS_USE_NANDFLASH
	/* restore system from factory default, WATCHDOG_RESET inside
	erase two environment partitions */
	if (erase_nand(nand, CONFIG_ENV_OFFSET, CONFIG_ENV_PART_SIZE))
	{
		puts("NAND erase error, resetting board...\n");
		do_reset(NULL, 0, 0, NULL); /* reset to retry */
	}

	env_relocate();

getval:
	WATCHDOG_RESET();
	/* retrieve partition table info */
	if (get_env_val("fs_add", &fs_add) ||
		get_env_val("fs_size", &fs_size) ||
		get_env_val("def_fs_add", &def_fs_add) ||
		get_env_val("def_fs_size", &def_fs_size) ||
		get_env_val("linux_add", &linux_add) ||
		get_env_val("linux_size", &linux_size) ||
		get_env_val("def_linux_add", &def_linux_add) ||
		get_env_val("def_linux_size", &def_linux_size))
	{
		puts("ERROR: partition table incomplete, load default env.\n");
		set_default_env();
		saveenv();
		goto getval;
	}

	/* more value checking, ? check address block aligned */
	if (!fs_add || !fs_size || !def_fs_add || !def_fs_size || 
		!linux_add || !linux_size || !def_linux_add || !def_linux_size ||
		def_fs_size > fs_size || def_linux_size > linux_size)
	{
		puts("ERROR: invalid partition table, load default env.\n");
		set_default_env();
		saveenv();
		goto getval;
	}

// File system restore by partition block-copy not supported for UBIFS.
#if 0
	puts("Restoring file system ...\n");
	/* erase active file system partition, WATCHDOG_RESET inside
	? erase clean with jffs2 cleanmarker */
	if (erase_nand(nand, fs_add, fs_size))
	{
		puts("NAND erase error at active file system partition.\n");
		/* env partitions was erased fine but active file system partition errors
		something must be wrong, display error */
		goto error;
	}
	/* restore active file system partition, WATCHDOG_RESET inside */
	size = (size_t)def_fs_size;
	if (nand_copy_skip_bad(nand, def_fs_add, &size, fs_add))
	{
		puts("ERROR: file system partition recovery failure.\n");
		goto error;
	}
#endif
	puts("Restoring linux kernel ...\n");
	/* erase active kernel partition, WATCHDOG_RESET inside */
	if (erase_nand(nand, linux_add, linux_size))
	{
		puts("NAND erase error at active Linux partition.\n");
		/* env and fs partitions were erased fine but active Linux partition errors
		something must be wrong, display error */
		goto error;
	}
	/* restore active Linux partition, WATCHDOG_RESET inside */
	size = (size_t)def_linux_size;
	if (nand_copy_skip_bad(nand, def_linux_add, &size, linux_add))
	{
		puts("ERROR: Linux partition recovery failure.\n");
		goto error;
	}

	/* everything is done, reset to start new system */
	puts("####  System recovery succeeded! ####\n");
	do_reset(NULL, 0, 0, NULL);

error:
	/* upgrade failure to serial */
	puts("####   ERROR ERROR ERROR !!!!!        ####\n");
	puts("####   SYSTEM RECOVERY FAILURE        ####\n");
	puts("#### Contact support@absolutetrac.com ####\n");
	puts("####   or call +1(403)252-8522        ####\n");

#ifdef CONFIG_LCD
	drv_lcd_sys_error();	/* upgrade failure to LCD */
#endif

	/*endless loop */
	for (;;)
	{
		ulong start = get_timer(0);
		WATCHDOG_RESET();
		/* sleep for 10 seconds */
		while (get_timer(start) < 10) {
			udelay(1000*1000);
		}
	}
#endif /* CONFIG_SYS_USE_NANDFLASH */
}

#define STARTUP_FAILURE		"yes"
#define UPGRADE_DISABLE_KEY	"SiconixMagicKey2009"
void review_env (void)
{
	int i;
	ulong start_good, fail_cnt, fail_max;
	char tmp[64];

	i = getenv_r ("upgrade_off", tmp, sizeof (tmp));
	if (i > 0 && strcmp(UPGRADE_DISABLE_KEY, tmp) == 0)
		return;

	/* evaluate fail_start */
	start_good = 1;
	i = getenv_r ("fail_start", tmp, sizeof (tmp));
	if (i > 0 && strcmp(STARTUP_FAILURE, tmp) == 0)
		start_good = 0;

	/* evalute fail_max */
	i = get_env_val("fail_max", &fail_max);
	if ( i > 0 || fail_max < 1)
	{
		setenv("fail_max", "0x05");
		fail_max = 0x05;
	}

	/* evaluate fail_cnt */
	i = get_env_val("fail_cnt", &fail_cnt);
	if ( i > 0 || start_good)
	{
		setenv("fail_cnt", "0x0");
		fail_cnt = 0;
	}
	else /* last startup fail and fail_cnt valid */
	{
		fail_cnt ++;
		if (fail_cnt >= fail_max)
		{
			system_recovery();
			/* system_recovery shall reset processor at the end and shall not return here. */
		}
	}

	sprintf(tmp, "0x%lx", fail_cnt);
	setenv("fail_cnt", tmp);
	setenv("fail_start", STARTUP_FAILURE);
	saveenv();
}
#endif /* CONFIG_SICONIX_UPGRADE_SYS */

extern unsigned char g_redstone_gpio_pins[10];

void setup_redstone_gpio_pins(void);
void read_redstone_gpio_pins(unsigned char *p_des);

void start_armboot (void)
{
	init_fnc_t **init_fnc_ptr;
	char *s;
#if defined(CONFIG_VFD) || defined(CONFIG_LCD)
	unsigned long addr;
#endif
	/* Disable PSWITCH */
	REG_SET_ADDR(0x80056060, 0x1 << 16);
	REG_SET_ADDR(0x80056060, 0x1 << 17);

	/* Disable all power-off paths except for the watchdog */
	REG_WR_ADDR(REGS_POWER_BASE + 0x100, (0x3E77<<16) | 0x6);

	setup_redstone_gpio_pins();
	read_redstone_gpio_pins(g_redstone_gpio_pins);

	/* Pointer is writable since we allocated a register for it */
	gd = (gd_t*)(_armboot_start - CONFIG_SYS_MALLOC_LEN - sizeof(gd_t));
	/* compiler optimization barrier needed for GCC >= 3.4 */
	__asm__ __volatile__("": : :"memory");

	memset ((void*)gd, 0, sizeof (gd_t));
	gd->bd = (bd_t*)((char*)gd - sizeof(bd_t));
	memset (gd->bd, 0, sizeof (bd_t));

	gd->flags |= GD_FLG_RELOC;

	monitor_flash_len = _bss_start - _armboot_start;

	for (init_fnc_ptr = init_sequence; *init_fnc_ptr; ++init_fnc_ptr) {
		if ((*init_fnc_ptr)() != 0) {
			hang ();
		}
	}

	/* armboot_start is defined in the board-specific linker script */
	mem_malloc_init (_armboot_start - CONFIG_SYS_MALLOC_LEN);

#ifndef CONFIG_SYS_NO_FLASH
	/* configure available FLASH banks */
	display_flash_config (flash_init ());
#endif /* CONFIG_SYS_NO_FLASH */

#ifdef CONFIG_VFD
#	ifndef PAGE_SIZE
#	  define PAGE_SIZE 4096
#	endif
	/*
	 * reserve memory for VFD display (always full pages)
	 */
	/* bss_end is defined in the board-specific linker script */
	addr = (_bss_end + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1);
	vfd_setmem (addr);
	gd->fb_base = addr;
#endif /* CONFIG_VFD */

#ifdef CONFIG_LCD
	/* board init may have inited fb_base */
	if (!gd->fb_base) {
#		ifndef PAGE_SIZE
#		  define PAGE_SIZE 4096
#		endif
		/*
		 * reserve memory for LCD display (always full pages)
		 */
		/* bss_end is defined in the board-specific linker script */
		addr = (_bss_end + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1);
		lcd_setmem (addr);
		gd->fb_base = addr;
	}
#endif /* CONFIG_LCD */

#if defined(CONFIG_CMD_NAND)
	puts ("NAND:  ");
	nand_init();		/* go init the NAND */
#endif

#if defined(CONFIG_CMD_ONENAND)
	onenand_init();
#endif

#ifdef CONFIG_HAS_DATAFLASH
	AT91F_DataflashInit();
	dataflash_print_info();
#endif

#ifdef CONFIG_GENERIC_MMC
	puts ("MMC:   ");
	mmc_initialize (gd->bd);
#endif

	/* initialize environment */
	env_relocate ();

#ifdef CONFIG_VFD
	/* must do this after the framebuffer is allocated */
	drv_vfd_init();
#endif /* CONFIG_VFD */

#ifdef CONFIG_SERIAL_MULTI
	serial_initialize();
#endif

	/* IP Address */
	gd->bd->bi_ip_addr = getenv_IPaddr ("ipaddr");

	stdio_init ();	/* get the devices list going. */

#ifdef CONFIG_SICONIX_UPGRADE_SYS
	/* now, LCD should be ready to use */
	review_env();
#endif /* CONFIG_SICONIX_UPGRADE_SYS */

	jumptable_init ();

#if defined(CONFIG_API)
	/* Initialize API */
	api_init ();
#endif

	console_init_r ();	/* fully init console as a device */

#if defined(CONFIG_ARCH_MISC_INIT)
	/* miscellaneous arch dependent initialisations */
	arch_misc_init ();
#endif
#if defined(CONFIG_MISC_INIT_R)
	/* miscellaneous platform dependent initialisations */
	misc_init_r ();
#endif

	/* enable exceptions */
	enable_interrupts ();

	/* Perform network card initialisation if necessary */
#ifdef CONFIG_DRIVER_TI_EMAC
	/* XXX: this needs to be moved to board init */
extern void davinci_eth_set_mac_addr (const u_int8_t *addr);
	if (getenv ("ethaddr")) {
		uchar enetaddr[6];
		eth_getenv_enetaddr("ethaddr", enetaddr);
		davinci_eth_set_mac_addr(enetaddr);
	}
#endif

#ifdef CONFIG_DRIVER_CS8900
	/* XXX: this needs to be moved to board init */
	cs8900_get_enetaddr ();
#endif

#if defined(CONFIG_DRIVER_SMC91111) || defined (CONFIG_DRIVER_LAN91C96)
	/* XXX: this needs to be moved to board init */
	if (getenv ("ethaddr")) {
		uchar enetaddr[6];
		eth_getenv_enetaddr("ethaddr", enetaddr);
		smc_set_mac_addr(enetaddr);
	}
#endif /* CONFIG_DRIVER_SMC91111 || CONFIG_DRIVER_LAN91C96 */

#if defined(CONFIG_ENC28J60_ETH) && !defined(CONFIG_ETHADDR)
	extern void enc_set_mac_addr (void);
	enc_set_mac_addr ();
#endif /* CONFIG_ENC28J60_ETH && !CONFIG_ETHADDR*/

	/* Initialize from environment */
	if ((s = getenv ("loadaddr")) != NULL) {
		load_addr = simple_strtoul (s, NULL, 16);
	}
#if defined(CONFIG_CMD_NET)
	if ((s = getenv ("bootfile")) != NULL) {
		copy_filename (BootFile, s, sizeof (BootFile));
	}
#endif

#ifdef BOARD_LATE_INIT
	board_late_init ();
#endif

#ifdef CONFIG_ANDROID_RECOVERY
	check_recovery_mode();
#endif

#if defined(CONFIG_CMD_NET)
#if defined(CONFIG_NET_MULTI)
	puts ("Net:   ");
#endif
	eth_initialize(gd->bd);
#if defined(CONFIG_RESET_PHY_R)
	debug ("Reset Ethernet PHY\n");
	reset_phy();
#endif
#endif

	{
		unsigned char * g = g_redstone_gpio_pins;
		char buf[64];
		sprintf(buf, "%02X,%02X,%02X,%02X,%02X", g[0], g[1], g[2], g[3], g[4]);
		setenv("j800", buf);

		sprintf(buf, "%02X,%02X,%02X,%02X,%02X", g[5], g[6], g[7], g[8], g[9]);
		setenv("j801", buf);
	}
	{
		const char svn_version_string[] = SVN_VERSION;
		char buf[16];
		sprintf(buf, "%s", svn_version_string);
		setenv("uboot_ver", buf);
	}

	/* main_loop() can return to retry autoboot, if so just run it again. */
	for (;;) {
		main_loop ();
	}

	/* NOTREACHED - no way out of command loop except booting */
}

void hang (void)
{
	puts ("### ERROR ### Please RESET the board ###\n");
	for (;;);
}
