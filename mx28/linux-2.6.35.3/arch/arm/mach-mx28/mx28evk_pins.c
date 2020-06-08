/*
 * Copyright (C) 2009-2010 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */


//---------------------------------------------------------------------
//
//  Modified Nov 2016 for the TL5000.  Most ifdef configuration options
//  have been removed and hard coded for the TL5000 
//  Dave Huff Nov 2016
//

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#include <mach/pinctrl.h>

#include "mx28_pins.h"

#define GPIO_OUTPUT 	.fun	= PIN_GPIO, \
		.output 	= 1, \
		.strength	= PAD_4MA, \
		.voltage	= PAD_3_3V, 

#define GPIO_INPUT 		.fun	= PIN_GPIO, \
		.output 	= 0, \
		.strength	= PAD_4MA, \
		.voltage	= PAD_3_3V, 



static struct pin_desc mx28evk_fixed_pins[] = 
{
	// FROM TruLink Combined schematics iMX28 Processor V3.0 page Block U75C - in order of appearance on the page
	{
		.name     = "GPMI RST-",
		.id       = PINID_GPMI_RESETN,
		.fun      = PIN_FUN1,
		.strength = PAD_12MA,
		.voltage  = PAD_3_3V,
		.pullup   = 0,
		.drive    = !0
	 },
	{
		.name     = "GPMI CLE",
		.id       = PINID_GPMI_CLE,
		.fun      = PIN_FUN1,
		.strength = PAD_4MA,
		.voltage  = PAD_3_3V,
		.pullup   = 0,
		.drive    = !0
	 },
	{
		.name     = "GPMI ALE",
		.id       = PINID_GPMI_ALE,
		.fun      = PIN_FUN1,
		.strength = PAD_4MA,
		.voltage  = PAD_3_3V,
		.pullup   = 0,
		.drive    = !0
	 },
	{
		.name     = "GPMI WR-",
		.id       = PINID_GPMI_WRN,
		.fun      = PIN_FUN1,
		.strength = PAD_12MA,
		.voltage  = PAD_3_3V,
		.pullup   = 0,
		.drive    = !0
	 },
	{
		.name     = "GPMI RD-",
		.id       = PINID_GPMI_RDN,
		.fun      = PIN_FUN1,
		.strength = PAD_12MA,
		.voltage  = PAD_3_3V,
		.pullup   = 0,
		.drive    = !0
	 },


