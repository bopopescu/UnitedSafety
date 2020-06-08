/*
 * mx-cputemp.c - Driver for mx CPU core temperature monitoring
 *
 *
 * based on existing coretemp.c, which is
 *
 * Copyright (C) 2007 Rudolf Marek <r.marek@assembler.cz>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/hwmon.h>
#include <linux/sysfs.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/cpu.h>
#include <linux/io.h>
#include <asm/processor.h>

#include <mach/hardware.h>
#include <mach/lradc.h>
#include <mach/regs-lradc.h>

#define DRVNAME	"mx_cputemp"

#define GAIN_CORRECTION 1012    /* 1.012 */
#define REGS_LRADC_BASE IO_ADDRESS(LRADC_PHYS_ADDR)

/*
 * Use the the lradc channel
 * get the die temperature from on-chip sensor.
 */
uint16_t MeasureInternalDieTemperature(void)
{
	uint32_t  ch8Value, ch9Value, lradc_irq_mask, channel;

	channel = 0; /* LRADC 0 */
	lradc_irq_mask = 1 << channel;

	/* power up internal tep sensor block */
	__raw_writel(BM_LRADC_CTRL2_TEMPSENSE_PWD,
			REGS_LRADC_BASE + HW_LRADC_CTRL2_CLR);

	/* mux to the lradc 8th temp channel */
	__raw_writel((0xF << (4 * channel)),
			REGS_LRADC_BASE + HW_LRADC_CTRL4_CLR);
	__raw_writel((8 << (4 * channel)),
			REGS_LRADC_BASE + HW_LRADC_CTRL4_SET);

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

	/* read temperature value and clr lradc */
	ch8Value = __raw_readl(REGS_LRADC_BASE +
			HW_LRADC_CHn(channel)) & BM_LRADC_CHn_VALUE;


	__raw_writel(BM_LRADC_CHn_VALUE,
			REGS_LRADC_BASE + HW_LRADC_CHn_CLR(channel));

	/* mux to the lradc 9th temp channel */
	__raw_writel((0xF << (4 * channel)),
			REGS_LRADC_BASE + HW_LRADC_CTRL4_CLR);
	__raw_writel((9 << (4 * channel)),
			REGS_LRADC_BASE + HW_LRADC_CTRL4_SET);

	/* Clear the interrupt flag */
	__raw_writel(lradc_irq_mask,
			REGS_LRADC_BASE + HW_LRADC_CTRL1_CLR);
	__raw_writel(BF_LRADC_CTRL0_SCHEDULE(1 << channel),
			REGS_LRADC_BASE + HW_LRADC_CTRL0_SET);
	/* Wait for conversion complete */
	while (!(__raw_readl(REGS_LRADC_BASE + HW_LRADC_CTRL1)
			& lradc_irq_mask))
		cpu_relax();

	/* Clear the interrupt flag */
	__raw_writel(lradc_irq_mask,
			REGS_LRADC_BASE + HW_LRADC_CTRL1_CLR);
	/* read temperature value */
	ch9Value = __raw_readl(
			REGS_LRADC_BASE + HW_LRADC_CHn(channel))
		  & BM_LRADC_CHn_VALUE;


	__raw_writel(BM_LRADC_CHn_VALUE,
			REGS_LRADC_BASE + HW_LRADC_CHn_CLR(channel));

	/* power down temp sensor block */
	__raw_writel(BM_LRADC_CTRL2_TEMPSENSE_PWD,
			REGS_LRADC_BASE + HW_LRADC_CTRL2_SET);


	return (uint16_t)((ch9Value-ch8Value)*GAIN_CORRECTION/4000);
}

/*  */
/* brief Report on the die temperature. */
/*  */
/* fntype Function */
/*  */
/*  This function reports on the die temperature. */
/*  */
/* param[out]  pLow   The low  end of the temperature range. */
/* param[out]  pHigh  The high end of the temperature range. */
/*  */

