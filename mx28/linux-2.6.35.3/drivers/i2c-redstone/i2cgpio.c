/*
 * Author: Lee Wei  <lee@absolutetrac.com>
 * Date: March 7, 2014
 * Copyright 2013-2014 AbsoluteGemini
 *
 * Description:
 * ==========================================================================
 *    This is a debug driver for testing the I2C interface on the TRULink
 *    (RedStone).
 *
 * Change Log:
 * ==========================================================================
 *    May 14, 2013: Amour Hassan <amour@absolutetrac.com>
 *       - Only call IRQ enable if "i2cgpio.open_count" is zero.
 *       - Refactoring
 *
 *    March 7, 2014: Amour Hassan <amour@absolutetrac.com>
 *       - Adding documentation
 */

#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/kfifo.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>

#include <mach/gpio.h>
#include <mach/pinctrl.h>
#include <mach/../../../mach-mx28/mx28_pins.h>
#include <mach/../../../mach-mx28/regs-pinctrl.h>

#define I2C_INPUT            0
#define I2C_OUTPUT           3
#define I2C_CONFIG0          6
#define I2C_CONFIG1          7

#define I2CGPIO_INPUTVALUE _IOR('r', 0, __u8)
#define I2CGPIO_OUTPUTVALUE _IOR('r', 1, __u8)

// FIXME: Major number might not be correct.
#define UMAJOR 237
#define NMINOR 1
#define DESCRIPTION "I2C GPIO expander driver for VPN"
#define VERSION "1:2.0-absolutetrac"

static const struct i2c_device_id i2cgpio_id[] = {
	{ "i2cgpio", 16, },
	{ }
};

MODULE_DEVICE_TABLE(i2c, i2cgpio_id);

static struct i2cgpio
{
	struct work_struct work;
	struct kfifo fifo;
	spinlock_t fifo_lock;
	wait_queue_head_t fifo_proc_list;
	int open_count;

	struct i2c_client* client;
	struct mutex lock;
	int irqnum;
	int exiting;
} g_i2cgpio;

static int i2cgpio_write_reg(int reg, u8 val)
{
	const int error = i2c_smbus_write_byte_data(g_i2cgpio.client, reg, val);

	if (error < 0)
	{
		dev_err(&g_i2cgpio.client->dev,
			"%s failed, reg: %d, val: %d, error: %d\n",
			__func__, reg, val, error);
		return error;
	}

	return 0;
}

static int i2cgpio_read_reg(int reg, u8 *val)
{
	const int retval = i2c_smbus_read_byte_data(g_i2cgpio.client, reg);

	if (retval < 0)
	{
		dev_err(&g_i2cgpio.client->dev, "%s failed, reg: %d, error: %d\n",
			__func__, reg, retval);
		return retval;
	}

	*val = (u8)retval;
	return 0;
}

static int set_config_reg(void)
{
	int stat = i2cgpio_write_reg(I2C_CONFIG0, 0x3F);//all undefined pin(p06-p07) should be set to out.

	if (stat < 0)
	{
		return stat;
	}

	stat = i2cgpio_write_reg(I2C_CONFIG1, 0x00);//all undefined pin(p16-p17) should be set to out.

	if (stat < 0)
	{
		return stat;
	}

	stat = i2cgpio_write_reg(I2C_OUTPUT, 0x00);

	if(stat < 0)
	{
		return stat;
	}

	return 0;
}

static irqreturn_t i2cgpio_irq(int irq, void* dev_id)
{
	disable_irq_nosync(irq);
	schedule_work(&g_i2cgpio.work);
	return IRQ_HANDLED;
}

static void i2cgpio_work(struct work_struct* work)
{
	u8 event = 0;
	mutex_lock(&g_i2cgpio.lock);

	event = i2c_smbus_read_byte_data(g_i2cgpio.client, I2C_INPUT);

	kfifo_in_locked(&g_i2cgpio.fifo, (unsigned char*)&event,
			sizeof(event), &g_i2cgpio.fifo_lock);

	wake_up_interruptible(&g_i2cgpio.fifo_proc_list);

	if(!g_i2cgpio.exiting)
	{
		enable_irq(g_i2cgpio.irqnum);
	}

	mutex_unlock(&g_i2cgpio.lock);
}

