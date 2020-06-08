/*
 * Author: Amour Hassan <Ahassan@absolutetrac.com>
 * Date: Janurary 6, 2014
 * Copyright 2010-2011 Siconix
 * Copyright 2010-2014 Absolutetrac
 *
 */

#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/circ_buf.h>
#include <linux/poll.h>
#include <asm/current.h>
#include <linux/sched.h>

#include <mach/gpio.h>
#include <mach/pinctrl.h>
#include <mach/../../../mach-mx28/mx28_pins.h>
#include <mach/../../../mach-mx28/regs-pinctrl.h>

#include "redstone.h"

#define DESCRIPTION "TRULink Iridium Emulator Driver"
#define VERSION "1:1.0-absolutetrac"

REDSTONE_COMMAND_PARSER_DEFINITION(g_, 64, 256)

#define MAX_BUF_SIZE 8192
static char g_output[2][MAX_BUF_SIZE];
static unsigned long g_output_head[2] = {0, 0};
static unsigned long g_output_tail[2] = {0, 0};

static DECLARE_MUTEX(g_sem0);
static DECLARE_MUTEX(g_sem1);

static int g_buffered_response = 0;

static DECLARE_WAIT_QUEUE_HEAD(g_read_wait);

static void write_output(const char* p_s, int p_len, int p_buf_id)
{

	while(p_len > 0)
	{
		const char c = *(p_s++);
		--p_len;

		if(CIRC_SPACE(g_output_head[p_buf_id], g_output_tail[p_buf_id], MAX_BUF_SIZE) >= 1)
		{
			g_output[p_buf_id][g_output_head[p_buf_id]] = c;
			g_output_head[p_buf_id] = (g_output_head[p_buf_id] + 1) & (MAX_BUF_SIZE - 1);

			switch(p_buf_id)
			{
			case 0:
				wake_up_interruptible(&g_read_wait);
				up(&g_sem0);
				break;

			case 1: up(&g_sem1); break;
			}

		}

	}

}

static void iridium_output(const char* p_s, int p_len)
{
	write_output(p_s, p_len, 0);
}

static void iridium_app_output(const char* p_s, int p_len)
{
	write_output(p_s, p_len, 1);
}

static int my_open( struct inode *inode, struct file *filp)
{
	return 0;
}

static struct mutex g_lock;

static ssize_t write_iridium(
	struct file* p_file,
	const char __user* p_user,
	size_t p_len,
	loff_t* p_off)
{
	size_t i = p_len;
	const char __user* p = p_user;

	mutex_lock(&g_lock);

	while(i > 0)
	{
		char c;
		const int ret = copy_from_user(&c, p++, 1);
		--i;

		if(ret)
		{
			printk(KERN_ERR "%s,%d: EFAULT: i=%zu, p=%p, p_len=%zu\n", __FILE__, __LINE__, i, p, p_len);
			mutex_unlock(&g_lock);
			return -EFAULT;
		}

		iridium_app_output(&c, 1);
	}

	mutex_unlock(&g_lock);
	return p_len;
}

static ssize_t my_write(
	struct file *p_file,
	const char __user *p_user,
	size_t p_len,
	loff_t *p_off)
{
	const int minor = iminor(p_file->f_path.dentry->d_inode);

	switch(minor)
	{
	// Data from Iridium Driver Command Input is processed by this driver.
	case 0: return redstone_write(&g_rpc, p_file, p_user, p_len, p_off);

	// Data from Iridium AT Input is processed by this driver.
	case 1: return write_iridium(p_file, p_user, p_len, p_off);

	// Data from APP sent directly to Iridium AT Output.
	case 2:
		{
			size_t len = p_len;

			while(len > 0)
			{
				char c;

				if(copy_from_user(&c, p_user++, 1))
				{
					return -EFAULT;
				}

				--len;
				iridium_output(&c, 1);
			}

			return p_len;
		}
		break;
	default: return -EINVAL;
	}

}

