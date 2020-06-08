/*
 * Author: Amour Hassan <Amour.Hassan@gps1.com>
 * Date: July 24, 2014
 * Copyright 2010-2011 Siconix
 * Copyright 2010-2013 Absolutetrac
 * Copyright 2010-2014 AbsoluteGemini
 *
 */

#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>

#include <mach/gpio.h>
#include <mach/pinctrl.h>
#include <mach/../../../mach-mx28/mx28_pins.h>
#include <mach/../../../mach-mx28/regs-pinctrl.h>

#include "redstone.h"

#ifdef CONFIG_TRULINK_ETHERNET_PHY_SUSPEND
#include "pm.h"
#endif

#define DESCRIPTION "GPIO Driver"
#define VERSION "1:10.0-AbsoluteGemini"

#define UMAJOR 238
#define NMINOR 3

#define C_EXTERNAL_POWER_ENABLE   (MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(2, 3)))
#define D_SERIAL_PORT_MODE		(MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(2, 7)))
#define E_SERIAL_PORT_DEN		(MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(2, 5)))
#define F_SERIAL_PORT_RXEN		(MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(3, 18)))
#define G_CELL_POWER 					(MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(2, 17)))
#define H_GPS_ANTENNA_POWER 	(MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(3, 11)))
#define J_CELL_ENABLE 				(MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(2, 16)))
#define L_CPU_POWER 					(MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(3, 12)))
#define M_USB0_POWER 					(MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(2, 20)))
#define N_CAN_POWER 					(MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(2, 9)))
#define O_WIRELESS_ENABLE		(MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(3, 2)))
#define P_ENABLE_3V 					(MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(3, 6)))
#define Q_ENABLE_5V 					(MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(3, 7)))
#define R_IRIDIUM_ENABLE			(MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(2, 26)))
#define S_XBEE_ENABLE					(MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(2, 27)))
#define T_POWER_LED_GREEN   	(MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(1, 11)))
#define U_POWER_LED_RED     	(MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(1, 10)))
#define V_USB_HUB_RESET 			(MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(2, 21)))
#define W_ENET_RESET		 			(MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(4, 13)))
#define X_LED_RESET			 			(MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(2, 4)))
#define Z_INPUT1 							(MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(3, 20)))

REDSTONE_COMMAND_PARSER_DEFINITION(g_, 64, 256)

