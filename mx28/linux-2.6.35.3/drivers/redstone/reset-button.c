/*
 * Author: Amour Hassan <amour@absolutetrac.com>
 * Date: June 17, 2013
 * Copyright 2010-2011 Siconix
 * Copyright 2010-2013 Absolutetrac
 *
 * Modified by Dave Huff - Dec 2016
 * New behaviour - sends '0', '1', '2', or '3' instead of original letters.
 *    0 - button not pressed 
 *    1 - button pressed < 3 seconds (release at this point does nothing) 				- solid orange power button
 *    2 - button pressed for > 3 seconds < 10) - release causes a reboot 					- quick flash orange
 *    3 - button pressed >10 and <20 - release causes factory restore							- slow flash orange
 *    4 - button pressed >20 - release causes factory restore and db-config wipe	- alternate 
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

#define UMAJOR 235
#define NMINOR 1

#define DESCRIPTION "RESET Button Driver"
#define VERSION "1:1.0-absolutetrac"

DECLARE_KFIFO(fifo, 32) g_event;

struct task_struct* g_sleeping_task = 0;

static struct mutex g_lock;

wait_queue_head_t g_wq_head;
static struct workqueue_struct *g_wq;

static struct work_context
{
	struct work_struct m_ws;

} g_work_context;

static atomic_t g_atomic_irq;
static atomic_t* g_irq = &g_atomic_irq;
static int g_irq_num = 0;
static DECLARE_WAIT_QUEUE_HEAD(g_q);
static int g_dbg = 0;
static struct file* g_file = 0;
static int g_pin = MXS_PIN_ENCODE(1, 12);

static struct timespec g_press_time;

static int g_reboot_timeout = 3;
static int g_factory_restore_timeout = 10;
static int g_full_restore_timeout = 20;

void on_timer(unsigned long p_data)
{
	atomic_set(g_irq, 2);
	wake_up(&g_wq_head);
}

// Description: Queues a reset-button event.
//
// XXX: Lock "g_lock" must be held before calling this function.
static void queue_event(char p_event)
{

	if(kfifo_len(&(g_event.fifo)) < 32)
	{
		kfifo_in(&(g_event.fifo), &p_event, 1);

		if(g_sleeping_task)
		{
			wake_up_process(g_sleeping_task);
			g_sleeping_task = 0;
		}

	}

}

static void wq_fn( struct work_struct *work)
{
	struct timer_list timer;
	int ret;

	for(;;)
	{
		wait_event(g_wq_head, atomic_read(g_irq) == 1);
		mutex_lock(&g_lock);
		queue_event('0');
		mutex_unlock(&g_lock);

		{
			static struct timespec t;
			static struct timespec* i = &g_press_time;
			static struct timespec* f = &t;
			char pending_action = '\0';
			int on = gpio_get_value(MXS_PIN_TO_GPIO(g_pin)) ? 0 : 1;

			while(on)
			{
				init_timer(&timer);
				setup_timer(&timer, on_timer, 0);
				ret = mod_timer(&timer, jiffies + msecs_to_jiffies(63));
				wait_event(g_wq_head, atomic_read(g_irq) == 2);
				getrawmonotonic(&t);
				on = gpio_get_value(MXS_PIN_TO_GPIO(g_pin)) ? 0 : 1;
				del_timer(&timer);
				atomic_set(g_irq, 1);

				{
					const int diff = f->tv_sec - i->tv_sec;
					char action = '\0';

					
					if(diff < g_reboot_timeout)
					{
						action = '1';  // will do nothing if released
					}
					else if(diff < g_factory_restore_timeout)
					{
						action = '2';  // will do reboot if released
					}
					else if(diff < g_full_restore_timeout)
					{
						action = '3';  // will do factory restore if released
					}
					else 
					{
						action = '4';
					}

					if(action != pending_action)
					{
						pending_action = action;
						mutex_lock(&g_lock);
						queue_event(pending_action);
						mutex_unlock(&g_lock);
					}

				}

			}

			mutex_lock(&g_lock);
			queue_event('0');
			mutex_unlock(&g_lock);
		}

		atomic_set(g_irq, 0);
	}

}

static irqreturn_t irq_fn(int irq, void* dev_id)
{

	if(!atomic_read(g_irq))
	{
		getrawmonotonic(&g_press_time);
		atomic_set(g_irq, 1);
		wake_up(&g_wq_head);
	}

        return IRQ_HANDLED;
}

static int my_open( struct inode* inode, struct file* p_file)
{
	mutex_lock(&g_lock);

	if(g_file)
	{
		mutex_unlock(&g_lock);
		return -EBUSY;
	}

	g_file = p_file;
	mutex_unlock(&g_lock);

	return 0;
}

/* ATS FIXME: This string/command parsing code is modular, and should be moved into its own
 *	source file/library (for use in other RedStone drivers).
 */
