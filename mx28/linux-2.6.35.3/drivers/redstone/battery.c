/*
 * Author: Amour Hassan <amour@absolutetrac.com>
 * Date: Mar 18, 2013
 * Copyright 2010-2011 Siconix
 * Copyright 2010-2013 Absolutetrac
 *
 */

#include <linux/module.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <asm/processor.h> /* cpu_relax */
#include <mach/hardware.h>
#include <mach/lradc.h>
#include <mach/regs-power.h>
#include <mach/regs-lradc.h>
#include <mach/lradc.h>

#include <mach/gpio.h>
#include <mach/pinctrl.h>
#include <mach/../../../mach-mx28/mx28_pins.h>
#include <mach/../../../mach-mx28/regs-pinctrl.h>

#define REGS_LRADC_BASE IO_ADDRESS(LRADC_PHYS_ADDR)
#define GAIN_CORRECTION 1012    /* 1.012 */

// FIXME: Major number might not be correct.
#define UMAJOR 236
#define NMINOR 1

int g_voltage_channel = 2;
module_param(g_voltage_channel, int, 0);

int get_voltage(void)
{
	uint32_t  ch8Value;
	const uint32_t channel = g_voltage_channel;
	const uint32_t lradc_irq_mask = 1 << channel;

	/* Clear the interrupt flag */
	__raw_writel(lradc_irq_mask,
			REGS_LRADC_BASE + HW_LRADC_CTRL1_CLR);
	__raw_writel(BF_LRADC_CTRL0_SCHEDULE(1 << channel),
			REGS_LRADC_BASE + HW_LRADC_CTRL0_SET);

	/* Wait for conversion complete*/
	while (!(__raw_readl(REGS_LRADC_BASE + HW_LRADC_CTRL1)
			& lradc_irq_mask))
		cpu_relax();

	/* Clear the interrupt flag again */
	__raw_writel(lradc_irq_mask,
			REGS_LRADC_BASE + HW_LRADC_CTRL1_CLR);

	/* read voltage value and clr lradc */
	ch8Value = __raw_readl(REGS_LRADC_BASE +
			HW_LRADC_CHn(channel)) & BM_LRADC_CHn_VALUE;

	__raw_writel(BM_LRADC_CHn_VALUE,
			REGS_LRADC_BASE + HW_LRADC_CHn_CLR(channel));

	return ch8Value;
}

static int my_open( struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t my_read(
	struct file *p_file,
	char __user *p_user,
	size_t p_len,
	loff_t *p_off)
{
	char buf[64];
	int out_len = sprintf(buf, "%d\n", get_voltage());

	if(out_len < 0)
	{
		return -EINVAL;
	}

	if(copy_to_user(p_user, buf, (p_len < out_len) ? p_len : out_len))
	{
		return -EFAULT;
	}
	
	return out_len;
}

static const struct file_operations my_fops = {
	.open		= my_open,
	.read		= my_read,
	.owner		= THIS_MODULE,
};

static struct cdev set_rssi_cdev = {
	.kobj	=	{.name = "battery-voltage", },
	.owner	=	THIS_MODULE,
};

static int __init init(void)
{
	const dev_t dev = MKDEV( UMAJOR, 0);
	int ret;
	ret = register_chrdev_region( dev, NMINOR, "battery-voltage");
	if( ret) {
		printk( "%s,%d: (%d) register_chrdev_region( 0x%X, %d) failed\n", __FILE__, __LINE__, ret, dev, NMINOR);
		return ret;
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

	__raw_writel(BF_LRADC_CTRL2_DIVIDE_BY_TWO(1 << g_voltage_channel),
			REGS_LRADC_BASE + HW_LRADC_CTRL2_SET);

	printk(KERN_NOTICE "TRULink Battery Voltage driver loaded");

	return 0;
}

static void __exit fini(void)
{
	cdev_del( &set_rssi_cdev);
	unregister_chrdev_region( MKDEV( UMAJOR, 0), NMINOR);

	printk(KERN_NOTICE "TRULink Battery Voltage driver unloaded");
}

module_init(init);
module_exit(fini);

MODULE_LICENSE( "GPL");
MODULE_DESCRIPTION( "Read Battery Voltage for TRULink");
MODULE_AUTHOR(	"Amour Hassan <amour@absolutetrac.com>");
MODULE_VERSION( "1:1.0-absolutetrac");