static ssize_t read_iridium(
	struct file *p_file,
	char __user *p_user,
	size_t p_len,
	loff_t *p_off)
{
	ssize_t count = 0;
	DECLARE_WAITQUEUE(wait, current);

	if(!p_len)
	{
		return 0;
	}

	for(;;)
	{
		char c = '\0';
		add_wait_queue(&g_read_wait, &wait);

		if(down_interruptible(&g_sem0))
		{
			remove_wait_queue(&g_read_wait, &wait);
			return -EAGAIN;
		}

		remove_wait_queue(&g_read_wait, &wait);

		if(CIRC_CNT(g_output_head[0], g_output_tail[0], MAX_BUF_SIZE) >= 1)
		{
			c = g_output[0][g_output_tail[0]];

			if(copy_to_user(p_user++, g_output[0] + g_output_tail[0], 1))
			{
				return -EFAULT;
			}

			g_output_tail[0] = (g_output_tail[0] + 1) & (MAX_BUF_SIZE - 1);
		}
		else
		{
			return -EIO;
		}

		++count;
		--p_len;

		if(g_buffered_response)
		{

			if(!p_len || ('\r' == c))
			{
				return count;
			}

		}
		else
		{
			return count;
		}

	}

	return 0;
}

static ssize_t read_data_for_iridium_app(
	struct file *p_file,
	char __user *p_user,
	size_t p_len,
	loff_t *p_off)
{
	const int app = 1;

	if(!p_len)
	{
		return 0;
	}

	if(down_interruptible(&g_sem1))
	{
		return -EAGAIN;
	}

	if(CIRC_CNT(g_output_head[app], g_output_tail[app], MAX_BUF_SIZE) >= 1)
	{

		if(copy_to_user(p_user++, g_output[app] + g_output_tail[app], 1))
		{
			return -EFAULT;
		}

		g_output_tail[app] = (g_output_tail[app] + 1) & (MAX_BUF_SIZE - 1);
	}
	else
	{
		return -EIO;
	}

	return 1;
}

extern int g_redstone_use_3_3_i2c_volt;
extern int g_redstone_enable_daughter_card_latching;
extern int g_redstone_developer_hw;
extern int g_redstone_fcc_mode;
extern int g_redstone_wdt_disable;
extern int g_redstone_enable_wdt_reboot;

static ssize_t my_read(
	struct file *p_file,
	char __user *p_user,
	size_t p_len,
	loff_t *p_off)
{
	const int minor = iminor(p_file->f_path.dentry->d_inode);

	switch(minor)
	{
	case 0:
		// READ from response buffer.
		return 0;

	case 1: return read_iridium(p_file, p_user, p_len, p_off);
	case 2: return read_data_for_iridium_app(p_file, p_user, p_len, p_off);
	default: return -EINVAL;
	}

}

static unsigned int my_poll(struct file *p_fp, struct poll_table_struct *p_poll_table)
{
	poll_wait(p_fp, &g_read_wait, p_poll_table);
	// ATS FIXME: Should add mutex lock for "g_output_head" and "g_output_tail". That is, perform
	//	the comparison inside a mutex. I have to go, so I don't have time to do it now.
	return (CIRC_CNT(g_output_head[0], g_output_tail[0], MAX_BUF_SIZE) >= 1) ? POLLIN : 0;
}

static const struct file_operations my_fops = {
	.open		= my_open,
	.write		= my_write,
	.read		= my_read,
	.poll		= my_poll,
	.owner		= THIS_MODULE,
};

static struct cdev g_cdev = {
	.kobj	=	{.name = "iridium-emulator", },
	.owner	=	THIS_MODULE,
};

// ifn = Iridium FunctioN
static int ifn_enable(struct RedStoneParserContext* p_rpc)
{
	printk(KERN_NOTICE "%s,%d:%s: %s='%s'\n", __FILE__, __LINE__, __FUNCTION__, p_rpc->m_key, p_rpc->m_val);
	return 0;
}

