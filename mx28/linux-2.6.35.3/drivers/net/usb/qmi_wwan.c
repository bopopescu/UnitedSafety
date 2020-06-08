/*
 * Copyright (c) 2012  Bjorn Mork <bjorn@mork.no>
 *
 * The probing code is heavily inspired by cdc_ether, which is:
 * Copyright (C) 2003-2005 by David Brownell
 * Copyright (C) 2006 by Ole Andre Vadla Ravnas (ActiveSync)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/usb.h>
#include <linux/usb/cdc.h>
#include <linux/usb/usbnet.h>
//#include <linux/usb/cdc-wdm.h>

/* This driver supports wwan (3G/LTE/?) devices using a vendor
 * specific management protocol called Qualcomm MSM Interface (QMI) -
 * in addition to the more common AT commands over serial interface
 * management
 *
 * QMI is wrapped in CDC, using CDC encapsulated commands on the
 * control ("master") interface of a two-interface CDC Union
 * resembling standard CDC ECM.  The devices do not use the control
 * interface for any other CDC messages.  Most likely because the
 * management protocol is used in place of the standard CDC
 * notifications NOTIFY_NETWORK_CONNECTION and NOTIFY_SPEED_CHANGE
 *
 * Alternatively, control and data functions can be combined in a
 * single USB interface.
 *
 * Handling a protocol like QMI is out of the scope for any driver.
 * It is exported as a character device using the cdc-wdm driver as
 * a subdriver, enabling userspace applications ("modem managers") to
 * handle it.
 *
 * These devices may alternatively/additionally be configured using AT
 * commands on a serial interface
 */




// imported from cdc_wdm.h 3.8
extern struct usb_driver *usb_cdc_wdm_register(struct usb_interface *intf,
                                        struct usb_endpoint_descriptor *ep,
                                        int bufsize,
                                        int (*manage_power)(struct usb_interface *, int));

// imported from device.h
/**
 * module_driver() - Helper macro for drivers that don't do anything
 * special in module init/exit. This eliminates a lot of boilerplate.
 * Each module may only use this macro once, and calling it replaces
 * module_init() and module_exit().
 *
 * @__driver: driver name
 * @__register: register function for this driver type
 * @__unregister: unregister function for this driver type
 * @...: Additional arguments to be passed to __register and __unregister.
 *
 * Use this macro to construct bus specific macros for registering
 * drivers, and do not use it on its own.
 */