static int my_open( struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t write_gpio(
	struct file *p_file,
	const char __user *p_user,
	size_t p_len,
	loff_t *p_off)
{
	const int len = p_len;
	while(p_len--) {
		char c;
		copy_from_user(&c, p_user++, 1);
		switch(c) {
		// Output 3
		case 'c': gpio_direction_output(C_EXTERNAL_POWER_ENABLE, 0); break;
		case 'C': gpio_direction_output(C_EXTERNAL_POWER_ENABLE, 1); break;

		// modem power
		case 'g': gpio_direction_output(G_CELL_POWER, 0); break;
		case 'G': gpio_direction_output(G_CELL_POWER, 1); break;
		// h - Power to GPS antenna
		case 'h': gpio_direction_output(H_GPS_ANTENNA_POWER, 0); break;
		case 'H': gpio_direction_output(H_GPS_ANTENNA_POWER, 1); break;

		// Cell Enable
		case 'j': gpio_direction_output(J_CELL_ENABLE, 0); break;
		case 'J': gpio_direction_output(J_CELL_ENABLE, 1); break;

		// CPU Off
		case 'l': gpio_direction_output(L_CPU_POWER, 0); break;
		case 'L': gpio_direction_output(L_CPU_POWER, 1); break;

		// USB0 Power (Host mode), active high
		case 'm': gpio_direction_output(M_USB0_POWER, 0); break;
		case 'M': gpio_direction_output(M_USB0_POWER, 1); break;

		// CAN_ON (Allow the CAN to transmit data), active high
		case 'n': gpio_direction_output(N_CAN_POWER, 0); break;
		case 'N': gpio_direction_output(N_CAN_POWER, 1); break;

		case 'd': gpio_direction_output(D_SERIAL_PORT_MODE, 0); break;
		case 'D': gpio_direction_output(D_SERIAL_PORT_MODE, 1); break;

		case 'e': gpio_direction_output(E_SERIAL_PORT_DEN, 0); break;
		case 'E': gpio_direction_output(E_SERIAL_PORT_DEN, 1); break;

		case 'f': gpio_direction_output(F_SERIAL_PORT_RXEN, 0); break;
		case 'F': gpio_direction_output(F_SERIAL_PORT_RXEN, 1); break;

		// Wireless Enable
		case 'o': gpio_direction_output(O_WIRELESS_ENABLE, 0); break;
		case 'O': gpio_direction_output(O_WIRELESS_ENABLE, 1); break;

		// 3V Enable
		case 'p': gpio_direction_output(P_ENABLE_3V, 0); break;
		case 'P': gpio_direction_output(P_ENABLE_3V, 1); break;

		// 5V Enable
		case 'q': gpio_direction_output(Q_ENABLE_5V, 0); break;
		case 'Q': gpio_direction_output(Q_ENABLE_5V, 1); break;
		// Iridium Enable
		case 'r': gpio_direction_output(R_IRIDIUM_ENABLE, 0); break;
		case 'R': gpio_direction_output(R_IRIDIUM_ENABLE, 1); break;
		// XBee Enable
		case 's': gpio_direction_output(S_XBEE_ENABLE, 0); break;
		case 'S': gpio_direction_output(S_XBEE_ENABLE, 1); break;
		// power LED green
		case 't': gpio_direction_output(T_POWER_LED_GREEN, 0); break;
		case 'T': gpio_direction_output(T_POWER_LED_GREEN, 1); break;
		// power LED red
		case 'u': gpio_direction_output(U_POWER_LED_RED, 0); break;
		case 'U': gpio_direction_output(U_POWER_LED_RED, 1); break;
		// V_USB_HUB_RESET
		case 'v': gpio_direction_output(V_USB_HUB_RESET, 0); break;
		case 'V': gpio_direction_output(V_USB_HUB_RESET, 1); break;
		// W_ENET_RESET
		case 'w': gpio_direction_output(W_ENET_RESET, 0); break;
		case 'W': gpio_direction_output(W_ENET_RESET, 1); break;
		// X_LED_RESET
		case 'x': gpio_direction_output(X_LED_RESET, 0); break;
		case 'X': gpio_direction_output(X_LED_RESET, 1); break;
		}
	}
	return len;
}

static ssize_t my_write
(
	struct file *p_file,
	const char __user *p_user,
	size_t p_len,
	loff_t *p_off)
{
	const int minor = iminor(p_file->f_path.dentry->d_inode);

	switch(minor)
	{
	case 0: return write_gpio(p_file, p_user, p_len, p_off);
	case 2: return redstone_write(&g_rpc, p_file, p_user, p_len, p_off);
	default: return -EINVAL;
	}

}

static ssize_t read_gpio
(
	struct file *p_file,
	char __user *p_user,
	size_t p_len,
	loff_t *p_off
)
{
	char __user *p_user_start = p_user;
	size_t i = 'a';
	while(p_len--) 
	{
		char c;
		switch(i) 
		{
		case 'c': c = gpio_get_value(C_EXTERNAL_POWER_ENABLE) ? 'C' : 'c'; break;
		case 'd': c = gpio_get_value(D_SERIAL_PORT_MODE) ? 'D' : 'd'; break;		// RS485_RS232
		case 'e': c = gpio_get_value(E_SERIAL_PORT_DEN) ? 'E' : 'e'; break;		// RS485_RS232_DEN
		case 'f': c = gpio_get_value(F_SERIAL_PORT_RXEN) ? 'F' : 'f'; break;		// RS485_RS232_RXEN
		case 'g': c = gpio_get_value(G_CELL_POWER) ? 'G' : 'g'; break;	// Cell power
		case 'h': c = gpio_get_value(H_GPS_ANTENNA_POWER) ? 'H' : 'h'; break;		// GPS Antenna power
		case 'j': c = gpio_get_value(J_CELL_ENABLE) ? 'J' : 'j'; break;	// Cell Enable
		case 'l': c = gpio_get_value(L_CPU_POWER) ? 'L' : 'l'; break;		// CPU Off
		case 'm': c = gpio_get_value(M_USB0_POWER) ? 'M' : 'm'; break;	// USB0 Power
		case 'n': c = gpio_get_value(N_CAN_POWER) ? 'N' : 'n'; break;		// CAN_ON
		case 'o': c = gpio_get_value(O_WIRELESS_ENABLE) ? 'O' : 'o'; break;		// Wireless Enable
		case 'p': c = gpio_get_value(P_ENABLE_3V) ? 'P' : 'p'; break;		// 3V Enable
		case 'q': c = gpio_get_value(Q_ENABLE_5V) ? 'Q' : 'q'; break;		// 5V Enable
		case 'r': c = gpio_get_value(R_IRIDIUM_ENABLE) ? 'R' : 'r'; break;		// Iridium Enable
		case 's': c = gpio_get_value(S_XBEE_ENABLE) ? 'S' : 's'; break;		// XBee Enable
		case 't': c = gpio_get_value(T_POWER_LED_GREEN) ? 'T' : 't'; break;		// Power LED - Green
		case 'u': c = gpio_get_value(U_POWER_LED_RED) ? 'U' : 'u'; break;		// Power LED - Red
		case 'v': c = gpio_get_value(V_USB_HUB_RESET) ? 'V' : 'v'; break;		// USB HUB reset
		case 'w': c = gpio_get_value(W_ENET_RESET) ? 'W' : 'w'; break;		// Ethernet reset
		case 'x': c = gpio_get_value(X_LED_RESET) ? 'X' : 'x'; break;		// Ethernet reset
		case 'z': c = gpio_get_value(Z_INPUT1) ? 'z' : 'Z'; break;		// Ignition line

		default: c = '.'; break;
		}
		if(copy_to_user(p_user++, &c, 1)) return -EFAULT;
		if((++i) > 'z') {
			i = 'a';
			if(p_len)
			{
				if(copy_to_user(p_user++, "\n", 1)) return -EFAULT;
				--p_len;
			}
		}
	}
	return (p_user - p_user_start);
}

extern int g_redstone_use_3_3_i2c_volt;
extern int g_redstone_enable_daughter_card_latching;
extern int g_redstone_developer_hw;
extern int g_redstone_fcc_mode;
extern int g_redstone_wdt_disable;
extern int g_redstone_enable_wdt_reboot;

/*
 * The "redstone_ddi_power_GetDieTemp" function pointer will be set by "drivers/hwmon/mx-cputemp.c"
 * when the i.MX28 hardware monitor driver is loaded. By default, the function pointer is NULL.
 * When the hardware monitor driver is not loaded, NO_TEMPERATURE is returned.
 */
void (*redstone_ddi_power_GetDieTemp)(int16_t *pLow, int16_t *pHigh) = 0;

EXPORT_SYMBOL_GPL(redstone_ddi_power_GetDieTemp);

#define NO_TEMPERATURE -32767

static ssize_t read_hw_info(
	struct file *p_file,
	char __user *p_user,
	size_t p_len,
	loff_t *p_off)
{
	char buf[1024];
	int len;
	int16_t lo = NO_TEMPERATURE;
	int16_t hi = NO_TEMPERATURE;

	if(redstone_ddi_power_GetDieTemp)
	{
		redstone_ddi_power_GetDieTemp(&lo, &hi);
	}

	/* AWARE360 FIXME: Add ability for caller to read back only a subset of hardware strings. */
	len = snprintf(buf, sizeof(buf) - 1,
		"g_redstone_use_3_3_i2c_volt=%d\n"
		"g_redstone_enable_daughter_card_latching=%d\n"
		"g_redstone_developer_hw=%d\n"
		"g_redstone_fcc_mode=%d\n"
		"g_redstone_wdt_disable=%d\n"
		"g_redstone_enable_wdt_reboot=%d\n"
		"die_temp=%d,%d\n"

		,g_redstone_use_3_3_i2c_volt
		,g_redstone_enable_daughter_card_latching
		,g_redstone_developer_hw
		,g_redstone_fcc_mode
		,g_redstone_wdt_disable
		,g_redstone_enable_wdt_reboot
		,(int)lo, (int)hi
		);

	/* ATS FIXME: Check implementation of "snprintf", "len < 0" might never happen. */
	if(len < 0)
	{
		return -EPERM;
	}

	if(len >= (sizeof(buf) - 1))
	{
		return -ENOMEM;
	}

	if((*p_off) >= len)
	{
		return 0;
	}

	len -= (*p_off);
	{
		const int nwrite = (p_len < len) ? p_len : len;

		if(copy_to_user(p_user, buf + (*p_off), nwrite))
		{
			return -EFAULT;
		}

		*p_off += nwrite;

		return nwrite;
	}

}

static ssize_t my_read(
	struct file *p_file,
	char __user *p_user,
	size_t p_len,
	loff_t *p_off)
{
	const int minor = iminor(p_file->f_path.dentry->d_inode);

	switch(minor)
	{
	case 0: return read_gpio(p_file, p_user, p_len, p_off);
	case 1: return read_hw_info(p_file, p_user, p_len, p_off);
	default: return -EINVAL;
	}

}

static const struct file_operations my_fops = {
	.open		= my_open,
	.write		= my_write,
	.read		= my_read,
	.owner		= THIS_MODULE,
};

static struct cdev set_rssi_cdev = {
	.kobj	=	{.name = "set_gpio", },
	.owner	=	THIS_MODULE,
};

#ifdef CONFIG_REDSTONE_EXAMPLE_SET_GPIO_COMMANDS
/* g_example_varaible is not static. Other compilation units (source files) can declare this variable
   as "extern" so it may be used there.
 */
int g_example_variable = 0;

static int example_function(struct RedStoneParserContext* p_rpc)
{
	printk(KERN_NOTICE "%s,%d:%s: value string is '%s'\n", __FILE__, __LINE__, __FUNCTION__, p_rpc->m_val);

	if((g_example_variable = simple_strtol(p_rpc->m_val, 0, 0)))
	{
		printk(KERN_NOTICE "%s,%d:%s: Example setting is ON\n", __FILE__, __LINE__, __FUNCTION__);
	}
	else
	{
		printk(KERN_NOTICE "%s,%d:%s: Example setting is OFF\n", __FILE__, __LINE__, __FUNCTION__);
	}

	return 0;
}

static struct RedStoneParserCommand g_example_command_struct = {0, "example_variable", example_function};
#endif /* CONFIG_REDSTONE_EXAMPLE_SET_GPIO_COMMANDS */

static void __init daughter_card_latch_on(void)
{
// removing the next line means that rshw does not need to be set anymore since nothing is used from it anymore.
//	if(g_redstone_enable_daughter_card_latching)
	{
		printk(KERN_EMERG "\x1b[1;42;37mTRULink: Latching Daughter Card Signals...\x1b[s\x1b[0m\n");
		gpio_direction_output(L_CPU_POWER, 1);
	}
}

static void __init daughter_card_latch_off(void)
{
//	if(g_redstone_enable_daughter_card_latching)
	{
		gpio_direction_output(L_CPU_POWER, 0);
		printk(KERN_EMERG "\x1b[u\x1b[A\x1b[1;42;37mOK!\x1b[0m\n");
	}
}
static void __init daughter_card_latch(void)
{
	gpio_direction_output(L_CPU_POWER, 1);
	gpio_direction_output(L_CPU_POWER, 0);
	printk(KERN_EMERG "\x1b[1;42;37mTRULink: Latched Daughter Card Signals...\x1b[s\x1b[0m\n");
}

static void __init turn_off_daughter_card(void)
{
	printk(KERN_NOTICE "\x1b[1;44;37mTRULink: Powering OFF Daughter Card\x1b[0m\n");
	gpio_direction_output(G_CELL_POWER, 0);
	gpio_direction_output(J_CELL_ENABLE, 0);
	gpio_direction_output(P_ENABLE_3V, 0);
	gpio_direction_output(Q_ENABLE_5V, 0);

	gpio_direction_output(O_WIRELESS_ENABLE, 0);
	gpio_direction_output(R_IRIDIUM_ENABLE, 0);
	gpio_direction_output(S_XBEE_ENABLE, 0);
	daughter_card_latch();
}

static void __init turn_on_daughter_card(void)
{
	printk(KERN_NOTICE "\x1b[1;44;37mTRULink: Powering ON Daughter Card\x1b[0m\n");
	gpio_direction_output(G_CELL_POWER, 1);
	gpio_direction_output(J_CELL_ENABLE, 1);
	gpio_direction_output(P_ENABLE_3V, 1);
	gpio_direction_output(Q_ENABLE_5V, 1);
	
	daughter_card_latch();
}

static int __init init(void)
{
	const dev_t dev = MKDEV( UMAJOR, 0);

	redstone_init_RedStoneParserContext(&g_rpc, "set-gpio", sizeof(g_key), sizeof(g_val), g_key, g_val);
	g_rpc.m_dbg =1000;
	redstone_append_command(&g_rpc, &g_trulink_pm_struct);

	{
		const int ret = register_chrdev_region( dev, NMINOR, "set_gpio");

		if( ret)
		{
			printk( "%s,%d: (%d) register_chrdev_region( 0x%X, %d) failed\n", __FILE__, __LINE__, ret, dev, NMINOR);
			return ret;
		}

	}

	cdev_init( &set_rssi_cdev, &my_fops);
	{
		const int ret = cdev_add( &set_rssi_cdev, dev, NMINOR);
		if( ret) {
			printk( "%s,%d: (%d) cdev_add( %p, %p, %d) failed\n", __FILE__, __LINE__, ret, &set_rssi_cdev, &dev, NMINOR);
			kobject_put( &set_rssi_cdev.kobj);
			unregister_chrdev_region( dev, NMINOR);
			return ret;
		}
	}

	/* Request the GPIO */
	gpio_request(C_EXTERNAL_POWER_ENABLE, "C_EXTERNAL_POWER_ENABLE");
	gpio_request(D_SERIAL_PORT_MODE, "Card_Detect_Output");
	gpio_request(E_SERIAL_PORT_DEN, "RS485_RS232_DEN");
	gpio_request(F_SERIAL_PORT_RXEN, "RS485_RS232_RXEN");
	gpio_request(G_CELL_POWER, "Modem_Power");
	gpio_request(H_GPS_ANTENNA_POWER, "GPS_Antenna_Power");
	gpio_request(J_CELL_ENABLE, "J_CELL_ENABLE");
	gpio_request(L_CPU_POWER, "CPU_OFF");
	gpio_request(M_USB0_POWER, "M_USB0_POWER");
	gpio_request(N_CAN_POWER, "CAN_ON");
	gpio_request(O_WIRELESS_ENABLE, "O_WIRELESS_ENABLE");
	gpio_request(P_ENABLE_3V, "3V Enable");
	gpio_request(Q_ENABLE_5V, "5V Enable");
	gpio_request(R_IRIDIUM_ENABLE, "Iridium Enable");
	gpio_request(S_XBEE_ENABLE, "XBee Enable");
	gpio_request(T_POWER_LED_GREEN, "Power LED Green");
	gpio_request(U_POWER_LED_RED, "Power LED Red");
	gpio_request(V_USB_HUB_RESET, "USB Hub reset");
	gpio_request(W_ENET_RESET, "W_ENET_RESET");
	gpio_request(X_LED_RESET, "X_LED_RESET");
	gpio_request(Z_INPUT1, "Z_INPUT1");

	/* Set the GPIO direction (output) */

	gpio_direction_output(C_EXTERNAL_POWER_ENABLE, 0);
	gpio_direction_output(D_SERIAL_PORT_MODE, 0);
	gpio_direction_output(E_SERIAL_PORT_DEN, 0);
	gpio_direction_output(F_SERIAL_PORT_RXEN, 0);

	gpio_direction_output(J_CELL_ENABLE, 0);
	gpio_direction_output(G_CELL_POWER, 0);
	gpio_direction_output(H_GPS_ANTENNA_POWER, 1);
	gpio_direction_output(L_CPU_POWER, 0);
	gpio_direction_output(M_USB0_POWER, 1);
	gpio_direction_output(N_CAN_POWER, 1);
	gpio_direction_output(W_ENET_RESET, 0);
	gpio_direction_output(X_LED_RESET, 1);

	turn_off_daughter_card();
	turn_on_daughter_card();

	printk(KERN_NOTICE "\x1b[1;44;37mTRULink: %s, %s loaded\x1b[0m\n", DESCRIPTION, VERSION);
	return 0;
}

static void __exit fini(void)
{
	gpio_free( C_EXTERNAL_POWER_ENABLE );
	gpio_free( D_SERIAL_PORT_MODE);
	gpio_free( E_SERIAL_PORT_DEN);
	gpio_free( F_SERIAL_PORT_RXEN);
	gpio_free( G_CELL_POWER);
	gpio_free( H_GPS_ANTENNA_POWER);
	gpio_free( J_CELL_ENABLE);
	gpio_free( L_CPU_POWER);
	gpio_free( M_USB0_POWER);
	gpio_free( N_CAN_POWER);
	gpio_free( O_WIRELESS_ENABLE);
	gpio_free( P_ENABLE_3V);
	gpio_free( Q_ENABLE_5V);
	gpio_free( R_IRIDIUM_ENABLE);
	gpio_free( S_XBEE_ENABLE);
	gpio_free( T_POWER_LED_GREEN);
	gpio_free( U_POWER_LED_RED);
	gpio_free( V_USB_HUB_RESET);
  gpio_free( W_ENET_RESET);
  gpio_free( X_LED_RESET);
  gpio_free( Z_INPUT1 );

	cdev_del( &set_rssi_cdev);
	unregister_chrdev_region( MKDEV( UMAJOR, 0), NMINOR);
	printk(KERN_NOTICE "\x1b[1;44;37mTRULink: %s, %s unloaded\x1b[0m\n", DESCRIPTION, VERSION);
}

module_init(init);
module_exit(fini);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_AUTHOR("Amour Hassan <Amour.Hassan@gps1.com>");
MODULE_VERSION(VERSION);
