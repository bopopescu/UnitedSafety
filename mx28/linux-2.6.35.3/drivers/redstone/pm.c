/*
 * Author: Amour Hassan <Amour.Hassan@gps1.com>
 * Date: March 12, 2014
 * Copyright 2010-2011 Siconix
 * Copyright 2010-2013 Absolutetrac
 * Copyright 2010-2014 AbsoluteGemini
 *
 * Description: Handles power management for the TRULink.
 */

#include <linux/phy.h>

#include "pm.h"

static int on_powerdown_ethernet_phy_bus_device(struct device* p_dev, void* p_data)
{

	if(p_dev->bus->pm)
	{
		printk(KERN_EMERG "\x1b[1;44;37mTRULink pm: Suspending Ethernet Phy(%s)...\x1b[0m\n", dev_name(p_dev));
		{
			/* ATS FIXME: There is no indication whether suspending worked or not.
			 *	Investigate further to know for sure when suspend occured.
			 *	For now, all that is known is that interface "eth0" must be "up"
			 *	(presumably because an Ethernet (Phy) data packet must be transmitted
			 *	containing the suspend request).
			 */
			p_dev->bus->pm->suspend(p_dev);
		}
	}

	return 0;
}

int trulink_pm(struct RedStoneParserContext* p_rpc)
{

	if(!strcmp("eth_powerdown", p_rpc->m_val))
	{
		/* For all TRULink Ethernet Phy devices on the MDIO bus. */
		bus_for_each_dev(&mdio_bus_type, NULL, NULL, on_powerdown_ethernet_phy_bus_device);
	}
	else
	{
		printk(KERN_ERR "%s,%d:%s: Invalid pm value=\"%s\"\n", __FILE__, __LINE__, __FUNCTION__, p_rpc->m_val);
	}

	return 0;
}

struct RedStoneParserCommand g_trulink_pm_struct = {0, "pm", trulink_pm};
