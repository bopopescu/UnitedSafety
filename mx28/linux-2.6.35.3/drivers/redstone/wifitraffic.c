/*
 * Author: Lee Wei <lwei@absolutetrac.com>
 * Date: Nov 05, 2014
 * Copyright 2010-2011 Siconix
 * Copyright 2010-2014 Absolutetrac
 *
 */

#include <linux/module.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/interrupt.h>

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

DECLARE_WAIT_QUEUE_HEAD(g_wifi_q);
static atomic_t g_atomic_irq;
atomic_t* g_wifi_irq = &g_atomic_irq;

// FIXME: Major number might not be correct.
#define UMAJOR 235
#define NMINOR 1

int g_threshold = 10000;
module_param(g_threshold, int, 0);

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
	if(!p_len)
	{
		return 0;
	}

	wait_event_interruptible(g_wifi_q, atomic_read(g_wifi_irq));

	if(!atomic_read(g_wifi_irq))
	{
		if(copy_to_user(p_user, "c", 1))
		{
			return -EFAULT;
		}

		return 1;
	}

	atomic_set(g_wifi_irq, 0);

	if(copy_to_user(p_user, "i", 1))
	{
		return -EFAULT;
	}

	return 1;

}

static const struct file_operations my_fops = {
	.open		= my_open,
	.read		= my_read,
	.owner		= THIS_MODULE,
};

static struct cdev set_cdev = {
	.kobj	=	{.name = "redstone-wifitraffic", },
	.owner	=	THIS_MODULE,
};

static int __init init(void)
{
	const dev_t dev = MKDEV( UMAJOR, 1);
	int ret;
	atomic_set(g_wifi_irq, 0);
	ret = register_chrdev_region( dev, NMINOR, "restone-wifitraffic");
	if( ret) {
		printk( "%s,%d: (%d) register_chrdev_region( 0x%X, %d) failed\n", __FILE__, __LINE__, ret, dev, NMINOR);
		return ret;
	}

	cdev_init( &set_cdev, &my_fops);
	{
		const int ret = cdev_add( &set_cdev, dev, NMINOR);
		if( ret) {
			printk( "%s,%d: (%d) cdev_add( %p, %p, %d) failed\n", __FILE__, __LINE__, ret, &set_cdev, &dev, NMINOR);
			kobject_put( &set_cdev.kobj);
			unregister_chrdev_region( dev, NMINOR);
			return ret;
		}
	}

	printk(KERN_NOTICE "TRULink WIFI Traffic indicator driver loaded");

	return 0;
}

static void __exit fini(void)
{
	cdev_del( &set_cdev);
	unregister_chrdev_region( MKDEV( UMAJOR, 0), NMINOR);

	printk(KERN_NOTICE "TRULink Wifi Traffic Indicator driver unloaded");
}

EXPORT_SYMBOL(g_wifi_irq);
EXPORT_SYMBOL(g_wifi_q);

module_init(init);
module_exit(fini);

MODULE_LICENSE( "GPL");
MODULE_DESCRIPTION( "WIFI Traffic Indicator for TRULink");
MODULE_AUTHOR(	"Lee Wei <lwei@absolutetrac.com>");
MODULE_VERSION( "1:1.0-absolutetrac");