	{
		.name	= "CAN0_RX",
		.id	= PINID_GPMI_RDY3,
		.fun	= PIN_FUN2,
		.strength	= PAD_4MA,
		.voltage	= PAD_3_3V,
		.pullup	= 0,
		.drive 	= 1,
		.pull 		= 0,
	 },
	{
		.name	= "CAN0_TX",
		.id	= PINID_GPMI_RDY2,
		.fun	= PIN_FUN2,
		.strength	= PAD_4MA,
		.voltage	= PAD_3_3V,
		.pullup	= 0,
		.drive 	= 1,
		.pull 		= 0,
	 },
	{
		.name	= "SSP1_CMD",
		.id	= PINID_GPMI_RDY1,
		.fun	= PIN_FUN2,
		.strength	= PAD_8MA,
		.voltage	= PAD_3_3V,
		.pullup	= 1,
		.drive 	= 1,
		.pull 		= 1,
	 },
	{
		.name	= "SSP1_DETECT",
		.id	= PINID_GPMI_RDY0,
		.fun	= PIN_FUN1,
		.strength	= PAD_8MA,
		.voltage	= PAD_3_3V,
		.pullup	= 0,
		.drive 	= 1,
		.pull 		= 0,
	 },
	{
		.name     = "GPMI CE1-",
		.id       = PINID_GPMI_CE1N,
		.fun      = PIN_FUN1,
		.strength = PAD_4MA,
		.voltage  = PAD_3_3V,
		.pullup   = 0,
		.drive    = !0
	 },
	{
		.name     = "GPMI CE0-",
		.id       = PINID_GPMI_CE0N,
		.fun      = PIN_FUN1,
		.strength = PAD_4MA,
		.voltage  = PAD_3_3V,
		.pullup   = 0,
		.drive    = !0
	 },
  // see mx28evk_gpmi_pins[] for GPMI definitions.
	{
		.name	= "RESET_BUTTON",
		.id	= PINID_LCD_D12,
		.pullup	= 1,
		GPIO_INPUT
	},
/*
	{
		.name	= "IridSats",  // Iridium sats in view
		.id	= PINID_SSP0_DATA0,
		GPIO_INPUT
	 },
	*/
	{
		.name	= "EN_PER_OUT",  // Enable Peripheral Power - active high
		.id	= PINID_SSP0_DATA3,
		GPIO_OUTPUT
		.data = 0,  // default to high output
	},
	{
		.name	= "DEN",
		.id	= PINID_SSP0_DATA5,
		GPIO_OUTPUT
	 },
	{// Control RS485/RS232 pin
		.name	= "RS485",
		.id	= PINID_SSP0_DATA7,
			GPIO_OUTPUT
		.data = 0,  // default to high output
	 },
	{
		.name	= "SSP0_DETECT",
		.id	= PINID_SSP0_DETECT,
		GPIO_OUTPUT
	},
	{
		.name	= "AccelInt",  // Accelerometer interrupt
		.id	= PINID_SSP1_SCK,
		GPIO_INPUT
	},
	{
		.name	= "CAN_PWDN",
		.id	= PINID_SSP1_CMD,
		GPIO_OUTPUT
		.data = 0,  // default to high output
	 },
	{
		.name	= "CellPwrGood",  // input indicating cell has good power.
		.id	= PINID_SSP1_DATA0,
		GPIO_INPUT
	 },
	{
		.name	= "CellEmergRst",
		.id	= PINID_SSP1_DATA3,
		GPIO_OUTPUT
		.data = 0,
	 },

	{
		.name	= "EnableCell",
		.id	= PINID_SSP2_SCK,
		GPIO_OUTPUT
		.data = 1,
	},
	{
		.name	= "CellOn",
		.id	= PINID_SSP2_MOSI,
		GPIO_OUTPUT
		.data = 0,
	 },
	{
		.name  = "AUART3.RX",
		.id    = PINID_SSP2_MISO,
		.fun   = PIN_FUN2,
	},
	{
		.name  = "AUART3.TX",
		.id    = PINID_SSP2_SS0,
		.fun   = PIN_FUN2,
	},
	{
		.name	= "USB0Enable",
		.id	= PINID_SSP2_SS1,
		GPIO_OUTPUT
		.data = 0,
	},
	{
		.name	= "USBHubRst",
		.id	= PINID_SSP2_SS2,
		GPIO_OUTPUT
		.data = 0,
	},
	{
		.name  = "AUART4.TX",
		.id    = PINID_SSP3_SCK,
		.fun   = PIN_FUN2,
	},
	{
		.name  = "AUART4.RX",
		.id    = PINID_SSP3_MOSI,
		.fun   = PIN_FUN2,
	},
	{
		.name  = "IridiumEnable",
		.id    = PINID_SSP3_MISO,
		GPIO_OUTPUT
		.data = 0,
	},
	{
		.name  = "ZigbeeReset",
		.id    = PINID_SSP3_SS0,
		GPIO_OUTPUT
		.data = 0,
	},

	// FROM TruLink Combined schematics iMX28 Processor page Block U75B - in order of appearance on the page
	{
		.name = "I2C0_SCL",
		.id = PINID_I2C0_SCL,
		.fun = PIN_FUN1,
		.strength = PAD_8MA,
		.voltage = PAD_3_3V,
		.drive	= 1,
	},
	{
		.name = "I2C0_SDA",
		.id = PINID_I2C0_SDA,
		.fun = PIN_FUN1,
		.strength = PAD_8MA,
		.voltage = PAD_3_3V,
		.drive	= 1,
	},