#define module_driver(__driver, __register, __unregister, ...) \
static int __init __driver##_init(void) \
{ \
        return __register(&(__driver) , ##__VA_ARGS__); \
} \
module_init(__driver##_init); \
static void __exit __driver##_exit(void) \
{ \
        __unregister(&(__driver) , ##__VA_ARGS__); \
} \
module_exit(__driver##_exit);


// imported from from usb.h
/**
  * module_usb_driver() - Helper macro for registering a USB driver
  * @__usb_driver: usb_driver struct
  *
  * Helper macro for USB drivers which do not do anything special in module
  * init/exit. This eliminates a lot of boilerplate. Each module may only
  * use this macro once, and calling it replaces module_init() and module_exit()
*/
#define module_usb_driver(__usb_driver) \
        module_driver(__usb_driver, usb_register, \
                      usb_deregister)


/* driver specific data */
struct qmi_wwan_state {
        struct usb_driver *subdriver;
        atomic_t pmcount;
        unsigned long unused;
        struct usb_interface *control;
        struct usb_interface *data;
};


/* using a counter to merge subdriver requests with our own into a combined state */
static int qmi_wwan_manage_power(struct usbnet *dev, int on)
{
        struct qmi_wwan_state *info = (void *)&dev->data;
        int rv = 0;

        dev_dbg(&dev->intf->dev, "%s() pmcount=%d, on=%d\n", __func__, atomic_read(&info->pmcount), on);

        if ((on && atomic_add_return(1, &info->pmcount) == 1) || (!on && atomic_dec_and_test(&info->pmcount))) {
                /* need autopm_get/put here to ensure the usbcore sees the new value */
                rv = usb_autopm_get_interface(dev->intf);
                if (rv < 0)
                        goto err;
                dev->intf->needs_remote_wakeup = on;
                usb_autopm_put_interface(dev->intf);
        }
err:
        return rv;
}

static int qmi_wwan_cdc_wdm_manage_power(struct usb_interface *intf, int on)
{
        struct usbnet *dev = usb_get_intfdata(intf);

        /* can be called while disconnecting */
        if (!dev)
                return 0;
        return qmi_wwan_manage_power(dev, on);
}

/* collect all three endpoints and register subdriver */
static int qmi_wwan_register_subdriver(struct usbnet *dev)
{
        int rv;
        struct usb_driver *subdriver = NULL;
        struct qmi_wwan_state *info = (void *)&dev->data;

        /* collect bulk endpoints */
        rv = usbnet_get_endpoints(dev, info->data);
        if (rv < 0)
                goto err;

        /* update status endpoint if separate control interface */
        if (info->control != info->data)
                dev->status = &info->control->cur_altsetting->endpoint[0];

        /* require interrupt endpoint for subdriver */
        if (!dev->status) {
                rv = -EINVAL;
                goto err;
        }

        /* for subdriver power management */
        atomic_set(&info->pmcount, 0);

        /* register subdriver */
        subdriver = usb_cdc_wdm_register(info->control, &dev->status->desc, 4096, &qmi_wwan_cdc_wdm_manage_power);
        if (IS_ERR(subdriver)) {
                dev_err(&info->control->dev, "subdriver registration failed\n");
                rv = PTR_ERR(subdriver);
                goto err;
        }

        /* prevent usbnet from using status endpoint */
        dev->status = NULL;

        /* save subdriver struct for suspend/resume wrappers */
        info->subdriver = subdriver;

err:
        return rv;
}

static int qmi_wwan_bind(struct usbnet *dev, struct usb_interface *intf)
{
        int status = -1;
        u8 *buf = intf->cur_altsetting->extra;
        int len = intf->cur_altsetting->extralen;
        struct usb_interface_descriptor *desc = &intf->cur_altsetting->desc;
        struct usb_cdc_union_desc *cdc_union = NULL;
        struct usb_cdc_ether_desc *cdc_ether = NULL;
        u32 found = 0;
        struct usb_driver *driver = driver_of(intf);
        struct qmi_wwan_state *info = (void *)&dev->data;

        BUILD_BUG_ON((sizeof(((struct usbnet *)0)->data) < sizeof(struct qmi_wwan_state)));

        /* control and data is shared? */
        if (intf->cur_altsetting->desc.bNumEndpoints == 3) {
                info->control = intf;
                info->data = intf;
                goto shared;
        }

shared:
        status = qmi_wwan_register_subdriver(dev);
        if (status < 0 && info->control != info->data) {
                usb_set_intfdata(info->data, NULL);
                usb_driver_release_interface(driver, info->data);
        }

err:
        return status;
}

static void qmi_wwan_unbind(struct usbnet *dev, struct usb_interface *intf)
{
        struct qmi_wwan_state *info = (void *)&dev->data;
        struct usb_driver *driver = driver_of(intf);
        struct usb_interface *other;

        if (info->subdriver && info->subdriver->disconnect)
                info->subdriver->disconnect(info->control);

        /* allow user to unbind using either control or data */
        if (intf == info->control)
                other = info->data;
        else
                other = info->control;

        /* only if not shared */
        if (other && intf != other) {
                usb_set_intfdata(other, NULL);
                usb_driver_release_interface(driver, other);
        }

        info->subdriver = NULL;
        info->data = NULL;
        info->control = NULL;
}

/* suspend/resume wrappers calling both usbnet and the cdc-wdm
 * subdriver if present.
 *
 * NOTE: cdc-wdm also supports pre/post_reset, but we cannot provide
 * wrappers for those without adding usbnet reset support first.
 */
static int qmi_wwan_suspend(struct usb_interface *intf, pm_message_t message)
{
        struct usbnet *dev = usb_get_intfdata(intf);
        struct qmi_wwan_state *info = (void *)&dev->data;
        int ret;

        ret = usbnet_suspend(intf, message);
        if (ret < 0)
                goto err;

        if (intf == info->control && info->subdriver && info->subdriver->suspend)
                ret = info->subdriver->suspend(intf, message);
        if (ret < 0)
                usbnet_resume(intf);
err:
        return ret;
}

static int qmi_wwan_resume(struct usb_interface *intf)
{
        struct usbnet *dev = usb_get_intfdata(intf);
        struct qmi_wwan_state *info = (void *)&dev->data;
        int ret = 0;
        bool callsub = (intf == info->control && info->subdriver && info->subdriver->resume);

        if (callsub)
                ret = info->subdriver->resume(intf);
        if (ret < 0)
                goto err;
        ret = usbnet_resume(intf);
        if (ret < 0 && callsub && info->subdriver->suspend)
                info->subdriver->suspend(intf, PMSG_SUSPEND);
err:
        return ret;
}

static const struct driver_info qmi_wwan_info = {
        .description    = "WWAN/QMI device",
        .flags          = FLAG_WWAN,
        .bind           = qmi_wwan_bind,
        .unbind         = qmi_wwan_unbind,
      //  .manage_power   = qmi_wwan_manage_power,
};

#define HUAWEI_VENDOR_ID        0x12D1

/* map QMI/wwan function by a fixed interface number */
#define QMI_FIXED_INTF(vend, prod, num) \
        USB_DEVICE_INTERFACE_NUMBER(vend, prod, num), \
        .driver_info = (unsigned long)&qmi_wwan_info

/* Gobi 1000 QMI/wwan interface number is 3 according to qcserial */
#define QMI_GOBI1K_DEVICE(vend, prod) \
        QMI_FIXED_INTF(vend, prod, 3)

/* Gobi 2000/3000 QMI/wwan interface number is 0 according to qcserial */
#define QMI_GOBI_DEVICE(vend, prod) \
        QMI_FIXED_INTF(vend, prod, 0)

static const struct usb_device_id products[] = {

        /* 3. Combined interface devices matching on interface number */
        {QMI_FIXED_INTF(0x0408, 0xea42, 4)},    /* Yota / Megafon M100-1 */
        {QMI_FIXED_INTF(0x12d1, 0x140c, 1)},    /* Huawei E173 */
        {QMI_FIXED_INTF(0x19d2, 0x0002, 1)},
        {QMI_FIXED_INTF(0x19d2, 0x0012, 1)},
        {QMI_FIXED_INTF(0x19d2, 0x0017, 3)},
        {QMI_FIXED_INTF(0x19d2, 0x0021, 4)},
        {QMI_FIXED_INTF(0x19d2, 0x0025, 1)},
        {QMI_FIXED_INTF(0x19d2, 0x0031, 4)},
        {QMI_FIXED_INTF(0x19d2, 0x0042, 4)},
        {QMI_FIXED_INTF(0x19d2, 0x0049, 5)},
        {QMI_FIXED_INTF(0x19d2, 0x0052, 4)},
        {QMI_FIXED_INTF(0x19d2, 0x0055, 1)},    /* ZTE (Vodafone) K3520-Z */
        {QMI_FIXED_INTF(0x19d2, 0x0058, 4)},
        {QMI_FIXED_INTF(0x19d2, 0x0063, 4)},    /* ZTE (Vodafone) K3565-Z */
        {QMI_FIXED_INTF(0x19d2, 0x0104, 4)},    /* ZTE (Vodafone) K4505-Z */
        {QMI_FIXED_INTF(0x19d2, 0x0113, 5)},
        {QMI_FIXED_INTF(0x19d2, 0x0118, 5)},
        {QMI_FIXED_INTF(0x19d2, 0x0121, 5)},
        {QMI_FIXED_INTF(0x19d2, 0x0123, 4)},
        {QMI_FIXED_INTF(0x19d2, 0x0124, 5)},
        {QMI_FIXED_INTF(0x19d2, 0x0125, 6)},
        {QMI_FIXED_INTF(0x19d2, 0x0126, 5)},
        {QMI_FIXED_INTF(0x19d2, 0x0130, 1)},
        {QMI_FIXED_INTF(0x19d2, 0x0133, 3)},
        {QMI_FIXED_INTF(0x19d2, 0x0141, 5)},
        {QMI_FIXED_INTF(0x19d2, 0x0157, 5)},    /* ZTE MF683 */
        {QMI_FIXED_INTF(0x19d2, 0x0158, 3)},
        {QMI_FIXED_INTF(0x19d2, 0x0167, 4)},    /* ZTE MF820D */
        {QMI_FIXED_INTF(0x19d2, 0x0168, 4)},
        {QMI_FIXED_INTF(0x19d2, 0x0176, 3)},
        {QMI_FIXED_INTF(0x19d2, 0x0178, 3)},
        {QMI_FIXED_INTF(0x19d2, 0x0191, 4)},    /* ZTE EuFi890 */
        {QMI_FIXED_INTF(0x19d2, 0x0199, 1)},    /* ZTE MF820S */
        {QMI_FIXED_INTF(0x19d2, 0x0200, 1)},
        {QMI_FIXED_INTF(0x19d2, 0x0257, 3)},    /* ZTE MF821 */
        {QMI_FIXED_INTF(0x19d2, 0x0265, 4)},    /* ONDA MT8205 4G LTE */
        {QMI_FIXED_INTF(0x19d2, 0x0284, 4)},    /* ZTE MF880 */
        {QMI_FIXED_INTF(0x19d2, 0x0326, 4)},    /* ZTE MF821D */
        {QMI_FIXED_INTF(0x19d2, 0x1008, 4)},    /* ZTE (Vodafone) K3570-Z */
        {QMI_FIXED_INTF(0x19d2, 0x1010, 4)},    /* ZTE (Vodafone) K3571-Z */
        {QMI_FIXED_INTF(0x19d2, 0x1012, 4)},
        {QMI_FIXED_INTF(0x19d2, 0x1018, 3)},    /* ZTE (Vodafone) K5006-Z */
        {QMI_FIXED_INTF(0x19d2, 0x1021, 2)},
        {QMI_FIXED_INTF(0x19d2, 0x1245, 4)},
        {QMI_FIXED_INTF(0x19d2, 0x1247, 4)},
        {QMI_FIXED_INTF(0x19d2, 0x1252, 4)},
        {QMI_FIXED_INTF(0x19d2, 0x1254, 4)},
        {QMI_FIXED_INTF(0x19d2, 0x1255, 3)},
        {QMI_FIXED_INTF(0x19d2, 0x1255, 4)},
        {QMI_FIXED_INTF(0x19d2, 0x1256, 4)},
        {QMI_FIXED_INTF(0x19d2, 0x1401, 2)},
        {QMI_FIXED_INTF(0x19d2, 0x1402, 2)},    /* ZTE MF60 */
        {QMI_FIXED_INTF(0x19d2, 0x1424, 2)},
        {QMI_FIXED_INTF(0x19d2, 0x1425, 2)},
        {QMI_FIXED_INTF(0x19d2, 0x1426, 2)},    /* ZTE MF91 */
        {QMI_FIXED_INTF(0x19d2, 0x2002, 4)},    /* ZTE (Vodafone) K3765-Z */
        {QMI_FIXED_INTF(0x0f3d, 0x68a2, 8)},    /* Sierra Wireless MC7700 */
        {QMI_FIXED_INTF(0x114f, 0x68a2, 8)},    /* Sierra Wireless MC7750 */
        {QMI_FIXED_INTF(0x1199, 0x68a2, 8)},    /* Sierra Wireless MC7710 in QMI mode */
        {QMI_FIXED_INTF(0x1199, 0x68a2, 19)},   /* Sierra Wireless MC7710 in QMI mode */
        {QMI_FIXED_INTF(0x1199, 0x901c, 8)},    /* Sierra Wireless EM7700 */
        {QMI_FIXED_INTF(0x1bbb, 0x011e, 4)},    /* Telekom Speedstick LTE II (Alcatel One Touch L100V LTE) */
        {QMI_FIXED_INTF(0x2357, 0x0201, 4)},    /* TP-LINK HSUPA Modem MA180 */
        {QMI_FIXED_INTF(0x1bc7, 0x1200, 5)},    /* Telit LE920 */

	{QMI_FIXED_INTF(0x1e2d, 0x0053, 4)},	/* Cinterion PH8 */
	{QMI_FIXED_INTF(0x1e2d, 0x0060, 4)},	/* Cinterion PLxx */

        /* 4. Gobi 1000 devices */
        {QMI_GOBI1K_DEVICE(0x05c6, 0x9212)},    /* Acer Gobi Modem Device */
        {QMI_GOBI1K_DEVICE(0x03f0, 0x1f1d)},    /* HP un2400 Gobi Modem Device */
        {QMI_GOBI1K_DEVICE(0x04da, 0x250d)},    /* Panasonic Gobi Modem device */
        {QMI_GOBI1K_DEVICE(0x413c, 0x8172)},    /* Dell Gobi Modem device */
        {QMI_GOBI1K_DEVICE(0x1410, 0xa001)},    /* Novatel Gobi Modem device */
        {QMI_GOBI1K_DEVICE(0x0b05, 0x1776)},    /* Asus Gobi Modem device */
        {QMI_GOBI1K_DEVICE(0x19d2, 0xfff3)},    /* ONDA Gobi Modem device */
        {QMI_GOBI1K_DEVICE(0x05c6, 0x9001)},    /* Generic Gobi Modem device */
        {QMI_GOBI1K_DEVICE(0x05c6, 0x9002)},    /* Generic Gobi Modem device */
        {QMI_GOBI1K_DEVICE(0x05c6, 0x9202)},    /* Generic Gobi Modem device */
        {QMI_GOBI1K_DEVICE(0x05c6, 0x9203)},    /* Generic Gobi Modem device */
        {QMI_GOBI1K_DEVICE(0x05c6, 0x9222)},    /* Generic Gobi Modem device */
        {QMI_GOBI1K_DEVICE(0x05c6, 0x9009)},    /* Generic Gobi Modem device */

        /* 5. Gobi 2000 and 3000 devices */
        {QMI_GOBI_DEVICE(0x413c, 0x8186)},      /* Dell Gobi 2000 Modem device (N0218, VU936) */
        {QMI_GOBI_DEVICE(0x413c, 0x8194)},      /* Dell Gobi 3000 Composite */
        {QMI_GOBI_DEVICE(0x05c6, 0x920b)},      /* Generic Gobi 2000 Modem device */
        {QMI_GOBI_DEVICE(0x05c6, 0x920d)},      /* Gobi 3000 Composite */
        {QMI_GOBI_DEVICE(0x05c6, 0x9225)},      /* Sony Gobi 2000 Modem device (N0279, VU730) */
        {QMI_GOBI_DEVICE(0x05c6, 0x9245)},      /* Samsung Gobi 2000 Modem device (VL176) */
        {QMI_GOBI_DEVICE(0x03f0, 0x251d)},      /* HP Gobi 2000 Modem device (VP412) */
        {QMI_GOBI_DEVICE(0x05c6, 0x9215)},      /* Acer Gobi 2000 Modem device (VP413) */
        {QMI_GOBI_DEVICE(0x05c6, 0x9265)},      /* Asus Gobi 2000 Modem device (VR305) */
        {QMI_GOBI_DEVICE(0x05c6, 0x9235)},      /* Top Global Gobi 2000 Modem device (VR306) */
        {QMI_GOBI_DEVICE(0x05c6, 0x9275)},      /* iRex Technologies Gobi 2000 Modem device (VR307) */
        {QMI_GOBI_DEVICE(0x1199, 0x68a5)},      /* Sierra Wireless Modem */
        {QMI_GOBI_DEVICE(0x1199, 0x68a9)},      /* Sierra Wireless Modem */
        {QMI_GOBI_DEVICE(0x1199, 0x9001)},      /* Sierra Wireless Gobi 2000 Modem device (VT773) */
        {QMI_GOBI_DEVICE(0x1199, 0x9002)},      /* Sierra Wireless Gobi 2000 Modem device (VT773) */
        {QMI_GOBI_DEVICE(0x1199, 0x9003)},      /* Sierra Wireless Gobi 2000 Modem device (VT773) */
        {QMI_GOBI_DEVICE(0x1199, 0x9004)},      /* Sierra Wireless Gobi 2000 Modem device (VT773) */
        {QMI_GOBI_DEVICE(0x1199, 0x9005)},      /* Sierra Wireless Gobi 2000 Modem device (VT773) */
        {QMI_GOBI_DEVICE(0x1199, 0x9006)},      /* Sierra Wireless Gobi 2000 Modem device (VT773) */
        {QMI_GOBI_DEVICE(0x1199, 0x9007)},      /* Sierra Wireless Gobi 2000 Modem device (VT773) */
        {QMI_GOBI_DEVICE(0x1199, 0x9008)},      /* Sierra Wireless Gobi 2000 Modem device (VT773) */
        {QMI_GOBI_DEVICE(0x1199, 0x9009)},      /* Sierra Wireless Gobi 2000 Modem device (VT773) */
        {QMI_GOBI_DEVICE(0x1199, 0x900a)},      /* Sierra Wireless Gobi 2000 Modem device (VT773) */
        {QMI_GOBI_DEVICE(0x1199, 0x9011)},      /* Sierra Wireless Gobi 2000 Modem device (MC8305) */
        {QMI_FIXED_INTF(0x1199, 0x9011, 5)},    /* alternate interface number!? */
        {QMI_GOBI_DEVICE(0x16d8, 0x8002)},      /* CMDTech Gobi 2000 Modem device (VU922) */
        {QMI_GOBI_DEVICE(0x05c6, 0x9205)},      /* Gobi 2000 Modem device */
        {QMI_GOBI_DEVICE(0x1199, 0x9013)},      /* Sierra Wireless Gobi 3000 Modem device (MC8355) */
        {QMI_GOBI_DEVICE(0x03f0, 0x371d)},      /* HP un2430 Mobile Broadband Module */
        {QMI_GOBI_DEVICE(0x1199, 0x9015)},      /* Sierra Wireless Gobi 3000 Modem device */
        {QMI_GOBI_DEVICE(0x1199, 0x9019)},      /* Sierra Wireless Gobi 3000 Modem device */
        {QMI_GOBI_DEVICE(0x1199, 0x901b)},      /* Sierra Wireless MC7770 */
        {QMI_GOBI_DEVICE(0x12d1, 0x14f1)},      /* Sony Gobi 3000 Composite */
        {QMI_GOBI_DEVICE(0x1410, 0xa021)},      /* Foxconn Gobi 3000 Modem device (Novatel E396) */

        { }                                     /* END */
};
MODULE_DEVICE_TABLE(usb, products);

static int qmi_wwan_probe(struct usb_interface *intf, const struct usb_device_id *prod)
{
        struct usb_device_id *id = (struct usb_device_id *)prod;

        /* Workaround to enable dynamic IDs.  This disables usbnet
         * blacklisting functionality.  Which, if required, can be
         * reimplemented here by using a magic "blacklist" value
         * instead of 0 in the static device id table
         */
        if (!id->driver_info) {
                dev_dbg(&intf->dev, "setting defaults for dynamic device id\n");
                id->driver_info = (unsigned long)&qmi_wwan_info;
        }

        return usbnet_probe(intf, id);
}

static struct usb_driver qmi_wwan_driver = {
        .name                 = "qmi_wwan",
        .id_table             = products,
        .probe                = qmi_wwan_probe,
        .disconnect           = usbnet_disconnect,
        .suspend              = qmi_wwan_suspend,
        .resume               = qmi_wwan_resume,
        .reset_resume         = qmi_wwan_resume,
        .supports_autosuspend = 1,
//        .disable_hub_initiated_lpm = 1,
};

module_usb_driver(qmi_wwan_driver);

MODULE_AUTHOR("Bjorn Mork <bjorn@mork.no>");
MODULE_DESCRIPTION("Qualcomm MSM Interface (QMI) WWAN driver");
MODULE_LICENSE("GPL");
