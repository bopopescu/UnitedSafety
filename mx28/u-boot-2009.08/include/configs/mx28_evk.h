/*
 * Copyright (C) 2010 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#ifndef __MX28_EVK_H
#define __MX28_EVK_H

#include <asm/arch/mx28.h>

#define CONFIG_SICONIX_UPGRADE_SYS
#define CONFIG_SYS_USE_NANDFLASH

/*
 * SoC configurations
 */
#define CONFIG_MX28				/* i.MX28 SoC */
#define CONFIG_MX28_TO1_2
#define CONFIG_SYS_HZ		1000		/* Ticks per second */
/* ROM loads UBOOT into DRAM */
#define CONFIG_SKIP_RELOCATE_UBOOT

/*
 * Memory configurations
 */
#define CONFIG_NR_DRAM_BANKS	1		/* 1 bank of DRAM */
#define PHYS_SDRAM_1		0x40000000	/* Base address */
#define PHYS_SDRAM_1_SIZE	0x08000000	/* 128 MB */
#define CONFIG_STACKSIZE	0x00020000	/* 128 KB stack */
#define CONFIG_SYS_MALLOC_LEN	0x00400000	/* 4 MB for malloc */
#define CONFIG_SYS_GBL_DATA_SIZE 128		/* Reserved for initial data */
#define CONFIG_SYS_MEMTEST_START 0x40000000	/* Memtest start address */
#define CONFIG_SYS_MEMTEST_END	 0x40400000	/* 4 MB RAM test */

/*
 * U-Boot general configurations
 */
#define CONFIG_SYS_LONGHELP
#define CONFIG_SYS_PROMPT	"RedStone U-Boot > "
#define CONFIG_SYS_CBSIZE	2048		/* Console I/O buffer size */
#define CONFIG_SYS_PBSIZE \
	(CONFIG_SYS_CBSIZE + sizeof(CONFIG_SYS_PROMPT) + 32)
						/* Print buffer size */
#define CONFIG_SYS_MAXARGS	32		/* Max number of command args */
#define CONFIG_SYS_BARGSIZE	CONFIG_SYS_CBSIZE
						/* Boot argument buffer size */
#define CONFIG_VERSION_VARIABLE			/* U-BOOT version */
#define CONFIG_AUTO_COMPLETE			/* Command auto complete */
#define CONFIG_CMDLINE_EDITING			/* Command history etc */

#define CONFIG_SYS_64BIT_VSPRINTF

/*
 * Boot Linux
 */
#define CONFIG_CMDLINE_TAG
#define CONFIG_SETUP_MEMORY_TAGS
#define CONFIG_BOOTDELAY	0
#define CONFIG_BOOTFILE		"uImage"

/*
 * (a, 0)  2M   at    0M - Freescale U-Boot (safeboot format)
 * (b, 1)  384K at 3072K - U-Boot environment (active)
 * (c, 2)  384K at 3456K - U-Boot environment (redundant)
 * (d, 3)  384K at 3840K - U-Boot environment (default)
 * (e, 4)  16M  at    5M - Non-volatile memory (NVRAM)
 * (f, 5)  3M   at   22M - Default (Install) Kernel
 * (g, 6)  3M   at   26M - Active Kernel
 * (h, 7)  8M   at   30M - Install Rootfs
 * (i, 8)  64M  at   39M - Default Rootfs
 * (j, 9)  64M  at  104M - Active Rootfs
 * (k, 10) 64M  at  169M - Work area
 * (l, 11) 22M  at  234M - Spare area
 */

#define TRULINK_MTDPARTS \
	" mtdparts=gpmi-nfc-main:" \
		"2M(a)"\
		",384K@3072K(b)"\
		",384K@3456K(c)"\
		",384K@3840K(d)"\
		",16M@5M(e)"\
		",3M@22M(f)"\
		",3M@26M(g)"\
		",8M@30M(h)"\
		",64M@39M(i)"\
		",64M@104M(j)"\
		",64M@169M(k)"\
		",22M@234M(l)"

#define CONFIG_BOOTARGS		"console=ttyAM0,115200n8" \
				" root=ubi0" \
				TRULINK_MTDPARTS \
					" rw" \
					" rootfstype=ubifs" \
				" gpmi"

#define CONFIG_BOOTCOMMAND	"if fixture;then run fixture_bootcmd;else run trulink_bootcmd;fi"
#define CONFIG_LOADADDR		0x42000000
#define CONFIG_SYS_LOAD_ADDR	CONFIG_LOADADDR
#define CONFIG_MEM_CPY_ADD	CONFIG_LOADADDR

/*
 * Extra Environments
 */
#define	CONFIG_EXTRA_ENV_SETTINGS \
	"def_linux_add=0x1600000\0" \
	"def_linux_size=0x300000\0" \
	"linux_add=0x1A00000\0" \
	"linux_size=0x300000\0" \
	"def_fs_add=0x2700000\0" \
	"def_fs_size=0x4000000\0" \
	"fs_add=0x6800000\0" \
	"fs_size=0x4000000\0" \
	"fs_part=8\0" \
	"trulink_bootcmd=run redstone_args;nand read 42000000 1A00000 ${linux_size};bootm\0" \
	"fixture_bootcmd=dhcp;run bootargs_nfs;bootm\0" \
	"bootfile=trulink_fixture.fw\0"\
	"redstone_args=setenv bootargs ${bootargs} ubi.mtd=${fs_part} rshw=\"${rshw}\" j800=\"${j800}\" j801=\"${j801}\"\0" \
	"nfsroot=/srv/rootfs-trulink-testmode/\0" \
	"bootargs_nfs=setenv bootargs " \
		"console=ttyAM0,115200n8 " \
		"root=/dev/nfs " \
		"ip=dhcp nfsroot=${serverip}:${nfsroot},v3,tcp " \
		"fec_mac=${ethaddr} gpmi " TRULINK_MTDPARTS "\0" \