static int my_open(struct inode* inode, struct file* filp)
{
	mutex_lock(&g_i2cgpio.lock);

	if(!(g_i2cgpio.open_count))
	{
		enable_irq(g_i2cgpio.irqnum);
		/* Flush input queue on first open */
		kfifo_reset(&g_i2cgpio.fifo);
	}

	g_i2cgpio.open_count++;

	mutex_unlock(&g_i2cgpio.lock);
	return 0;
}

static int my_release(struct inode* inode, struct file* filp)
{
	mutex_lock(&g_i2cgpio.lock);
	g_i2cgpio.open_count--;

	if(!(g_i2cgpio.open_count))
	{
		disable_irq(g_i2cgpio.irqnum);
	}

	mutex_unlock(&g_i2cgpio.lock);
	return 0;
}

static ssize_t my_write(
	struct file* p_file,
	const char __user* p_user,
	size_t p_len,
	loff_t* p_off)
{
	const int len = p_len;
	unsigned long  value;
	char buf[32];

	if(len > 32)
	{
		return -EINVAL;
	}

	if(copy_from_user(buf, p_user, len))
	{
		return -EFAULT;
	}

	value = simple_strtoul(buf, NULL, 0);

	if(i2cgpio_write_reg(I2C_OUTPUT, value&0xFF))
	{
		return -EFAULT;
	}

	return len;
}

static ssize_t my_read(
	struct file* p_file,
	char __user* p_user,
	size_t p_len,
	loff_t* p_off)
{
	ssize_t ret;
	unsigned char c;

	if((kfifo_len(&g_i2cgpio.fifo) == 0) && (p_file->f_flags & O_NONBLOCK))
	{
		return -EAGAIN;
	}

	ret = wait_event_interruptible(g_i2cgpio.fifo_proc_list, kfifo_len(&g_i2cgpio.fifo) != 0);

	if(ret)
	{
		return ret;
	}

	while(ret < p_len && (kfifo_out_locked(&g_i2cgpio.fifo, &c, sizeof(c), &g_i2cgpio.fifo_lock) == sizeof(c)))
	{
		char buf[64] = {0};
		const int out_len = sprintf(buf, "%d\n", c);

		if(copy_to_user(p_user, buf, out_len))
		{
			return -EFAULT;
		}

		ret+=out_len;
	}

	if(ret > 0)
	{
		struct inode* inode = p_file->f_path.dentry->d_inode;
		inode->i_atime = current_fs_time(inode->i_sb);
	}

	return ret;
}

static int i2cgpio_probe(struct i2c_client* p_client,
				   const struct i2c_device_id* p_id)
{
	int ret;
	memset(&g_i2cgpio, 0, sizeof(g_i2cgpio));
	g_i2cgpio.exiting = 0;
	g_i2cgpio.client = p_client;
	i2c_set_clientdata(p_client, &g_i2cgpio);

	spin_lock_init(&g_i2cgpio.fifo_lock);
	ret = kfifo_alloc(&g_i2cgpio.fifo, 128, GFP_KERNEL);

	if(ret)
	{
		printk(KERN_ERR "i2cgpio: kfifo_alloc failed\n");
		return ret;
	}

	init_waitqueue_head(&g_i2cgpio.fifo_proc_list);

	INIT_WORK(&g_i2cgpio.work, i2cgpio_work);
	mutex_init(&g_i2cgpio.lock);

	g_i2cgpio.irqnum = gpio_to_irq(MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(3, 3)));

	if(g_i2cgpio.irqnum < 0)
	{
		ret = g_i2cgpio.irqnum;
		dev_err(&g_i2cgpio.client->dev,"could not get irq!");
		goto out_free;
	}

	if(g_i2cgpio.irqnum > 0)
	{
		ret = request_threaded_irq(g_i2cgpio.irqnum, NULL, i2cgpio_irq, IRQF_TRIGGER_FALLING, "i2cgpio", p_client);

		if (ret < 0)
		{
			dev_err(&g_i2cgpio.client->dev,"request_irq() failed: %d", ret);
			goto out_free;
		}

	}

	disable_irq(g_i2cgpio.irqnum);

	if(set_config_reg())
	{
//		goto out_irq;
	}

	printk(KERN_NOTICE "\x1b[1;31;44mRedStone: \x1b[37m" DESCRIPTION ", version " VERSION "\x1b[0m\n");

	return 0;

