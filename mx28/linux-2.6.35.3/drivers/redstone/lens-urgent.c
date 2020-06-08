/*
 * Author: Lee Wei <lwei@aware360.com>
 * Date: October 31, 2017
 * Copyright 2014-2017 Aware360
 *
 */
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/types.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>

#include <mach/gpio.h>
#include <mach/pinctrl.h>
#include <mach/../../../mach-mx28/mx28_pins.h>
#include <mach/../../../mach-mx28/regs-pinctrl.h>

#define UMAJOR 240
#define NMINOR 1

#define DESCRIPTION "LENS URGENT Driver"
#define VERSION "1:1.0-aware360"

static struct mutex g_lock;

static atomic_t g_atomic_irq;
static atomic_t* g_irq = &g_atomic_irq;
static int g_irq_num = 0;
static DECLARE_WAIT_QUEUE_HEAD(g_q);
DECLARE_WAIT_QUEUE_HEAD(g_urgent_q);
#define ISC_nRESET     (MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(3, 26)))
#define ISC_nUrgent    (MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(3, 23)))
#define AURAT2    (MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(3, 27)))
#define ISC_RESETRADIO  _IO('r', 0)
#define ISC_WAKEUPRADIO  _IO('r', 1)
bool notFirstInterrupt = 0;

static irqreturn_t irq_fn(int irq, void* dev_id)
{
	//printk(KERN_NOTICE "%s,%d:%s: irq=%d, dev_id=%p\n", __FILE__, __LINE__, __FUNCTION__, irq, dev_id);
	atomic_set(g_irq, 1);
	wake_up_interruptible(&g_urgent_q);
	return IRQ_HANDLED;
}

static int my_open( struct inode* inode, struct file* p_file)
{
	mutex_lock(&g_lock);

	if(!g_irq_num)
	{
		const int irq = g_irq_num = gpio_to_irq(ISC_nUrgent);
		const int ret = request_threaded_irq(irq, NULL, irq_fn, IRQF_TRIGGER_RISING, "lens-urgent", &my_open);

		if (ret < 0)
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

	//printk(KERN_NOTICE "%s,%d: leave open function!\n", __FILE__, __LINE__);
	mutex_unlock(&g_lock);
	return 0;
}

static long my_ioctl(struct file* fp, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	mutex_lock(&g_lock);

	switch (cmd)
	{
	case ISC_RESETRADIO:
		gpio_direction_output(ISC_nRESET, 0); //Reset
		msleep(40);
		//gpio_direction_output(ISC_nRESET, 1);
		gpio_direction_input(ISC_nRESET);
		break;

	case ISC_WAKEUPRADIO:
		gpio_direction_output(ISC_nUrgent, 0); //wake up radio
		msleep(100);
		//gpio_direction_output(ISC_nUrgent, 1);
		gpio_direction_input(ISC_nUrgent);
		break;

	default:
		ret = -EINVAL;
	}

	mutex_unlock(&g_lock);
	return ret;
}


static ssize_t my_read(
	struct file* p_file,
	char __user* p_user,
	size_t p_len,
	loff_t* p_off)
{
	//printk(KERN_NOTICE "%s,%d: beigin read !\n", __FILE__, __LINE__);
	wait_event_interruptible(g_urgent_q, atomic_read(g_irq));

	if(!atomic_read(g_irq))
	{
		printk(KERN_NOTICE "%s,%d: Cancelled!\n", __FILE__, __LINE__);
		if(copy_to_user(p_user, "c", 1))
		{
			return -EFAULT;
		}

		return 1;
	}

	atomic_set(g_irq, 0);
	if(copy_to_user(p_user, (!notFirstInterrupt)?"f":"i", 1))
	{
		printk(KERN_NOTICE "\x1b[1;44;37m%s,%d: leave read function!\x1b[0m\n", __FILE__, __LINE__);
		return -EFAULT;
	}
	if(!notFirstInterrupt) notFirstInterrupt=!notFirstInterrupt;
	//printk(KERN_NOTICE "%s,%d: leave read function!\n", __FILE__, __LINE__);
	return 1;
}

static int my_release(struct inode* inode, struct file* filp)
{
	mutex_lock(&g_lock);
	disable_irq(g_irq_num);
	mutex_unlock(&g_lock);
	//printk(KERN_NOTICE "%s,%d: leave release function!\n", __FILE__, __LINE__);
	return 0;
}

static const struct file_operations my_fops =
{
	.open		= my_open,
	.read		= my_read,
	.release	= my_release,
	.unlocked_ioctl = my_ioctl,
	.owner		= THIS_MODULE,
};

static struct cdev set_rssi_cdev =
{
	.kobj	=	{.name = "lens-urgent", },
	.owner	=	THIS_MODULE,
};

static int __init init(void)
{
	const dev_t dev = MKDEV(UMAJOR, 0);
	int ret;

	mutex_init(&g_lock);
	atomic_set(g_irq, 0);
	ret = register_chrdev_region( dev, NMINOR, "lens-urgent");

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

	gpio_request(ISC_nRESET, "lens-urgent");
	gpio_export(ISC_nRESET, true);
	gpio_request(ISC_nUrgent, "lens-urgent");
	gpio_export(ISC_nUrgent, true);
	gpio_request(AURAT2, "AURAT2");
	gpio_export(AURAT2 , true);
	printk(KERN_NOTICE "TRULink %s, %s loaded\n", DESCRIPTION, VERSION);
	return 0;
}

static void __exit fini(void)
{
	gpio_free(ISC_nRESET);
	gpio_free(ISC_nUrgent);
	cdev_del( &set_rssi_cdev);
	unregister_chrdev_region( MKDEV( UMAJOR, 0), NMINOR);
}

module_init(init);
module_exit(fini);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_AUTHOR("Lee Wei<lwei@aware360.com>");
MODULE_VERSION(VERSION);
