/*
 * Copyright (C) 2010 Freescale Semiconductor, Inc.
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

#include <common.h>
#include <asm/arch/regs-pinctrl.h>
#include <asm/arch/pinctrl.h>
#include <asm/arch/regs-clkctrl.h>
#include <asm/arch/regs-ocotp.h>

#include <mmc.h>
#include <imx_ssp_mmc.h>

/* This should be removed after it's added into mach-types.h */
#ifndef MACH_TYPE_MX28EVK
#define MACH_TYPE_MX28EVK	2531
#endif

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_IMX_SSP_MMC

/* MMC pins */
static struct pin_desc mmc0_pins_desc[] = {
	{ PINID_SSP0_DATA0, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
	{ PINID_SSP0_DATA1, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
	{ PINID_SSP0_DATA2, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
	{ PINID_SSP0_DATA3, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
	{ PINID_SSP0_DATA4, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
	{ PINID_SSP0_DATA5, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
	{ PINID_SSP0_DATA6, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
	{ PINID_SSP0_DATA7, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
	{ PINID_SSP0_CMD, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
	{ PINID_SSP0_DETECT, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
	{ PINID_SSP0_SCK, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
};

static struct pin_desc mmc1_pins_desc[] = {
	{ PINID_GPMI_D00, PIN_FUN2, PAD_8MA, PAD_3V3, 1 },
	{ PINID_GPMI_D01, PIN_FUN2, PAD_8MA, PAD_3V3, 1 },
	{ PINID_GPMI_D02, PIN_FUN2, PAD_8MA, PAD_3V3, 1 },
	{ PINID_GPMI_D03, PIN_FUN2, PAD_8MA, PAD_3V3, 1 },
	{ PINID_GPMI_D04, PIN_FUN2, PAD_8MA, PAD_3V3, 1 },
	{ PINID_GPMI_D05, PIN_FUN2, PAD_8MA, PAD_3V3, 1 },
	{ PINID_GPMI_D06, PIN_FUN2, PAD_8MA, PAD_3V3, 1 },
	{ PINID_GPMI_D07, PIN_FUN2, PAD_8MA, PAD_3V3, 1 },
	{ PINID_GPMI_RDY1, PIN_FUN2, PAD_8MA, PAD_3V3, 1 },
	{ PINID_GPMI_RDY0, PIN_FUN2, PAD_8MA, PAD_3V3, 1 },
	{ PINID_GPMI_WRN, PIN_FUN2, PAD_8MA, PAD_3V3, 1 }
};

static struct pin_group mmc0_pins = {
	.pins		= mmc0_pins_desc,
	.nr_pins	= ARRAY_SIZE(mmc0_pins_desc)
};

static struct pin_group mmc1_pins = {
	.pins		= mmc1_pins_desc,
	.nr_pins	= ARRAY_SIZE(mmc1_pins_desc)
};

struct imx_ssp_mmc_cfg ssp_mmc_cfg[2] = {
	{REGS_SSP0_BASE, HW_CLKCTRL_SSP0, BM_CLKCTRL_CLKSEQ_BYPASS_SSP0},
	{REGS_SSP1_BASE, HW_CLKCTRL_SSP1, BM_CLKCTRL_CLKSEQ_BYPASS_SSP1},
};
#endif

/* ENET pins */
static struct pin_desc enet_pins_desc[] = {
	{ PINID_ENET0_MDC, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
	{ PINID_ENET0_MDIO, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
	{ PINID_ENET0_RX_EN, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
	{ PINID_ENET0_RXD0, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
	{ PINID_ENET0_RXD1, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
	{ PINID_ENET0_TX_EN, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
	{ PINID_ENET0_TXD0, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
	{ PINID_ENET0_TXD1, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
	{ PINID_ENET_CLK, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
	{ PINID_ENET0_RX_CLK, PIN_GPIO, PAD_8MA, PAD_3V3, 0 }
};

/* Gpmi pins */
static struct pin_desc gpmi_pins_desc[] = {
	{ PINID_GPMI_D00, PIN_FUN1, PAD_4MA, PAD_3V3, 0 },
	{ PINID_GPMI_D01, PIN_FUN1, PAD_4MA, PAD_3V3, 0 },
	{ PINID_GPMI_D02, PIN_FUN1, PAD_4MA, PAD_3V3, 0 },
	{ PINID_GPMI_D03, PIN_FUN1, PAD_4MA, PAD_3V3, 0 },
	{ PINID_GPMI_D04, PIN_FUN1, PAD_4MA, PAD_3V3, 0 },
	{ PINID_GPMI_D05, PIN_FUN1, PAD_4MA, PAD_3V3, 0 },
	{ PINID_GPMI_D06, PIN_FUN1, PAD_4MA, PAD_3V3, 0 },
	{ PINID_GPMI_D07, PIN_FUN1, PAD_4MA, PAD_3V3, 0 },
	{ PINID_GPMI_RDN, PIN_FUN1, PAD_8MA, PAD_1V8, 1 },
	{ PINID_GPMI_WRN, PIN_FUN1, PAD_4MA, PAD_3V3, 0 },
	{ PINID_GPMI_ALE, PIN_FUN1, PAD_4MA, PAD_3V3, 0 },
	{ PINID_GPMI_CLE, PIN_FUN1, PAD_4MA, PAD_3V3, 0 },
	{ PINID_GPMI_RDY0, PIN_FUN1, PAD_4MA, PAD_3V3, 0 },
	{ PINID_GPMI_RDY1, PIN_FUN1, PAD_4MA, PAD_3V3, 0 },
	{ PINID_GPMI_CE0N, PIN_FUN1, PAD_4MA, PAD_3V3, 0 },
	{ PINID_GPMI_CE1N, PIN_FUN1, PAD_4MA, PAD_3V3, 0 },
	{ PINID_GPMI_RESETN, PIN_FUN1, PAD_4MA, PAD_3V3, 0 }
};
static struct pin_group enet_pins = {
	.pins		= enet_pins_desc,
	.nr_pins	= ARRAY_SIZE(enet_pins_desc)
};
static struct pin_group gpmi_pins = {
	.pins		= gpmi_pins_desc,
	.nr_pins	= ARRAY_SIZE(gpmi_pins_desc)
};

#ifdef CONFIG_NAND_GPMI
void setup_gpmi_nand(void)
{
	/* Set up GPMI pins */
	pin_set_group(&gpmi_pins);
}
#endif
// Description: This function sets select i.MX28 pins to GPIO pins for reading as per
//	SVN Rev 1516: "RedStone/Documents/Specifications/M2M Board Connectors Pin Definition.xlsx", sheet "Rev 1 0".
//
//	Do NOT modify this function without also updating the document mentioned above.
void setup_redstone_gpio_pins(void)
{
	// clear all the leds
	pin_gpio_set(PINID_ENCODE(2,4), 0);
	pin_set_type(PINID_ENCODE(2,4), PIN_GPIO);
	pin_gpio_direction(PINID_ENCODE(2,4), 1);
	
  // turn on the Orange power LED to indicate boot
	pin_gpio_set(PINID_ENCODE(1,10), 1);
	pin_set_type(PINID_ENCODE(1,10), PIN_GPIO);
	pin_gpio_direction(PINID_ENCODE(1,10), 1);
	
	pin_gpio_set(PINID_ENCODE(1,11), 1);
	pin_set_type(PINID_ENCODE(1,11), PIN_GPIO);
	pin_gpio_direction(PINID_ENCODE(1,11), 1);
	
	// reenable the leds
	
//	pin_gpio_set(PINID_ENCODE(2,4), 1);
//	pin_set_type(PINID_ENCODE(2,4), PIN_GPIO);
//	pin_gpio_direction(PINID_ENCODE(2,4), 1);
	
	// ATS FIXME: Work-around for USB hotplug not working in the Kernel
	pin_gpio_set(PINID_ENCODE(2,20), 0);
	pin_set_type(PINID_ENCODE(2,20), PIN_GPIO);
	pin_gpio_direction(PINID_ENCODE(2,20), 1);

  // Fix for latching to off at startup - set gpio l (not L)
	pin_set_type(PINID_ENCODE(3,12), PIN_GPIO);
	pin_gpio_direction(PINID_ENCODE(3,12), 1);
	pin_gpio_set(PINID_ENCODE(3,12), 0);
	
	// disable the 3.3 and 5V lines
	pin_set_type(PINID_ENCODE(3,6), PIN_GPIO);
	pin_gpio_direction(PINID_ENCODE(3,6), 1);
	pin_gpio_set(PINID_ENCODE(3,6), 0);
	
	pin_set_type(PINID_ENCODE(3,7), PIN_GPIO);
	pin_gpio_direction(PINID_ENCODE(3,7), 1);
	pin_gpio_set(PINID_ENCODE(3,7), 0);
	// toggle latching to ensure that 3.3 and 5.0V lines come up
	pin_gpio_set(PINID_ENCODE(3,12), 1);
	udelay(50);
	pin_gpio_set(PINID_ENCODE(3,12), 0);

	#define J800(P_Bank, P_Pin, P_Row) \
		pin_set_type(PINID_ENCODE(P_Bank,P_Pin), PIN_GPIO)
	#define J801(P_Bank, P_Pin, P_Row) \
		pin_set_type(PINID_ENCODE(P_Bank,P_Pin), PIN_GPIO)
	// ATS XXX: All pins should be inputs by default, so no need to set direction.
	//          Direction is only set if output is required.
	//
	// ATS XXX: The below code matches with the diagram for this function (i.e. the gaps/spaces).
	// J800          |  J801              |  Pin
	//---------------+--------------------+-----
	J800(2, 16,  1);     J801(4,  5,  1); //  1
	J800(2, 27,  2);     J801(4, 15,  2); //  2
	J800(2, 18,  3);     J801(4, 13,  3); //  3
	J800(2, 25,  4);     J801(4,  4,  4); //  4
	J800(2, 24,  5);     J801(4,  1,  5); //  5
	J800(2, 17,  6);     J801(4,  3,  6); //  6
	J800(2, 26,  7);     J801(4,  0,  7); //  7
	J800(2, 12,  8);     J801(4,  8,  8); //  8
	J800(2,  3,  9);     J801(4,  6,  9); //  9
	/*J800(3, 16, 10);*/ J801(4,  7, 10); // 10   NOTE: DUART_RX is used by U-Boot, and is not needed for wakeup signalling
	J800(2, 10, 11);     J801(4,  2, 11); // 11
	J800(2, 19, 12);     J801(0, 22, 12); // 12
	                     J801(3, 18, 13); // 13
	J800(2,  8, 14);     J801(0, 23, 14); // 14
	                     J801(3,  4, 15); // 15
	J800(2,  5, 16);     J801(4, 16, 16); // 16
	J800(2, 20, 17);     J801(3,  5, 17); // 17
	J800(2,  7, 18);     J801(3,  6, 18); // 18
	J800(2,  0, 19);     J801(3, 27, 19); // 19
	                     J801(3,  7, 20); // 20
	J800(2,  1, 21);     J801(3, 21, 21); // 21
	J800(2,  9, 22);                      // 22
	                     J801(3, 22, 23); // 23
	J800(3,  1, 24);     J801(1,  4, 24); // 24
	J800(2,  2, 25);     J801(3, 20, 25); // 25
	J800(3,  9, 26);                      // 26
	                     J801(3, 23, 27); // 27
	J800(3,  8, 28);                      // 28
	J800(3,  0, 29);     J801(3, 26, 29); // 29
	J800(3, 24, 30);                      // 30
	J800(3,  2, 31);     J801(1,  3, 31); // 31
	J800(3, 25, 32);     J801(1,  0, 32); // 32
	J800(3,  3, 33);     J801(1,  1, 33); // 33
	/*J800(3, 17, 34);*/ J801(1,  2, 34); // 34   NOTE: DUART_TX is used by U-Boot, and is not needed for wakeup signalling
	                                      // 35
	                     J801(3, 15, 36); // 36
	                                      // 37
	                     J801(3, 14, 38); // 38
	                                      // 39
	                                      // 40
	#undef J800
	#undef J801
}

static void h_set_redstone_pin(unsigned char* p_des, int p_val, int p_row, int p_col)
{
	--p_row;
	--p_col;
	const int byte = p_row >> 3;
	const int shift = p_row - (byte << 3);

	p_des[byte + (p_col * 5)] |= (unsigned int)(p_val << shift);
}

unsigned char g_redstone_gpio_pins[10];
const int g_redstone_gpio_bytes = sizeof(g_redstone_gpio_pins);

void read_redstone_gpio_pins(unsigned char *p_des)
{
	#define J800(P_Bank, P_Pin, P_Row) \
		h_set_redstone_pin(p_des, pin_gpio_get(PINID_ENCODE(P_Bank, P_Pin)), P_Row, 1)
	#define J801(P_Bank, P_Pin, P_Row) \
		h_set_redstone_pin(p_des, pin_gpio_get(PINID_ENCODE(P_Bank, P_Pin)), P_Row, 2)
	// ATS XXX: The below code matches with the diagram for function "setup_redstone_gpio_pins".
	// J800          |  J801              |  Pin
	//---------------+--------------------+-----
	J800(2, 16,  1);     J801(4,  5,  1); //  1
	J800(2, 27,  2);     J801(4, 15,  2); //  2
	J800(2, 18,  3);     J801(4, 13,  3); //  3
	J800(2, 25,  4);     J801(4,  4,  4); //  4
	J800(2, 24,  5);     J801(4,  1,  5); //  5
	J800(2, 17,  6);     J801(4,  3,  6); //  6
	J800(2, 26,  7);     J801(4,  0,  7); //  7
	J800(2, 12,  8);     J801(4,  8,  8); //  8
	J800(2,  3,  9);     J801(4,  6,  9); //  9
	/*J800(3, 16, 10);*/ J801(4,  7, 10); // 10   NOTE: DUART_RX is used by U-Boot, and is not needed for wakeup signalling
	J800(2, 10, 11);     J801(4,  2, 11); // 11
	J800(2, 19, 12);     J801(0, 22, 12); // 12
	                     J801(3, 18, 13); // 13
	J800(2,  8, 14);     J801(0, 23, 14); // 14
	                     J801(3,  4, 15); // 15
	J800(2,  5, 16);     J801(4, 16, 16); // 16
	J800(2, 20, 17);     J801(3,  5, 17); // 17
	J800(2,  7, 18);     J801(3,  6, 18); // 18
	J800(2,  0, 19);     J801(3, 27, 19); // 19
	                     J801(3,  7, 20); // 20
	J800(2,  1, 21);     J801(3, 21, 21); // 21
	J800(2,  9, 22);                      // 22
	                     J801(3, 22, 23); // 23
	J800(3,  1, 24);     J801(1,  4, 24); // 24
	J800(2,  2, 25);     J801(3, 20, 25); // 25
	J800(3,  9, 26);                      // 26
	                     J801(3, 23, 27); // 27
	J800(3,  8, 28);                      // 28
	J800(3,  0, 29);     J801(3, 26, 29); // 29
	J800(3, 24, 30);                      // 30
	J800(3,  2, 31);     J801(1,  3, 31); // 31
	J800(3, 25, 32);     J801(1,  0, 32); // 32
	J800(3,  3, 33);     J801(1,  1, 33); // 33
	/*J800(3, 17, 34);*/ J801(1,  2, 34); // 34   NOTE: DUART_TX is used by U-Boot, and is not needed for wakeup signalling
	                                      // 35
	                     J801(3, 15, 36); // 36
	                                      // 37
	                     J801(3, 14, 38); // 38
	                                      // 39
	                                      // 40
	#undef J800
	#undef J801
}

/*
 * Functions
 */
int board_init(void)
{
	/* Enable watchdog */
	REG_SET_ADDR(REGS_RTC_BASE + 0, 1 << 4);
	REG_WR_ADDR(REGS_RTC_BASE + 0x50, 16000);

	/* Will change it for MX28 EVK later */
	gd->bd->bi_arch_number = MACH_TYPE_MX28EVK;
	/* Adress of boot parameters */
	gd->bd->bi_boot_params = PHYS_SDRAM_1 + 0x100;
#ifdef CONFIG_NAND_GPMI
	setup_gpmi_nand();
#endif
	return 0;
}

int dram_init(void)
{
	gd->bd->bi_dram[0].start = PHYS_SDRAM_1;
	gd->bd->bi_dram[0].size = PHYS_SDRAM_1_SIZE;

	return 0;
}

#ifdef CONFIG_IMX_SSP_MMC

#ifdef CONFIG_DYNAMIC_MMC_DEVNO
int get_mmc_env_devno()
{
	unsigned long global_boot_mode;

	global_boot_mode = REG_RD_ADDR(GLOBAL_BOOT_MODE_ADDR);
	return ((global_boot_mode & 0xf) == BOOT_MODE_SD1) ? 1 : 0;
}
#endif

#define PINID_SSP0_GPIO_WP PINID_SSP1_SCK
#define PINID_SSP1_GPIO_WP PINID_GPMI_RESETN

u32 ssp_mmc_is_wp(struct mmc *mmc)
{
	return (mmc->block_dev.dev == 0) ?
		pin_gpio_get(PINID_SSP0_GPIO_WP) :
		pin_gpio_get(PINID_SSP1_GPIO_WP);
}

int ssp_mmc_gpio_init(bd_t *bis)
{
	s32 status = 0;
	u32 index = 0;

	for (index = 0; index < CONFIG_SYS_SSP_MMC_NUM;
		++index) {
		switch (index) {
		case 0:
			/* Set up MMC pins */
			pin_set_group(&mmc0_pins);

			/* Power on the card slot 0 */
			pin_set_type(PINID_PWM3, PIN_GPIO);
			pin_gpio_direction(PINID_PWM3, 1);
			pin_gpio_set(PINID_PWM3, 0);

			/* Wait 10 ms for card ramping up */
			udelay(10000);

			/* Set up SD0 WP pin */
			pin_set_type(PINID_SSP0_GPIO_WP, PIN_GPIO);
			pin_gpio_direction(PINID_SSP0_GPIO_WP, 0);

			break;
		case 1:
#ifdef CONFIG_CMD_MMC
			/* Set up MMC pins */
			pin_set_group(&mmc1_pins);

			/* Power on the card slot 1 */
			pin_set_type(PINID_PWM4, PIN_GPIO);
			pin_gpio_direction(PINID_PWM4, 1);
			pin_gpio_set(PINID_PWM4, 0);

			/* Wait 10 ms for card ramping up */
			udelay(10000);

			/* Set up SD1 WP pin */
			pin_set_type(PINID_SSP1_GPIO_WP, PIN_GPIO);
			pin_gpio_direction(PINID_SSP1_GPIO_WP, 0);
#endif
			break;
		default:
			printf("Warning: you configured more ssp mmc controller"
				"(%d) as supported by the board(2)\n",
				CONFIG_SYS_SSP_MMC_NUM);
			return status;
		}
		status |= imx_ssp_mmc_initialize(bis, &ssp_mmc_cfg[index]);
	}

	return status;
}

int board_mmc_init(bd_t *bis)
{
	if (!ssp_mmc_gpio_init(bis))
		return 0;
	else
		return -1;
}

#endif

#ifdef CONFIG_MXC_FEC
#ifdef CONFIG_GET_FEC_MAC_ADDR_FROM_IIM
int fec_get_mac_addr(unsigned char *mac)
{
	u32 val;

	/*set this bit to open the OTP banks for reading*/
	REG_WR(REGS_OCOTP_BASE, HW_OCOTP_CTRL_SET,
		BM_OCOTP_CTRL_RD_BANK_OPEN);

	/*wait until OTP contents are readable*/
	while (BM_OCOTP_CTRL_BUSY & REG_RD(REGS_OCOTP_BASE, HW_OCOTP_CTRL))
		udelay(100);

	mac[0] = 0x00;
	mac[1] = 0x04;
	val = REG_RD(REGS_OCOTP_BASE, HW_OCOTP_CUSTn(0));
	mac[2] = (val >> 24) & 0xFF;
	mac[3] = (val >> 16) & 0xFF;
	mac[4] = (val >> 8) & 0xFF;
	mac[5] = (val >> 0) & 0xFF;
	return 0;
}
#endif
#endif

void enet_board_init(void)
{
	/* Set up ENET pins */
	pin_set_group(&enet_pins);

	/* This is now the cell emg reset - still needs to be set. */
	pin_set_type(PINID_SSP1_DATA3, PIN_GPIO);
	pin_gpio_direction(PINID_SSP1_DATA3, 1);
	pin_gpio_set(PINID_SSP1_DATA3, 0);

	/* Reset the external phy */
	pin_set_type(PINID_ENET0_RX_CLK, PIN_GPIO);
	pin_gpio_direction(PINID_ENET0_RX_CLK, 1);
	pin_gpio_set(PINID_ENET0_RX_CLK, 0);
	udelay(200);
	pin_gpio_set(PINID_ENET0_RX_CLK, 1);
}