	{
		.name  = "AUART0.RX",
		.id    = PINID_AUART0_RX,
		.fun   = PIN_FUN1,
		.strength = PAD_8MA,
		.voltage = PAD_3_3V,
	 },
	{
		.name  = "AUART0.TX",
		.id    = PINID_AUART0_TX,
		.fun   = PIN_FUN1,
		.strength = PAD_8MA,
		.voltage = PAD_3_3V,
	},
	{
		.name		= "WiFiEnable",
		.id		= PINID_AUART0_CTS,
		.fun	= PIN_GPIO,
		.strength	= PAD_4MA,
		.voltage	= PAD_3_3V,
		.output = 0,
	},
	{
		.name		= "INT_IO",
		.id		= PINID_AUART0_RTS,
		GPIO_INPUT
	},

	{
		.name  = "AUART1.RX",
		.id    = PINID_AUART1_RX,
		.fun   = PIN_FUN1,
		.strength = PAD_8MA,
		.voltage = PAD_3_3V,
	},
	{
		.name  = "AUART1.TX",
		.id    = PINID_AUART1_TX,
		.fun   = PIN_FUN1,
		.strength = PAD_8MA,
		.voltage = PAD_3_3V,
	},
	{
		.name 	= "3VEnable",
		.id 	= PINID_AUART1_CTS,
		GPIO_OUTPUT
		.data 	= 1,
	},
	{
		.name 	= "5VEnable",
		.id 	= PINID_AUART1_RTS,
		GPIO_OUTPUT
		.data 	= 1,
	},

	{
		.name  = "AUART2.RX",
		.id    = PINID_AUART2_RX,
		.fun   = PIN_FUN1,
	},
	{
		.name  = "AUART2.TX",
		.id    = PINID_AUART2_TX,
		.fun   = PIN_FUN1,
		.strength = PAD_8MA,
		.voltage = PAD_3_3V,
	},
	{
		.name  = "GPSAntPwr",  // Power to GPS Antenna - off by default 
		.id    = PINID_AUART2_RTS,
		GPIO_OUTPUT
		.data = 0,
	},

	{
		.name  = "AUART3.RX",  // this is CPU off (set-gpio 'L')
		.id    = PINID_AUART3_RX,
		GPIO_OUTPUT
		.data = 0,
	},
	{
		.name  = "AUART3.CTS",  // CAN1_TX
		.id    = PINID_AUART3_CTS,
		.fun   = PIN_FUN2,
		.strength = PAD_8MA,
		.voltage = PAD_3_3V,
	},
	{
		.name  = "AUART3.RTS",  // CAN1_RX
		.id    = PINID_AUART3_RTS,
		.fun   = PIN_FUN2,
		.strength = PAD_8MA,
		.voltage = PAD_3_3V,
	},

  // see mx28evk_eth_pins[] for ENET0 definitions.

	{
		.name = "DUART.RX",
		.id = PINID_PWM0,
		.fun = PIN_FUN3,
		.strength = PAD_8MA,
		.voltage = PAD_3_3V,
	},
	{
		.name = "DUART.TX",
		.id = PINID_PWM1,
		.fun = PIN_FUN3,
		.strength = PAD_8MA,
		.voltage = PAD_3_3V,
	},
	{
		.name = "RXEN",// Control RS485/RS232 RXEN pin
		.id = PINID_PWM2,
		GPIO_OUTPUT
		.data	= 1,
	}, 

	{
		.name	= "SAIF0_MCLK",
		.id	= PINID_SAIF0_MCLK,
		GPIO_INPUT
	},
/*  not used
	{
		.name = "SAIF0_LRCLK",
		.id = PINID_SAIF0_LRCLK,
		.fun = PIN_FUN1,
		.strength = PAD_12MA,
		.voltage = PAD_3_3V,
		.pull = 1,
		.pullup = 1,
		.drive	= 1,
	},
	{
		.name	= "SAIF0_BITCLK",
		.id	= PINID_SAIF0_BITCLK,
		.fun	= PIN_FUN1,
		.strength	= PAD_12MA,
		.voltage	= PAD_3_3V,
		.pullup	= 1,
		.drive 	= 1,
		.pull 		= 1,
	},
*/
	// FROM TruLink Combined schematics iMX28 Processor page Block U75D - in order of appearance on the page

