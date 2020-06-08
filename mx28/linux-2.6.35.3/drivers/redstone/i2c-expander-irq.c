/*
 * Author: Amour Hassan <amour@absolutetrac.com>
 * Date: November 7, 2013
 * Copyright 2010-2011 Siconix
 * Copyright 2010-2013 Absolutetrac
 *
 */

#include <asm/uaccess.h>
#include <linux/semaphore.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>

#include <mach/gpio.h>
#include <mach/pinctrl.h>
#include <mach/../../../mach-mx28/mx28_pins.h>
#include <mach/../../../mach-mx28/regs-pinctrl.h>

#include "redstone.h"

#define UMAJOR 237
#define NMINOR 1

#define DESCRIPTION "I2C GPIO Expander IRQ"
#define VERSION "1:1.0-absolutetrac"

static atomic_t g_atomic_irq;
static atomic_t* g_irq = &g_atomic_irq;
static int g_irq_num = 0;
DECLARE_WAIT_QUEUE_HEAD(g_q);
static int g_dbg = 0;
static struct file* g_file = 0;

static irqreturn_t irq_fn(int irq, void* dev_id)
{

	if(g_dbg >= 5)
	{
		printk(KERN_NOTICE "%s,%d:%s: irq=%d, dev_id=%p\n", __FILE__, __LINE__, __FUNCTION__, irq, dev_id);
	}

	atomic_set(g_irq, 1);
	wake_up_interruptible(&g_q);
        return IRQ_HANDLED;
}

static struct mutex g_lock;

/*
 * Description: Opens a connection to the I2C Expander IRQ. Only one process instance may listen for IRQs.
 *
 * Return: 0 is returned on success and the I2C Expander IRQ is enabled (will trigger on events). Otherwise a negative errno number is returned
 *	and the I2C Expander IRQ is not enabled.
 */
static int my_open( struct inode* inode, struct file* p_file)
{
	mutex_lock(&g_lock);

	if(g_file)
	{
		mutex_unlock(&g_lock);
		return -EBUSY;
	}

	if(!g_irq_num)
	{
		const int irq = gpio_to_irq(MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(3, 3)));
		const int ret = request_threaded_irq(irq, NULL, irq_fn, IRQF_TRIGGER_FALLING, "i2c-gpio-expander-irq", &my_open);

		if(ret < 0)
		{
			mutex_unlock(&g_lock);
			printk(KERN_ERR "%s,%d: request_irq() failed: %d\n", __FILE__, __LINE__, ret);
			return ret;
		}

		g_irq_num = irq;
	}
	else
	{
		enable_irq(g_irq_num);
	}

	g_file = p_file;
	mutex_unlock(&g_lock);
	return 0;
}

/* ATS FIXME: This string/command parsing code is modular, and should be moved into its own
 *	source file/library (for use in other RedStone drivers).
 */
REDSTONE_COMMAND_PARSER_DEFINITION(g_, 64, 256)

static ssize_t my_write(struct file* p_file, const char __user* p_user, size_t p_len, loff_t* p_off)
{
	return redstone_write(&g_rpc, p_file, p_user, p_len, p_off);
}

static ssize_t my_read(
	struct file* p_file,
	char __user* p_user,
	size_t p_len,
	loff_t* p_off)
{

	if(!p_len)
	{
		return 0;
	}

	wait_event_interruptible(g_q, atomic_read(g_irq));

	if(!atomic_read(g_irq))
	{

		if(g_dbg >= 4)
		{
			printk(KERN_NOTICE "\x1b[1;44;37m%s,%d: Cancelled!\x1b[0m\n", __FILE__, __LINE__);
		}

		if(copy_to_user(p_user, "c", 1))
		{
			return -EFAULT;
		}

		return 1;
	}

	atomic_set(g_irq, 0);

	if(g_dbg >= 4)
	{
		printk(KERN_NOTICE "\x1b[1;44;37m%s,%d: Interrupt!\x1b[0m\n", __FILE__, __LINE__);
	}

	if(copy_to_user(p_user, "i", 1))
	{
		return -EFAULT;
	}

	return 1;
}

static int my_release(struct inode* inode, struct file* filp)
{
	mutex_lock(&g_lock);
	disable_irq(g_irq_num);
	g_file = 0;
	redstone_reset_parser_state_machine(&g_rpc);
	mutex_unlock(&g_lock);
	return 0;
}

static const struct file_operations my_fops =
{
	.open		= my_open,
	.write		= my_write,
	.read		= my_read,
	.release	= my_release,
	.owner		= THIS_MODULE,
};

static struct cdev set_rssi_cdev =
{
	.kobj	=	{.name = "i2c-gpio-expander-irq", },
	.owner	=	THIS_MODULE,
};

static int cmd_debug(struct RedStoneParserContext* p_rpc)
{
	p_rpc->m_dbg = simple_strtol(p_rpc->m_val, 0, 0);

	if(p_rpc->m_dbg)
	{
		printk(KERN_NOTICE "%s,%d: Debugging is ON\n", __FILE__, __LINE__);
	}
	else
	{
		printk(KERN_NOTICE "%s,%d: Debugging is OFF\n", __FILE__, __LINE__);
	}

	return 0;
}

static struct RedStoneParserCommand g_cmd_dbg = {0, "dbg", cmd_debug};
static struct RedStoneParserCommand g_cmd_debug = {0, "debug", cmd_debug};

static int __init init(void)
{
	const dev_t dev = MKDEV(UMAJOR, 0);
	int ret;

	redstone_init_RedStoneParserContext(&g_rpc, "i2c-gpio-expander-irq", sizeof(g_key), sizeof(g_val), g_key, g_val);
	redstone_append_command(&g_rpc, &g_cmd_dbg);
	redstone_append_command(&g_rpc, &g_cmd_debug);

	mutex_init(&g_lock);
	atomic_set(g_irq, 0);
	ret = register_chrdev_region( dev, NMINOR, "i2c-gpio-expander-irq");

	if( ret)
	{
		printk( "%s,%d: (%d) register_chrdev_region( 0x%X, %d) failed\n", __FILE__, __LINE__, ret, dev, NMINOR);
		return ret;
	}

	cdev_init( &set_rssi_cdev, &my_fops);
	{
		const int ret = cdev_add( &set_rssi_cdev, dev, NMINOR);

		if( ret)
		{
			printk( "%s,%d: (%d) cdev_add( %p, %p, %d) failed\n", __FILE__, __LINE__, ret, &set_rssi_cdev, &dev, NMINOR);
			kobject_put( &set_rssi_cdev.kobj);
			unregister_chrdev_region( dev, NMINOR);
			return ret;
		}

	}

	gpio_request(MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(3, 3)), "i2c-gpio-expander-irq");
	gpio_direction_input(MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(3, 3)));
	gpio_export(MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(3, 3)), true);
	printk(KERN_NOTICE "TRULink %s, %s loaded\n", DESCRIPTION, VERSION);
	return 0;
}

module_init(init);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_AUTHOR("Amour Hassan <amour@absolutetrac.com>");
MODULE_VERSION(VERSION);