/* Temperature constant */
#define TEMP_READING_ERROR_MARGIN 5
#define KELVIN_TO_CELSIUS_CONST 273

extern void (*redstone_ddi_power_GetDieTemp)(int16_t*, int16_t*);

void ddi_power_GetDieTemp(int16_t *pLow, int16_t *pHigh)
{
	int16_t i16High, i16Low;
	uint16_t u16Reading;

	/* Get the reading in Kelvins */
	u16Reading = MeasureInternalDieTemperature();

	/* Adjust for error margin */
	i16High = u16Reading + TEMP_READING_ERROR_MARGIN;
	i16Low  = u16Reading - TEMP_READING_ERROR_MARGIN;

	/* Convert to Celsius */
	i16High -= KELVIN_TO_CELSIUS_CONST;
	i16Low  -= KELVIN_TO_CELSIUS_CONST;

	/* Return the results */
	*pHigh = i16High;
	*pLow  = i16Low;
}

/*
void redstone_ddi_power_GetDieTemp(int16_t *pLow, int16_t *pHigh)
{
	ddi_power_GetDieTemp(pLow, pHigh);
}

EXPORT_SYMBOL_GPL(redstone_ddi_power_GetDieTemp);
*/


enum { SHOW_TEMP, SHOW_LABEL, SHOW_NAME } SHOW;

/*
 * Functions declaration
 */

struct mx_cputemp_data {
	struct device *hwmon_dev;
	const char *name;
	u32 id;
	u32 msr;
};

/*
 * Sysfs stuff
 */

static ssize_t show_name(struct device *dev, struct device_attribute
			  *devattr, char *buf)
{
	int ret;
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	struct mx_cputemp_data *data = dev_get_drvdata(dev);

	if (attr->index == SHOW_NAME)
		ret = sprintf(buf, "%s\n", data->name);
	else	/* show label */
		ret = sprintf(buf, "Core %d\n", data->id);
	return ret;
}

static ssize_t show_temp(struct device *dev,
			 struct device_attribute *devattr, char *buf)
{
	struct mx_cputemp_data *data = dev_get_drvdata(dev);
	int16_t temp_lo, temp_hi;

	ddi_power_GetDieTemp(&temp_lo, &temp_hi);
	
	return sprintf(buf, "lo:%u hi:%u\n",  temp_lo, temp_hi);
}

static SENSOR_DEVICE_ATTR(temp1_input, S_IRUGO, show_temp, NULL,
			  SHOW_TEMP);
static SENSOR_DEVICE_ATTR(temp1_label, S_IRUGO, show_name, NULL, SHOW_LABEL);
static SENSOR_DEVICE_ATTR(name, S_IRUGO, show_name, NULL, SHOW_NAME);

static struct attribute *mx_cputemp_attributes[] = {
	&sensor_dev_attr_name.dev_attr.attr,
	&sensor_dev_attr_temp1_label.dev_attr.attr,
	&sensor_dev_attr_temp1_input.dev_attr.attr,
	NULL
};

static const struct attribute_group mx_cputemp_group = {
	.attrs = mx_cputemp_attributes,
};