	{
		.name	= "LCD_RS",
		.id	= PINID_LCD_RS,
		.fun	= PIN_FUN1,
		.strength	= PAD_12MA,
		.voltage	= PAD_3_3V,
		.pullup	= 1,
		.drive 	= 1,
		.pull 	= 1,
	},
	{
		.name	= "PowerDownNotify",
		.id	= PINID_LCD_D14,
		GPIO_INPUT
	},
	{
		.name	= "PowerLEDGreen",
		.id	= PINID_LCD_D11,
		GPIO_OUTPUT
		.data = 1,
	},
	{
		.name	= "PowerLEDRed",
		.id	= PINID_LCD_D10,
		GPIO_OUTPUT
		.data = 1,
	},
	// NOTE remaining pins in block (LCD_D00 to LCD_D06) are for boot loader - do not initialize or enable.

	// FROM TruLink Combined schematics iMX28 Processor page Block U75D - in order of appearance on the page
	//
	// setup for lens
	{
		.name	= "ISC_SPI_CS",
		.id	= PINID_SSP0_DATA1,
		GPIO_OUTPUT
			.data = 1,  // default to high output
	},
	{
		.name	= "ISC_SPI_SCK",
		.id	= PINID_SSP0_SCK,
		.fun   = PIN_FUN1,
		.strength = PAD_8MA,
		.voltage = PAD_3_3V,
		.drive = 1,
	},
	{
		.name	= "ISC_SPI_MOSI",
		.id	= PINID_SSP0_CMD,
		.fun   = PIN_FUN1,
		.strength = PAD_8MA,
		.voltage = PAD_3_3V,
		.drive = 1,
	},
	{
		.name	= "ISC_SPI_MISO",
		.id    = PINID_SSP0_DATA0,
		.fun   = PIN_FUN1,
		.strength = PAD_8MA,
		.voltage = PAD_3_3V,
		.drive = 1,
	},
	{
		.name	= "ISC_nReset",
		.id  = PINID_SAIF1_SDATA0,
		GPIO_OUTPUT
			.data = 1,  // default to high output
	},
	{
		.name	= "ISC_nRequest",
		.id	= PINID_SAIF0_BITCLK,
		GPIO_OUTPUT
			.data = 1,  // default to high output
	},
	{
		.name	= "ISC_Grant",
		.id	= PINID_SAIF0_LRCLK,
		GPIO_INPUT
	},
	{
		.name  = "ISC_nUrgent",
		.id = PINID_SAIF0_SDATA0,
		GPIO_OUTPUT
			.data = 1,  // default to high output
	},
	{
		.name  = "AUART2_Select",
		.id = PINID_SPDIF,
		GPIO_OUTPUT
			.data = 1,  // default to high output
	},
};