/*
 * U-Boot Commands
 */
#define CONFIG_SYS_NO_FLASH
#include <config_cmd_default.h>
#define CONFIG_ARCH_CPU_INIT
#define CONFIG_DISPLAY_CPUINFO

/*
 * Serial Driver
 */
#define CONFIG_UARTDBG_CLK		24000000
#define CONFIG_BAUDRATE			115200		/* Default baud rate */
#define CONFIG_SYS_BAUDRATE_TABLE	{ 9600, 19200, 38400, 57600, 115200 }

/*
 * FEC Driver
 */
#define CONFIG_MXC_FEC
#define CONFIG_GET_FEC_MAC_ADDR_FROM_IIM
#define CONFIG_FEC0_IOBASE		REGS_ENET_BASE
#define CONFIG_FEC0_PHY_ADDR		0
#define CONFIG_NET_MULTI
#define CONFIG_ETH_PRIME
#define CONFIG_RMII
#define CONFIG_CMD_MII
#define CONFIG_CMD_DHCP
#define CONFIG_CMD_PING
#define CONFIG_IPADDR
#define CONFIG_SERVERIP			192.168.28.99
#define CONFIG_NETMASK
/* Add for working with "strict" DHCP server */
#define CONFIG_BOOTP_SUBNETMASK
#define CONFIG_BOOTP_GATEWAY
#define CONFIG_BOOTP_DNS

/*
 * MMC Driver
 */
//#define CONFIG_CMD_MMC

#ifdef CONFIG_CMD_MMC
	#define CONFIG_MMC
	#define CONFIG_IMX_SSP_MMC		/* MMC driver based on SSP */
	#define CONFIG_GENERIC_MMC
	#define CONFIG_DYNAMIC_MMC_DEVNO
	#define CONFIG_DOS_PARTITION
	#define CONFIG_CMD_FAT
	#define CONFIG_SYS_SSP_MMC_NUM 2
#endif

/*
 * GPMI Nand Configs
 */
#ifndef CONFIG_CMD_MMC	/* NAND conflict with MMC */

#define CONFIG_CMD_NAND

#ifdef CONFIG_CMD_NAND
	#define CONFIG_NAND_GPMI
	#define CONFIG_GPMI_NFC_SWAP_BLOCK_MARK
	#define CONFIG_GPMI_NFC_V1

	#define CONFIG_GPMI_REG_BASE	GPMI_BASE_ADDR
	#define CONFIG_BCH_REG_BASE	BCH_BASE_ADDR

	#define NAND_MAX_CHIPS		8
	#define CONFIG_SYS_NAND_BASE		0x40000000
	#define CONFIG_SYS_MAX_NAND_DEVICE	1
#endif

/*
 * APBH DMA Configs
 */
#define CONFIG_APBH_DMA

#ifdef CONFIG_APBH_DMA
	#define CONFIG_APBH_DMA_V1
	#define CONFIG_MXS_DMA_REG_BASE ABPHDMA_BASE_ADDR
#endif

#endif

/*
 * Environments
 */
//#define CONFIG_FSL_ENV_IN_MMC
#define CONFIG_FSL_ENV_IN_NAND

#define CONFIG_CMD_ENV
#define CONFIG_ENV_OVERWRITE

#if defined(CONFIG_FSL_ENV_IN_NAND)
	#define CONFIG_ENV_IS_IN_NAND 1
	#define CONFIG_ENV_OFFSET		0x300000 /* Nand env, offset: 3M */
	#define CONFIG_ENV_OFFSET_REDUND	0x360000 /* Nand redundant */
	#define CONFIG_DEF_ENV_OFFSET		0x3C0000 /* Nand default */
	#define CONFIG_ENV_PART_SIZE		 0xC0000 /* size of two env partitions */
	#define CONFIG_ENV_SECT_SIZE    (128 * 1024)
	#define CONFIG_ENV_SIZE         CONFIG_ENV_SECT_SIZE
#elif defined(CONFIG_FSL_ENV_IN_MMC)
	#define CONFIG_ENV_IS_IN_MMC	1
	/* Assoiated with the MMC layout defined in mmcops.c */
	#define CONFIG_ENV_OFFSET               (0x400) /* 1 KB */
	#define CONFIG_ENV_SIZE                 (0x20000 - 0x400) /* 127 KB */
#else
	#define CONFIG_ENV_IS_NOWHERE	1
#endif

/* The global boot mode will be detected by ROM code and
 * a boot mode value will be stored at fixed address:
 * TO1.0 addr 0x0001a7f0
 * TO1.2 addr 0x00019BF0
 */
#ifndef MX28_EVK_TO1_0
 #define GLOBAL_BOOT_MODE_ADDR 0x00019BF0
#else
 #define GLOBAL_BOOT_MODE_ADDR 0x0001a7f0
#endif
#define BOOT_MODE_SD0 0x9
#define BOOT_MODE_SD1 0xa

/*
 * Enable the watchdog
 */
#define CONFIG_HW_WATCHDOG  1

#define CONFIG_SYS_HUSH_PARSER 1
#define CONFIG_SYS_PROMPT_HUSH_PS2 "TRULink> "

#endif /* __MX28_EVK_H */