#define max_key_len 64
#define max_val_len 256
static char g_key[max_key_len];
static char g_val[max_val_len];
static size_t g_key_i = 0;
static size_t g_val_i = 0;

static enum
{
	READ_KEY,
	READ_VAL,
	PROCESS_COMMAND
} g_state = READ_KEY;

static void reset_parser_state_machine(void)
{
	g_key[0] = '\0';
	g_val[0] = '\0';
	g_key_i = 0;
	g_val_i = 0;
	g_state = READ_KEY;
}

static int process_command(void)
{

	if(g_dbg >= 3)
	{
		printk(KERN_NOTICE "%s,%d: key=\"%s\", val=\"%s\"\n", __FILE__, __LINE__, g_key, g_val);
	}

	if((!strcmp("debug", g_key)) || (!strcmp("dbg", g_key)))
	{
		g_dbg = simple_strtol(g_val, 0, 0);

		if(g_dbg)
		{
			printk(KERN_NOTICE "%s,%d: Debugging is ON\n", __FILE__, __LINE__);
		}
		else
		{
			printk(KERN_NOTICE "%s,%d: Debugging is OFF\n", __FILE__, __LINE__);
		}

	}
	else if(!strcmp("g_factory_restore_timeout", g_key))
	{
		g_factory_restore_timeout = simple_strtol(g_val, 0, 0);

		if(g_factory_restore_timeout < 1)
		{
			g_factory_restore_timeout = 10;
		}

	}
	else if(!strcmp("g_reboot_timeout", g_key))
	{
		g_reboot_timeout = simple_strtol(g_val, 0, 0);

		if(g_reboot_timeout < 1)
		{
			g_reboot_timeout = 3;
		}

	}
	else if(!strcmp("g_full_restore_timeout", g_key))
	{
		g_full_restore_timeout = simple_strtol(g_val, 0, 0);

		if(g_full_restore_timeout < 1)
		{
			g_full_restore_timeout = 20;
		}

	}
	else if(!strcmp("status", g_key))
	{
		printk(KERN_INFO "%s: g_full_restore_timeout=%d\n", DESCRIPTION, g_full_restore_timeout);
		printk(KERN_INFO "%s: g_factory_restore_timeout=%d\n", DESCRIPTION, g_factory_restore_timeout);
		printk(KERN_INFO "%s: g_reboot_timeout=%d\n", DESCRIPTION, g_reboot_timeout);
	}
	else
	{

		if(g_dbg >= 1)
		{
			printk(KERN_NOTICE "%s,%d: Invalid key=\"%s\"\n", __FILE__, __LINE__, g_key);
		}

		return -EINVAL;
	}

	return 0;
}

static int read_from_user(
	char* p_c,
	size_t* i,
	const char __user** p_user)
{

	if(!(*i))
	{
		return 0;
	}

	--(*i);

	if(copy_from_user(p_c, (*p_user)++, 1))
	{
		return -EFAULT;
	}

	return 1;
}

