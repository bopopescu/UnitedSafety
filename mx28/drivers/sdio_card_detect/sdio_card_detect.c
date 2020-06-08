/*
 * Author: Amour Hassan <ahassan@siconix.com>
 * Date: May 4, 2011
 * Copyright 2011 Siconix
 */

#include <linux/module.h>
#include <mach/gpio.h>
#include <mach/pinctrl.h>
#include <mach/../../../mach-mx28/mx28_pins.h>
#include <mach/../../../mach-mx28/regs-pinctrl.h>

static int __init init(void)
{
	gpio_request(MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(2, 7)), "SDIO_CARD_DETECT");
	gpio_direction_output(MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(2, 7)), 0);
	return 0;
}

static void __exit fini(void)
{
	gpio_direction_output(MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(2, 7)), 1);
	gpio_free( MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(2, 7)));
}

module_init(init);
module_exit(fini);

MODULE_LICENSE( "GPL");
MODULE_DESCRIPTION( "SDIO Card Detect (for WiFi)");
MODULE_AUTHOR(	"Amour Hassan <ahassan@siconix.com>");
MODULE_VERSION( "1:1.0-siconix");