out_irq:

	if(g_i2cgpio.irqnum > 0)
	{
		free_irq(g_i2cgpio.irqnum, p_client);
	}

out_free:
	return ret;
}

static long my_ioctl(struct file* fp, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	u8 val8;
	void __user* argp = (void __user*)arg;

	mutex_lock(&g_i2cgpio.lock);

	switch (cmd)
	{
	case I2CGPIO_INPUTVALUE:

		if (i2cgpio_read_reg(I2C_INPUT, &val8))
		{
			ret = -EIO;
			break;
		}

		if (copy_to_user(argp, &val8, sizeof(val8)))
		{
			ret = -EFAULT;
		}

		break;

	case I2CGPIO_OUTPUTVALUE:

		if (i2cgpio_read_reg(I2C_OUTPUT, &val8))
		{
			ret = -EIO;
			break;
		}

		if (copy_to_user(argp, &val8, sizeof(val8)))
		{
			ret = -EFAULT;
		}

		break;

	default:
		ret = -EINVAL;
	}

	mutex_unlock(&g_i2cgpio.lock);
	return ret;
}

static int i2cgpio_remove(struct i2c_client* client)
{

	if(g_i2cgpio.irqnum > 0)
	{
		mutex_lock(&g_i2cgpio.lock);
		g_i2cgpio.exiting = 1;
		disable_irq(g_i2cgpio.irqnum);
		mutex_unlock(&g_i2cgpio.lock);

		free_irq(g_i2cgpio.irqnum, client);
		flush_scheduled_work();
	}

	kfifo_free(&g_i2cgpio.fifo);
	return 0;
}

static struct i2c_driver i2cgpio_driver = {
	.driver = {
		.name	= "i2cgpio",
	},
	.probe		= i2cgpio_probe,
	.remove		= __devexit_p(i2cgpio_remove),
	.id_table	= i2cgpio_id,

};

static const struct file_operations my_fops = {
	.open		= my_open,
	.write		= my_write,
	.read		= my_read,
	.release	= my_release,
	.unlocked_ioctl	= my_ioctl,
	.owner		= THIS_MODULE,
};

static struct cdev set_i2cgpio_cdev = {
	.kobj	=	{.name = "i2cgpio", },
	.owner	=	THIS_MODULE,
};

static int __init i2cgpio_init(void)
{
	const dev_t dev = MKDEV( UMAJOR, 0);
	int ret;

	ret = register_chrdev_region( dev, NMINOR, "i2cgpio");

	if(ret)
	{
		printk( "%s,%d: (%d) register_chrdev_region( 0x%X, %d) failed\n", __FILE__, __LINE__, ret, dev, NMINOR);
		return ret;
	}

	cdev_init( &set_i2cgpio_cdev, &my_fops);
	{
		const int ret = cdev_add( &set_i2cgpio_cdev, dev, NMINOR);

		if(ret)
		{
			printk( "%s,%d: (%d) cdev_add( %p, %p, %d) failed\n", __FILE__, __LINE__, ret, &set_i2cgpio_cdev, &dev, NMINOR);
			kobject_put( &set_i2cgpio_cdev.kobj);
			unregister_chrdev_region( dev, NMINOR);
			return ret;
		}

	}

	gpio_request(MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(3, 3)), "i2cgpio");
	gpio_direction_input(MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(3, 3)));
	gpio_export(MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(3, 3)), true);

	return i2c_add_driver(&i2cgpio_driver);
}

module_init(i2cgpio_init);

static void __exit i2cgpio_fini(void)
{
	gpio_free( MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(3, 3)));
	i2c_del_driver(&i2cgpio_driver);
	cdev_del( &set_i2cgpio_cdev);
	unregister_chrdev_region( MKDEV( UMAJOR, 0), NMINOR);
}

module_exit(i2cgpio_fini);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_AUTHOR("Lee Wei <lee@absolutetrac.com>");
MODULE_VERSION(VERSION);