static struct RedStoneParserCommand g_iridium_command_struct = {0, "enable", ifn_enable};
static const int g_first_minor = 0;
static const int g_minor_count = 3;

static struct class* g_iridium_class = 0;
static dev_t g_dev;

static int __init init(void)
{
	redstone_init_RedStoneParserContext(&g_rpc, "iridium-emulator", sizeof(g_key), sizeof(g_val), g_key, g_val);
	redstone_append_command(&g_rpc, &g_iridium_command_struct);

	{
		const int ret = alloc_chrdev_region(&g_dev, g_first_minor, g_minor_count, "iridium-emulator");

		if(ret)
		{
			printk( "%s,%d: (%d) alloc_chrdev_region(0x%X, %d) failed\n", __FILE__, __LINE__, ret, g_first_minor, g_minor_count);
			return ret;
		}

	}

	cdev_init(&g_cdev, &my_fops);

	{
		const int ret = cdev_add( &g_cdev, g_dev, g_minor_count);

		if(ret)
		{
			printk(KERN_ERR "%s,%d: (%d) cdev_add( %p, %zu, %d) failed\n", __FILE__, __LINE__, ret, &g_cdev, g_dev, g_minor_count);
			kobject_put(&g_cdev.kobj);
			unregister_chrdev_region(g_dev, g_minor_count);
			return ret;
		}

	}

	g_iridium_class = class_create(THIS_MODULE, "iridium-emulator");

	{
		struct device* iridium_device = device_create(g_iridium_class, 0, MKDEV(MAJOR(g_dev), 0), 0, "iridium-emulator");

	        if(((struct device*)ERR_PTR) == iridium_device)
		{
			printk(KERN_ERR "%s,%d: device_create failed\n", __FILE__, __LINE__);
			cdev_del(&g_cdev);
			kobject_put(&g_cdev.kobj);
			unregister_chrdev_region(g_dev, g_minor_count);
			return -EINVAL;
	        }

	}

	{
		struct device* iridium_device = device_create(g_iridium_class, 0, MKDEV(MAJOR(g_dev), 1), 0, "iridium-emulator-AT");

	        if(((struct device*)ERR_PTR) == iridium_device)
		{
			printk(KERN_ERR "%s,%d: device_create failed\n", __FILE__, __LINE__);
			cdev_del(&g_cdev);
			kobject_put(&g_cdev.kobj);
			unregister_chrdev_region(g_dev, g_minor_count);
			return -EINVAL;
	        }

	}

	{
		struct device* iridium_device = device_create(g_iridium_class, 0, MKDEV(MAJOR(g_dev), 2), 0, "iridium-emulator-APP");

	        if(((struct device*)ERR_PTR) == iridium_device)
		{
			printk(KERN_ERR "%s,%d: device_create failed\n", __FILE__, __LINE__);
			cdev_del(&g_cdev);
			kobject_put(&g_cdev.kobj);
			unregister_chrdev_region(g_dev, g_minor_count);
			return -EINVAL;
	        }

	}

	init_MUTEX(&g_sem0);
	init_MUTEX(&g_sem1);
	down(&g_sem0);
	down(&g_sem1);
	mutex_init(&g_lock);
	printk(KERN_NOTICE "\x1b[1;44;37mRedStone: %s, %s loaded\x1b[0m\n", DESCRIPTION, VERSION);
	return 0;
}

static void __exit fini(void)
{
	device_destroy(g_iridium_class, g_dev);
	cdev_del(&g_cdev);
	kobject_put(&g_cdev.kobj);
	unregister_chrdev_region(g_first_minor, g_minor_count);
	printk(KERN_NOTICE "\x1b[1;44;37mRedStone: %s, %s unloaded\x1b[0m\n", DESCRIPTION, VERSION);
}

module_init(init);
module_exit(fini);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_AUTHOR("Amour Hassan <Ahassan@absolutetrac.com>");
MODULE_VERSION(VERSION);