#if defined(CONFIG_FEC) || defined(CONFIG_FEC_MODULE)	|| defined(CONFIG_FEC_L2SWITCH)
static struct pin_desc mx28evk_eth_pins[] = 
{
	{
		.name = "ENET0_RESET",// Control RS485/RS232 RXEN pin
		.id = PINID_ENET0_RX_CLK,
		GPIO_OUTPUT
		.drive	= 0,
	}, 
	{
		.name = "ENET0_MDC",
		.id = PINID_ENET0_MDC,
		.fun = PIN_FUN1,
		.strength = PAD_8MA,
		.pull = 1,
		.pullup = 1,
		.voltage = PAD_3_3V,
		.drive	= 1,
	 },
	{
		.name = "ENET0_MDIO",
		.id = PINID_ENET0_MDIO,
		.fun = PIN_FUN1,
		.strength = PAD_8MA,
		.pull = 1,
		.pullup = 1,
		.voltage = PAD_3_3V,
		.drive	= 1,
	 },
	{
		.name = "ENET0_RX_EN",
		.id = PINID_ENET0_RX_EN,
		.fun = PIN_FUN1,
		.strength = PAD_8MA,
		.pull = 1,
		.pullup = 1,
		.voltage = PAD_3_3V,
		.drive	= 1,
	 },
	{
		.name = "ENET0_RXD0",
		.id = PINID_ENET0_RXD0,
		.fun = PIN_FUN1,
		.strength = PAD_8MA,
		.pull = 1,
		.pullup = 1,
		.voltage = PAD_3_3V,
		.drive	= 1,
	 },
	{
		.name = "ENET0_RXD1",
		.id = PINID_ENET0_RXD1,
		.fun = PIN_FUN1,
		.strength = PAD_8MA,
		.pull = 1,
		.pullup = 1,
		.voltage = PAD_3_3V,
		.drive	= 1,
	 },
	{
		.name = "ENET0_TX_EN",
		.id = PINID_ENET0_TX_EN,
		.fun = PIN_FUN1,
		.strength = PAD_8MA,
		.pull = 1,
		.pullup = 1,
		.voltage = PAD_3_3V,
		.drive	= 1,
	 },
	{
		.name = "ENET0_TXD0",
		.id = PINID_ENET0_TXD0,
		.fun = PIN_FUN1,
		.strength = PAD_8MA,
		.pull = 1,
		.pullup = 1,
		.voltage = PAD_3_3V,
		.drive	= 1,
	 },
	{
		.name = "ENET0_TXD1",
		.id = PINID_ENET0_TXD1,
		.fun = PIN_FUN1,
		.strength = PAD_8MA,
		.pull = 1,
		.pullup = 1,
		.voltage = PAD_3_3V,
		.drive	= 1,
	 },
	{
		.name = "ENET0_TXD1",
		.id = PINID_ENET0_TXD1,
		.fun = PIN_FUN1,
		.strength = PAD_8MA,
		.pull = 1,
		.pullup = 1,
		.voltage = PAD_3_3V,
		.drive	= 1,
	 },
	{
		.name = "ENET_CLK",
		.id = PINID_ENET_CLK,
		.fun = PIN_FUN1,
		.strength = PAD_8MA,
		.pull = 1,
		.pullup = 1,
		.voltage = PAD_3_3V,
		.drive	= 1,
	 },
};
#endif


int enable_gpmi = { 1 };
static int __init gpmi_setup(char *__unused)
{
	enable_gpmi = 1;
	return 1;
}

__setup("gpmi", gpmi_setup);

static struct pin_desc mx28evk_gpmi_pins[] = 
{
	{
		.name     = "GPMI D0",
		.id       = PINID_GPMI_D00,
		.fun      = PIN_FUN1,
		.strength = PAD_4MA,
		.voltage  = PAD_3_3V,
		.pullup   = 0,
		.drive    = !0
	},
	{
		.name     = "GPMI D1",
		.id       = PINID_GPMI_D01,
		.fun      = PIN_FUN1,
		.strength = PAD_4MA,
		.voltage  = PAD_3_3V,
		.pullup   = 0,
		.drive    = !0
	},
	{
		.name     = "GPMI D2",
		.id       = PINID_GPMI_D02,
		.fun      = PIN_FUN1,
		.strength = PAD_4MA,
		.voltage  = PAD_3_3V,
		.pullup   = 0,
		.drive    = !0
	 },
	{
		.name     = "GPMI D3",
		.id       = PINID_GPMI_D03,
		.fun      = PIN_FUN1,
		.strength = PAD_4MA,
		.voltage  = PAD_3_3V,
		.pullup   = 0,
		.drive    = !0
	 },
	{
		.name     = "GPMI D4",
		.id       = PINID_GPMI_D04,
		.fun      = PIN_FUN1,
		.strength = PAD_4MA,
		.voltage  = PAD_3_3V,
		.pullup   = 0,
		.drive    = !0
	 },
	{
		.name     = "GPMI D5",
		.id       = PINID_GPMI_D05,
		.fun      = PIN_FUN1,
		.strength = PAD_4MA,
		.voltage  = PAD_3_3V,
		.pullup   = 0,
		.drive    = !0
	 },
	{
		.name     = "GPMI D6",
		.id       = PINID_GPMI_D06,
		.fun      = PIN_FUN1,
		.strength = PAD_4MA,
		.voltage  = PAD_3_3V,
		.pullup   = 0,
		.drive    = !0
	 },
	{
		.name     = "GPMI D7",
		.id       = PINID_GPMI_D07,
		.fun      = PIN_FUN1,
		.strength = PAD_4MA,
		.voltage  = PAD_3_3V,
		.pullup   = 0,
		.drive    = !0
	 },
	{
		.name     = "GPMI RDY0",
		.id       = PINID_GPMI_RDY0,
		.fun      = PIN_FUN1,
		.strength = PAD_4MA,
		.voltage  = PAD_3_3V,
		.pullup   = 0,
		.drive    = !0
	 },
};