static ssize_t my_write(
	struct file* p_file,
	const char __user* p_user,
	size_t p_len,
	loff_t* p_off)
{
	size_t i = p_len;

	if(!i)
	{
		return 0;
	}

	for(;;)
	{
		char c;

		if(g_dbg >= 5)
		{
			printk(KERN_INFO "%s,%d: c=%02X, g_state=%d, g_key_i=%d, g_val_i=%d\n", __FILE__, __LINE__, c, g_state, g_key_i, g_val_i);
		}

		if((READ_KEY == g_state) || (READ_VAL == g_state))
		{
			const int ret = read_from_user(&c, &i, &p_user);

			if(ret < 0)
			{
				reset_parser_state_machine();
				return ret;
			}

			if(!ret)
			{
				break;
			}

		}

		switch(g_state)
		{
		case READ_KEY:

			if('=' == c)
			{
				g_key[g_key_i] = '\0';
				g_state = READ_VAL;
				break;
			}

			if('\n' == c)
			{
				g_key[g_key_i] = '\0';
				g_state = PROCESS_COMMAND;
				break;
			}

			if(g_key_i >= (max_key_len - 1))
			{
				reset_parser_state_machine();
				return -EINVAL;
			}

			g_key[g_key_i++] = c;

			break;

		case READ_VAL:

			if('\n' == c)
			{
				g_val[g_val_i] = '\0';
				g_state = PROCESS_COMMAND;
				break;
			}

			if(g_val_i >= (max_val_len - 1))
			{
				reset_parser_state_machine();
				return -EINVAL;
			}

			g_val[g_val_i++] = c;
			break;

		case PROCESS_COMMAND:
			{
				const int ret = process_command();
				reset_parser_state_machine();

				if(ret)
				{
					return ret;
				}

			}
			break;
		}

	}

	return p_len - i;
}

static ssize_t my_read(
	struct file* p_file,
	char __user* p_user,
	size_t p_len,
	loff_t* p_off)
{
	int first_check = 1;

	if(p_len < 2)
	{
		return -EINVAL;
	}

	for(;;)
	{
		mutex_lock(&g_lock);

		if(kfifo_len(&(g_event.fifo)))
		{
			char event;

			kfifo_out(&(g_event.fifo), &event, 1);
			mutex_unlock(&g_lock);

			if(copy_to_user(p_user, &event, 1))
			{
				return -EFAULT;
			}

			break;
		}
		else
		{

			if(!first_check)
			{
				mutex_unlock(&g_lock);
				return -EAGAIN;
			}

			g_sleeping_task = current;
			set_current_state(TASK_INTERRUPTIBLE);
			mutex_unlock(&g_lock);
			schedule();
		}

		first_check = 0;
	}

	return 1;
}

static int my_release(struct inode* inode, struct file* filp)
{
	mutex_lock(&g_lock);
	g_file = 0;
	g_key_i = 0;
	g_val_i = 0;
	g_state = READ_KEY;
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
	.kobj	=	{.name = "redstone-reset-button", },
	.owner	=	THIS_MODULE,
};

static int __init init(void)
{
	const dev_t dev = MKDEV(UMAJOR, 0);
	int ret;

	mutex_init(&g_lock);
	atomic_set(g_irq, 0);
	ret = register_chrdev_region( dev, NMINOR, "redstone-reset-button");

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

	gpio_request(MXS_PIN_TO_GPIO(g_pin), "redstone-reset-button");
	mxs_set_pullup(g_pin, 1, "gpio");

	INIT_KFIFO(g_event.fifo);

	init_waitqueue_head(&g_wq_head);
	g_wq = create_workqueue("reset-button-wq");
	INIT_WORK((struct work_struct*)&g_work_context, wq_fn);
	queue_work(g_wq, (struct work_struct*)&g_work_context);

	gpio_direction_input(MXS_PIN_TO_GPIO(g_pin));
	gpio_export(MXS_PIN_TO_GPIO(g_pin), true);

	{
		const int irq = g_irq_num = gpio_to_irq(MXS_PIN_TO_GPIO(g_pin));
		const int ret = request_irq(irq, irq_fn, IRQF_TRIGGER_FALLING, "redstone-reset-button", init);

		if (ret < 0)
		{
			printk(KERN_ERR "%s,%d: request_irq() failed: %d\n", __FILE__, __LINE__, ret);
			return ret;
		}

	}

	disable_irq(g_irq_num);
	enable_irq(g_irq_num);
	printk(KERN_NOTICE "TRULink %s, %s loaded\n", DESCRIPTION, VERSION);

	return 0;
}

module_init(init);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_AUTHOR("Amour Hassan <amour@absolutetrac.com>");
MODULE_VERSION(VERSION);
