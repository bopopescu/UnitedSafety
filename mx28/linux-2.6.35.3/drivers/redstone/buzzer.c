/*
 * Author: Amour Hassan <amour@absolutetrac.com>
 * Date: August 22, 2013
 * Copyright 2010-2011 Siconix
 * Copyright 2010-2013 Absolutetrac
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

#include <mach/gpio.h>
#include <mach/pinctrl.h>
#include <mach/../../../mach-mx28/mx28_pins.h>
#include <mach/../../../mach-mx28/regs-pinctrl.h>

// FIXME: Major number might not be correct.
#define UMAJOR 239
#define NMINOR 1

static int g_dbg = 0;
static wait_queue_head_t g_driver;
static atomic_t g_work;
static unsigned int g_driver_open = 0;
static unsigned long g_ondelay = 0;
static unsigned long g_offdelay = 0;
static unsigned long g_timeout = 0;

/* Description: Module Kernel command line paramter.
 *
 * Format: redstone_buzzer=<debug level>[,future use,...,future use]
 *
 */
static int __init redstone_buzzer(char* p_str)
{
	g_dbg = simple_strtol(p_str, 0, 0);
	return 0;
}

__setup("redstone_buzzer=", redstone_buzzer);

static int my_open( struct inode *inode, struct file *filp)
{

	if(g_driver_open)
		return -EBUSY;

	g_driver_open = (unsigned int)filp;
	return 0;
}

static ssize_t my_write(
	struct file *p_file,
	const char __user *p_user,
	size_t p_len,
	loff_t *p_off)
{
	const char *arg_buf = p_user; 
	char buf[64];
	int ondelay_len = 0;
	int offdelay_len = 0;
	int timeout_len = 0;
	int next=0;
	const int len = p_len;
	g_ondelay = 0;
	g_offdelay = 0;
	g_timeout = 0;
	memset(buf, 0, 64);

	while(p_len--) {
		char c;
		copy_from_user(&c, p_user++, 1);
		switch(next) {
		case 0:
			if(ondelay_len < 63)
				ondelay_len++;
			if(c == ',') {
				copy_from_user(buf, arg_buf, ondelay_len);
				g_ondelay = simple_strtoul(buf, NULL, 0);
				arg_buf = p_user;
				next++;
				memset(buf, 0, 64);
			}	
			break;
		case 1:
			if(offdelay_len < 63)
				offdelay_len++;
			if(c == ',') {
				copy_from_user(buf, arg_buf, offdelay_len);
				g_offdelay = simple_strtoul(buf, NULL, 0);
				arg_buf = p_user;
				next++;
				memset(buf, 0, 64);
			}
			break;
		case 2:
			if(timeout_len < 63)
				timeout_len++;

		}

	}

	switch(next)
	{
	case 0: 
		copy_from_user(buf, arg_buf, ondelay_len);
		g_ondelay = simple_strtoul(buf, NULL, 0);
		break;
	case 1:
		copy_from_user(buf, arg_buf, offdelay_len);
		g_offdelay = simple_strtoul(buf, NULL, 0);
		break;
	case 2: 
		copy_from_user(buf, arg_buf, timeout_len);
		g_timeout = simple_strtoul(buf, NULL, 0);
	}

	atomic_set(&g_work, 1);
	wake_up(&g_driver);
	wait_event(g_driver, atomic_read(&g_work) == 0);

	if(g_dbg)
	{
		printk(KERN_DEBUG "Continuing buzzer driver\n");
	}

	return len;
}

static ssize_t my_read(
	struct file *p_file,
	char __user *p_user,
	size_t p_len,
	loff_t *p_off)
{

	char buf[32];
	memset(buf, 0 , 32);
	snprintf(buf,32,"%lu,%lu\n", g_ondelay, g_offdelay);

	if(copy_to_user(p_user, buf, strlen(buf)))
	{
		return -EFAULT;
	}

	return strlen(buf);
}

static int my_close(struct inode *inode, struct file *filp)
{

	if(g_driver_open == (unsigned int)filp)
	{
		g_driver_open = 0;
	}

	return 0;
}


static const struct file_operations my_fops = {
	.open		= my_open,
	.write		= my_write,
	.read		= my_read,
	.release	= my_close,
	.owner		= THIS_MODULE,
};

static struct cdev set_rssi_cdev = {
	.kobj	=	{.name = "set_buzzer", },
	.owner	=	THIS_MODULE,
};

//***************************
//TODO: Rename variable to make more sense

static struct workqueue_struct *my_wq;

typedef struct {
	struct work_struct my_work;
	int x;
} my_work_t;

my_work_t *work;

struct buzzer {
	struct work_struct my_work;
	struct workqueue_struct *my_wq2;
	atomic_t run_flag;
	atomic_t can_run;
};