#if defined(CONFIG_FEC) || defined(CONFIG_FEC_MODULE)	|| defined(CONFIG_FEC_L2SWITCH)
int mx28evk_enet_gpio_init(void)
{
	/* pwr */
//	gpio_request(MXS_PIN_TO_GPIO(PINID_SSP1_DATA3), "ENET_PWR");
//	gpio_direction_output(MXS_PIN_TO_GPIO(PINID_SSP1_DATA3), 0);

	/* reset phy */
	gpio_request(MXS_PIN_TO_GPIO(PINID_ENET0_RX_CLK), "PHY_RESET");
	gpio_direction_output(MXS_PIN_TO_GPIO(PINID_ENET0_RX_CLK), 0);

	/*
	 * Before timer bug fix(set wrong match value of timer),
	 * mdelay(10) delay 50ms actually.
	 * So change delay to 50ms after timer issue fix.
	 */
	mdelay(50);
	gpio_direction_output(MXS_PIN_TO_GPIO(PINID_ENET0_RX_CLK), 1);

	return 0;
}

void mx28evk_enet_io_lowerpower_enter(void)
{
	int i;
	gpio_direction_output(MXS_PIN_TO_GPIO(PINID_ENET0_RX_CLK), 0);
	gpio_request(MXS_PIN_TO_GPIO(PINID_ENET0_TX_CLK), "ETH_INT");
	gpio_direction_output(MXS_PIN_TO_GPIO(PINID_ENET0_TX_CLK), 0);

	for (i = 0; i < ARRAY_SIZE(mx28evk_eth_pins); i++) 
	{
		mxs_release_pin(mx28evk_eth_pins[i].id,	mx28evk_eth_pins[i].name);
		gpio_request(MXS_PIN_TO_GPIO(mx28evk_eth_pins[i].id),	mx28evk_eth_pins[i].name);
		gpio_direction_output(MXS_PIN_TO_GPIO(mx28evk_eth_pins[i].id), 0);
	}

}

void mx28evk_enet_io_lowerpower_exit(void)
{
	int i;
//	gpio_direction_output(MXS_PIN_TO_GPIO(PINID_SSP1_DATA3), 0);
	gpio_direction_output(MXS_PIN_TO_GPIO(PINID_ENET0_RX_CLK), 1);
	gpio_free(MXS_PIN_TO_GPIO(PINID_ENET0_TX_CLK));
	for (i = 0; i < ARRAY_SIZE(mx28evk_eth_pins); i++) 
	{
		gpio_free(MXS_PIN_TO_GPIO(mx28evk_eth_pins[i].id));
		mxs_request_pin(mx28evk_eth_pins[i].id,	mx28evk_eth_pins[i].fun, mx28evk_eth_pins[i].name);
	}
}

#else
int mx28evk_enet_gpio_init(void)
{
	return 0;
}
void mx28evk_enet_io_lowerpower_enter(void)
{}
void mx28evk_enet_io_lowerpower_exit(void)
{}
#endif