static int __devinit mx_cputemp_probe(struct platform_device *pdev)
{
	struct mx_cputemp_data *data;
	//struct cpuinfo_arm *c = &cpu_data(pdev->id);
	int err;

	data = kzalloc(sizeof(struct mx_cputemp_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		dev_err(&pdev->dev, "Out of memory\n");
		goto exit;
	}

	data->id = pdev->id;
	data->name = "mx_cputemp";
	platform_set_drvdata(pdev, data);

	err = sysfs_create_group(&pdev->dev.kobj, &mx_cputemp_group);
	if (err)
		goto exit_free;

	data->hwmon_dev = hwmon_device_register(&pdev->dev);
	if (IS_ERR(data->hwmon_dev)) {
		err = PTR_ERR(data->hwmon_dev);
		dev_err(&pdev->dev, "Class registration failed (%d)\n",
			err);
		goto exit_remove;
	}

	return 0;

exit_remove:
	sysfs_remove_group(&pdev->dev.kobj, &mx_cputemp_group);
exit_free:
	platform_set_drvdata(pdev, NULL);
	kfree(data);
exit:
	return err;
}

static int __devexit mx_cputemp_remove(struct platform_device *pdev)
{
	struct mx_cputemp_data *data = platform_get_drvdata(pdev);

	hwmon_device_unregister(data->hwmon_dev);
	sysfs_remove_group(&pdev->dev.kobj, &mx_cputemp_group);
	platform_set_drvdata(pdev, NULL);
	kfree(data);
	return 0;
}

static struct platform_driver mx_cputemp_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = DRVNAME,
	},
	.probe = mx_cputemp_probe,
	.remove = __devexit_p(mx_cputemp_remove),
};

struct pdev_entry {
	struct list_head list;
	struct platform_device *pdev;
	unsigned int cpu;
};

static LIST_HEAD(pdev_list);
static DEFINE_MUTEX(pdev_list_mutex);

static int __cpuinit mx_cputemp_device_add(unsigned int cpu)
{
	int err;
	struct platform_device *pdev;
	struct pdev_entry *pdev_entry;

	pdev = platform_device_alloc(DRVNAME, cpu);
	if (!pdev) {
		err = -ENOMEM;
		printk(KERN_ERR DRVNAME ": Device allocation failed\n");
		goto exit;
	}

	pdev_entry = kzalloc(sizeof(struct pdev_entry), GFP_KERNEL);
	if (!pdev_entry) {
		err = -ENOMEM;
		goto exit_device_put;
	}

	err = platform_device_add(pdev);
	if (err) {
		printk(KERN_ERR DRVNAME ": Device addition failed (%d)\n",
		       err);
		goto exit_device_free;
	}

	pdev_entry->pdev = pdev;
	pdev_entry->cpu = cpu;
	mutex_lock(&pdev_list_mutex);
	list_add_tail(&pdev_entry->list, &pdev_list);
	mutex_unlock(&pdev_list_mutex);

	return 0;

exit_device_free:
	kfree(pdev_entry);
exit_device_put:
	platform_device_put(pdev);
exit:
	return err;
}


static int __init mx_cputemp_init(void)
{
	int i, err;
	struct pdev_entry *p, *n;
	err = platform_driver_register(&mx_cputemp_driver);
	if (err)
		goto exit;

	for_each_online_cpu(i) {
		err = mx_cputemp_device_add(i);
		if (err)
			goto exit_devices_unreg;
	}
	if (list_empty(&pdev_list)) {
		err = -ENODEV;
		goto exit_driver_unreg;
	}

	redstone_ddi_power_GetDieTemp = ddi_power_GetDieTemp;

	return 0;

exit_devices_unreg:
	mutex_lock(&pdev_list_mutex);
	list_for_each_entry_safe(p, n, &pdev_list, list) {
		platform_device_unregister(p->pdev);
		list_del(&p->list);
		kfree(p);
	}
	mutex_unlock(&pdev_list_mutex);
exit_driver_unreg:
	platform_driver_unregister(&mx_cputemp_driver);
exit:
	return err;
}

static void __exit mx_cputemp_exit(void)
{
	struct pdev_entry *p, *n;
	mutex_lock(&pdev_list_mutex);
	list_for_each_entry_safe(p, n, &pdev_list, list) {
		platform_device_unregister(p->pdev);
		list_del(&p->list);
		kfree(p);
	}
	mutex_unlock(&pdev_list_mutex);
	platform_driver_unregister(&mx_cputemp_driver);
}

MODULE_AUTHOR("Pierantonio Tabaro <tabarop@vdavda.com>");
MODULE_DESCRIPTION("mx CPU temperature monitor");
MODULE_LICENSE("GPL");

module_init(mx_cputemp_init)
module_exit(mx_cputemp_exit)