static void my_wq_function2( struct work_struct *work)
{
	struct buzzer *b = container_of(work, struct buzzer, my_work);

	if(atomic_read(&(b->can_run)) == 0)
	{
		return;
	}

	for(;;)
	{
		unsigned int toggle = gpio_get_value(MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(3, 21)));
		unsigned long d;
		atomic_set(&(b->run_flag), 0);

		if(toggle)
		{
			toggle=0;
			d = g_offdelay;
		}
		else
		{	 
			toggle=1;
			d = g_ondelay;
		}

		gpio_direction_output(MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(3, 21)), toggle);
		wait_event_timeout(g_driver, atomic_read(&(b->run_flag)) == 1, usecs_to_jiffies(d));

		if(atomic_read(&(b->can_run)) == 0)
		{
			break;
		}

	}

	return;
}

static void my_wq_function( struct work_struct *work)
{
	my_work_t *my_work = (my_work_t *)work;
	struct buzzer buzz;
	buzz.my_wq2 = create_workqueue("my_queue2");

	for(;;)
	{

		if(g_dbg)
		{
			printk(KERN_DEBUG "my_work.x %d, ondelay=%lu offdelay=%lu timeout=%lu\n", my_work->x, g_ondelay, g_offdelay, g_timeout);
		}

		wait_event(g_driver, atomic_read(&g_work) == 1);

		if(g_ondelay)
		{
			//turn on buzzer
			if(!g_offdelay)
			{
				gpio_direction_output(MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(3, 21)), 1);
				atomic_set(&(buzz.can_run),0);
				atomic_set(&(buzz.run_flag),1);
				wake_up(&g_driver);
			}
			else
			{
				atomic_set(&(buzz.can_run), 1);

  				if (buzz.my_wq2)
				{

					int ret;
      					INIT_WORK( (struct work_struct *)&buzz, my_wq_function2 );
      					ret = queue_work( buzz.my_wq2, (struct work_struct *)&buzz );
    				}

			}

		}
		else
		{
			//turn off buzzer
			gpio_direction_output(MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(3, 21)), 0);
			atomic_set(&(buzz.can_run),0);
			atomic_set(&(buzz.run_flag),1);
			wake_up(&g_driver);
		}

		atomic_set(&g_work, 0);
		wake_up(&g_driver);
	}

	flush_workqueue( buzz.my_wq2 );
	destroy_workqueue( buzz.my_wq2 );

	kfree( (void *)work );
	return;
}

static int __init init(void)
{
	const dev_t dev = MKDEV( UMAJOR, 0);
	int ret = register_chrdev_region( dev, NMINOR, "set_buzzer");

	if( ret)
	{
		printk(KERN_ERR "%s,%d: (%d) register_chrdev_region( 0x%X, %d) failed\n", __FILE__, __LINE__, ret, dev, NMINOR);
		return ret;
	}

	cdev_init( &set_rssi_cdev, &my_fops);
	{

		if((ret = cdev_add( &set_rssi_cdev, dev, NMINOR)))
		{
			printk(KERN_ERR "%s,%d: (%d) cdev_add( %p, %p, %d) failed\n", __FILE__, __LINE__, ret, &set_rssi_cdev, &dev, NMINOR);
			kobject_put( &set_rssi_cdev.kobj);
			unregister_chrdev_region( dev, NMINOR);
			return ret;
		}

	}

	gpio_request(MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(3, 21)), "Buzzer");
	gpio_direction_output(MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(3, 21)), 0);
	init_waitqueue_head(&g_driver);
	atomic_set(&g_work, 0);
	my_wq = create_workqueue("my_queue");

	if (my_wq)
	{
		/* Queue some work (item 1) */
		work = (my_work_t *)kmalloc(sizeof(my_work_t), GFP_KERNEL);

		if(work)
		{
			INIT_WORK( (struct work_struct *)work, my_wq_function );
			work->x = 1;

			if(!queue_work( my_wq, (struct work_struct *)work ))
			{
				printk(KERN_ERR "%s,%d: Failed to queue work\n", __FILE__, __LINE__);
			}

		}

	}

	printk(KERN_INFO "TRULink Buzzer driver loaded\n");
	return 0;
}

static void __exit fini(void)
{
	gpio_free( MXS_PIN_TO_GPIO(MXS_PIN_ENCODE(3, 21)));

	cdev_del( &set_rssi_cdev);
	unregister_chrdev_region( MKDEV( UMAJOR, 0), NMINOR);

	flush_workqueue( my_wq );

	destroy_workqueue( my_wq );
	printk(KERN_INFO "TRULink Buzzer driver unloaded\n");
}

module_init(init);
module_exit(fini);

MODULE_LICENSE( "GPL");
MODULE_DESCRIPTION( "Buzzer driver for TRULink");
MODULE_AUTHOR(	"Tyson Pullukatt <tyson@absolutetrac.com>");
MODULE_VERSION( "1:5.0-absolutetrac");