void __init mx28evk_init_pin_group(struct pin_desc *pins, unsigned count)
{
	int i;
	struct pin_desc *pin;
	for (i = 0; i < count; i++) 
	{
		pin = pins + i;
		
		if (pin->fun == PIN_GPIO)
			gpio_request(MXS_PIN_TO_GPIO(pin->id), pin->name);
		else
			mxs_request_pin(pin->id, pin->fun, pin->name);
			
		if (pin->drive) 
		{
			mxs_set_strength(pin->id, pin->strength, pin->name);
			mxs_set_voltage(pin->id, pin->voltage, pin->name);
		}
		
		if (pin->pull)
			mxs_set_pullup(pin->id, pin->pullup, pin->name);
		if (pin->fun == PIN_GPIO) 
		{
			if (pin->output)
				gpio_direction_output(MXS_PIN_TO_GPIO(pin->id),	pin->data);
			else
				gpio_direction_input(MXS_PIN_TO_GPIO(pin->id));
		}
	}
}

static int __init to_hex(int p)
{
	char buf[] = {p, 0};
	return simple_strtol(buf, 0, 16);
}

int g_redstone_use_3_3_i2c_volt = 0;
int g_redstone_enable_daughter_card_latching = 0;
int g_redstone_developer_hw = 0;
int g_redstone_fcc_mode = 0;

int g_redstone_wdt_disable = 0;
int g_redstone_enable_wdt_reboot = 0;
#ifdef CONFIG_TRULINK_2500_MAGIC_USB_IRQ_93_WORK_AROUND
/* Description: Flag used to active work-around on TRULink 2500 devices only (as
 *	the device type can only be determined at run-time). To prevent breaking
 *	other hardware models due to side-effects, the default setting is to not
 *	activate the work-around (since Kernel does not know at compile-time if
 *	the hardware really is a TRULink 2500).
 */
int g_trulink_2500_magic_usb_irq_93_work_around = 0;
EXPORT_SYMBOL(g_trulink_2500_magic_usb_irq_93_work_around);
#endif

/*
 * Description: Parses the RedStone Hardware Configuration (RSHW) string.
 *
 *	The RSHW string is a hexadecimal string which toggles specific hardware features on
 *	or off. Each hex character(char) defines 4 bits (or 4 toggle options). If all of the 4
 *  bits are assigned, add another hex character to add another field. The bit field definition 
 *  below describes	each hardware option.
 *
 * Bit Field Definition:
 *      3210
 *  [0] 0000
 *      ||||
 *      |||+------  0=Not in FCC mode             [1=In FCC mode]
 *      ||+-------  0=Is a production device      [1=Is a developer device]
 *      |+--------  0=No daughter card latching   [1=Enable daughter card latching]
 *      +---------  0=Use 1.8 I2C volt            [1=Use 3.3 I2C volt]
 *
 *      3210
 *  [1] 0000
 *      ||
 *      |+--------  0=Use watchdog reset          [1=Use the regular CPU reset]
 *      +---------  0=No action                   [1=Disable i.MX28 watchdog]
 *
 */
static int __init redstone_hw_config(char* p_str)
{
	unsigned char c = p_str[0];

	if(!c)
	{
		return 0;
	}

	c = to_hex(c);
	g_redstone_use_3_3_i2c_volt = (c & 0x8) >> 3;
	g_redstone_enable_daughter_card_latching = (c & 0x4) >> 2;
	g_redstone_developer_hw = (c & 0x2) >> 1;
	g_redstone_fcc_mode = c & 0x1;

	c = p_str[1];

	if(!c)
	{
		return 0;
	}

	c = to_hex(c);
	g_redstone_wdt_disable = (c & 0x8) >> 3;
	g_redstone_enable_wdt_reboot = (c & 0x4) >> 2;
	return 0;
}

__setup("rshw=", redstone_hw_config);

void __init mx28evk_pins_init(void)
{
	pr_info("Initializing TL5000 imx28 pin configuration\n");

	mx28evk_init_pin_group(mx28evk_fixed_pins, ARRAY_SIZE(mx28evk_fixed_pins));
	mx28evk_init_pin_group(mx28evk_gpmi_pins, ARRAY_SIZE(mx28evk_gpmi_pins));
	mx28evk_init_pin_group(mx28evk_eth_pins,						ARRAY_SIZE(mx28evk_eth_pins));
}

