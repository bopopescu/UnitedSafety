/*
 * af_can.c - Protocol family CAN core module
 *            (used by different CAN protocol modules)
 *
 * Copyright (c) 2002-2007 Volkswagen Group Electronic Research
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Volkswagen nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * Alternatively, provided that this notice is retained in full, this
 * software may be distributed under the terms of the GNU General
 * Public License ("GPL") version 2, in which case the provisions of the
 * GPL apply INSTEAD OF those given above.
 *
 * The provided data structures and external interfaces from this code
 * are not restricted to be used by modules with a GPL compatible license.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * Send feedback to <socketcan-users@lists.berlios.de>
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kmod.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <asm/uaccess.h>
#include <linux/net.h>
#include <linux/netdevice.h>
#include <linux/socket.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <linux/skbuff.h>
#include <linux/can.h>
#include <linux/can/core.h>
#include <net/sock.h>

#include "af_can.h"
#include "compat.h"

#include <linux/can/version.h> /* for RCSID. Removed by mkpatch script */
RCSID("$Id: af_can.c 1055 2009-09-15 17:29:13Z hartkopp $");

static __initdata const char banner[] = KERN_INFO
	"can: controller area network core (" CAN_VERSION_STRING ")\n";

MODULE_DESCRIPTION("Controller Area Network PF_CAN core");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Urs Thuermann <urs.thuermann@volkswagen.de>, "
	      "Oliver Hartkopp <oliver.hartkopp@volkswagen.de>");

static int stats_timer __read_mostly = 1;
MODULE_PARM(stats_timer, "1i");

struct dev_rcv_lists *can_rx_dev_list = NULL;
static struct dev_rcv_lists can_rx_alldev_list;
rwlock_t can_rcvlists_lock = RW_LOCK_UNLOCKED;

static kmem_cache_t *rcv_cache;

/* table of registered CAN protocols */
static struct can_proto *proto_tab[CAN_NPROTO] __read_mostly;

struct timer_list can_stattimer;   /* timer for statistics update */
struct s_stats    can_stats;       /* packet statistics */
struct s_pstats   can_pstats;      /* receive list statistics */

/*
 * af_can socket functions
 */

static int can_ioctl(struct socket *sock, unsigned int cmd, unsigned long arg)
{
	struct sock *sk = sock->sk;

	switch (cmd) {

	case SIOCGSTAMP:
		if (sk->stamp.tv_sec == 0)
			return -ENOENT;

		return copy_to_user((void *)arg, &sk->stamp,
				    sizeof(sk->stamp)) ? -EFAULT : 0;
		break;
	default:
		return dev_ioctl(cmd, (void __user *)arg);
	}
}

static void can_sock_destruct(struct sock *sk)
{
	skb_queue_purge(&sk->sk_receive_queue);
}

static int can_create(struct socket *sock, int protocol)
{
	struct sock *sk;
	struct can_proto *cp;
	char module_name[sizeof("can-proto-000")];
	int err = 0;

	sock->state = SS_UNCONNECTED;

	if (protocol < 0 || protocol >= CAN_NPROTO)
		return -EINVAL;

	/* try to load protocol module, when CONFIG_KMOD is defined */
	if (!proto_tab[protocol]) {
		sprintf(module_name, "can-proto-%d", protocol);
		err = request_module(module_name);

		/*
		 * In case of error we only print a message but don't
		 * return the error code immediately.  Below we will
		 * return -EPROTONOSUPPORT
		 */
		if (err == -ENOSYS)
			printk(KERN_INFO "can: request_module(%s)"
			       " not implemented.\n", module_name);
		else if (err)
			printk(KERN_ERR "can: request_module(%s)"
			       " failed.\n", module_name);
	}

	cp = proto_tab[protocol];

	/* check for available protocol and correct usage */

	if (!cp)
		return -EPROTONOSUPPORT;

	if (cp->type != sock->type)
		return -EPROTONOSUPPORT;

	if (cp->capability >= 0 && !capable(cp->capability))
		return -EPERM;

	sock->ops = cp->ops;

	sk = sk_alloc(PF_CAN, GFP_KERNEL, 1);
	if (!sk)
		return -ENOMEM;

	sock_init_data(sock, sk);
	sk->sk_destruct = can_sock_destruct;

	if (cp->init)
		err = cp->init(sk);

	if (err) {
		/* release sk on errors */
		sock_orphan(sk);
		sock_put(sk);
	}

	return err;
}

/*
 * af_can tx path
 */

/**
 * can_send - transmit a CAN frame (optional with local loopback)
 * @skb: pointer to socket buffer with CAN frame in data section
 * @loop: loopback for listeners on local CAN sockets (recommended default!)
 *
 * Due to the loopback this routine must not be called from hardirq context.
 *
 * Return:
 *  0 on success
 *  -ENETDOWN when the selected interface is down
 *  -ENOBUFS on full driver queue (see net_xmit_errno())
 *  -ENOMEM when local loopback failed at calling skb_clone()
 *  -EPERM when trying to send on a non-CAN interface
 *  -EINVAL when the skb->data does not contain a valid CAN frame
 */
int can_send(struct sk_buff *skb, int loop)
{
	struct sk_buff *newskb = NULL;
	struct can_frame *cf = (struct can_frame *)skb->data;
	int err;

	if (skb->len != sizeof(struct can_frame) || cf->can_dlc > 8) {
		kfree_skb(skb);
		return -EINVAL;
	}

	if (skb->dev->type != ARPHRD_CAN) {
		kfree_skb(skb);
		return -EPERM;
	}

	if (!(skb->dev->flags & IFF_UP)) {
		kfree_skb(skb);
		return -ENETDOWN;
	}

	skb->protocol = htons(ETH_P_CAN);
	skb->nh.raw = skb->data;
	skb->h.raw  = skb->data;

	if (loop) {
		/* local loopback of sent CAN frames */

		/* indication for the CAN driver: do loopback */
		skb->pkt_type = PACKET_LOOPBACK;

		/*
		 * The reference to the originating sock may be required
		 * by the receiving socket to check whether the frame is
		 * its own. Example: can_raw sockopt CAN_RAW_RECV_OWN_MSGS
		 * Therefore we have to ensure that skb->sk remains the
		 * reference to the originating sock by restoring skb->sk
		 * after each skb_clone() or skb_orphan() usage.
		 */

#define IFF_ECHO IFF_LOOPBACK

		if (!(skb->dev->flags & IFF_ECHO)) {
			/*
			 * If the interface is not capable to do loopback
			 * itself, we do it here.
			 */
			newskb = skb_clone(skb, GFP_ATOMIC);
			if (!newskb) {
				kfree_skb(skb);
				return -ENOMEM;
			}

			newskb->sk = skb->sk;
			newskb->ip_summed = CHECKSUM_UNNECESSARY;
			newskb->pkt_type = PACKET_BROADCAST;
		}
	} else {
		/* indication for the CAN driver: no loopback required */
		skb->pkt_type = PACKET_HOST;
	}

	/* send to netdevice */
	err = dev_queue_xmit(skb);
	if (err > 0)
		err = net_xmit_errno(err);

	if (err) {
		if (newskb)
			kfree_skb(newskb);
		return err;
	}

	if (newskb)
		netif_rx_ni(newskb);

	/* update statistics */
	can_stats.tx_frames++;
	can_stats.tx_frames_delta++;

	return 0;
}
EXPORT_SYMBOL(can_send);

/*
 * af_can rx path
 */

static struct dev_rcv_lists *find_dev_rcv_lists(struct net_device *dev)
{
	struct dev_rcv_lists *d = NULL;

	/* find receive list for this device */

	for (d = can_rx_dev_list; d; d = d->next) {
		if (d->dev == dev)
			break;
	}

	return d;
}

/**
 * find_rcv_list - determine optimal filterlist inside device filter struct
 * @can_id: pointer to CAN identifier of a given can_filter
 * @mask: pointer to CAN mask of a given can_filter
 * @d: pointer to the device filter struct
 *
 * Description:
 *  Returns the optimal filterlist to reduce the filter handling in the
 *  receive path. This function is called by service functions that need
 *  to register or unregister a can_filter in the filter lists.
 *
 *  A filter matches in general, when
 *
 *          <received_can_id> & mask == can_id & mask
 *
 *  so every bit set in the mask (even CAN_EFF_FLAG, CAN_RTR_FLAG) describe
 *  relevant bits for the filter.
 *
 *  The filter can be inverted (CAN_INV_FILTER bit set in can_id) or it can
 *  filter for error frames (CAN_ERR_FLAG bit set in mask). For error frames
 *  there is a special filterlist and a special rx path filter handling.
 *
 * Return:
 *  Pointer to optimal filterlist for the given can_id/mask pair.
 *  Constistency checked mask.
 *  Reduced can_id to have a preprocessed filter compare value.
 */
static struct receiver **find_rcv_list(canid_t *can_id, canid_t *mask,
				       struct dev_rcv_lists *d)
{
	canid_t inv = *can_id & CAN_INV_FILTER; /* save flag before masking */

	/* filter for error frames in extra filterlist */
	if (*mask & CAN_ERR_FLAG) {
		/* clear CAN_ERR_FLAG in filter entry */
		*mask &= CAN_ERR_MASK;
		return &d->rx[RX_ERR];
	}

	/* with cleared CAN_ERR_FLAG we have a simple mask/value filterpair */

#define CAN_EFF_RTR_FLAGS (CAN_EFF_FLAG | CAN_RTR_FLAG)

	/* ensure valid values in can_mask for 'SFF only' frame filtering */
	if ((*mask & CAN_EFF_FLAG) && !(*can_id & CAN_EFF_FLAG))
		*mask &= (CAN_SFF_MASK | CAN_EFF_RTR_FLAGS);

	/* reduce condition testing at receive time */
	*can_id &= *mask;

	/* inverse can_id/can_mask filter */
	if (inv)
		return &d->rx[RX_INV];

	/* mask == 0 => no condition testing at receive time */
	if (!(*mask))
		return &d->rx[RX_ALL];

	/* extra filterlists for the subscription of a single non-RTR can_id */
	if (((*mask & CAN_EFF_RTR_FLAGS) == CAN_EFF_RTR_FLAGS)
	    && !(*can_id & CAN_RTR_FLAG)) {

		if (*can_id & CAN_EFF_FLAG) {
			if (*mask == (CAN_EFF_MASK | CAN_EFF_RTR_FLAGS)) {
				/* RFC: a future use-case for hash-tables? */
				return &d->rx[RX_EFF];
			}
		} else {
			if (*mask == (CAN_SFF_MASK | CAN_EFF_RTR_FLAGS))
				return &d->rx_sff[*can_id];
		}
	}

	/* default: filter via can_id/can_mask */
	return &d->rx[RX_FIL];
}

/**
 * can_rx_register - subscribe CAN frames from a specific interface
 * @dev: pointer to netdevice (NULL => subcribe from 'all' CAN devices list)
 * @can_id: CAN identifier (see description)
 * @mask: CAN mask (see description)
 * @func: callback function on filter match
 * @data: returned parameter for callback function
 * @ident: string for calling module indentification
 *
 * Description:
 *  Invokes the callback function with the received sk_buff and the given
 *  parameter 'data' on a matching receive filter. A filter matches, when
 *
 *          <received_can_id> & mask == can_id & mask
 *
 *  The filter can be inverted (CAN_INV_FILTER bit set in can_id) or it can
 *  filter for error frames (CAN_ERR_FLAG bit set in mask).
 *
 *  The provided pointer to the sk_buff is guaranteed to be valid as long as
 *  the callback function is running. The callback function must *not* free
 *  the given sk_buff while processing it's task. When the given sk_buff is
 *  needed after the end of the callback function it must be cloned inside
 *  the callback function with skb_clone().
 *
 * Return:
 *  0 on success
 *  -ENOMEM on missing cache mem to create subscription entry
 *  -ENODEV unknown device
 */
int can_rx_register(struct net_device *dev, canid_t can_id, canid_t mask,
		    void (*func)(struct sk_buff *, void *), void *data,
		    char *ident)
{
	struct receiver *r;
	struct receiver **rl;
	struct dev_rcv_lists *d;
	int err = 0;

	/* insert new receiver  (dev,canid,mask) -> (func,data) */

	r = kmem_cache_alloc(rcv_cache, GFP_KERNEL);
	if (!r)
		return -ENOMEM;

	write_lock_bh(&can_rcvlists_lock);

	d = find_dev_rcv_lists(dev);
	if (d) {
		rl = find_rcv_list(&can_id, &mask, d);

		r->can_id  = can_id;
		r->mask    = mask;
		r->matches = 0;
		r->func    = func;
		r->data    = data;
		r->ident   = ident;

		r->next = *rl;
		*rl = r;
		d->entries++;

		can_pstats.rcv_entries++;
		if (can_pstats.rcv_entries_max < can_pstats.rcv_entries)
			can_pstats.rcv_entries_max = can_pstats.rcv_entries;
	} else {
		kmem_cache_free(rcv_cache, r);
		err = -ENODEV;
	}

	write_unlock_bh(&can_rcvlists_lock);

	return err;
}
EXPORT_SYMBOL(can_rx_register);

/**
 * can_rx_unregister - unsubscribe CAN frames from a specific interface
 * @dev: pointer to netdevice (NULL => unsubcribe from 'all' CAN devices list)
 * @can_id: CAN identifier
 * @mask: CAN mask
 * @func: callback function on filter match
 * @data: returned parameter for callback function
 *
 * Description:
 *  Removes subscription entry depending on given (subscription) values.
 */
void can_rx_unregister(struct net_device *dev, canid_t can_id, canid_t mask,
		       void (*func)(struct sk_buff *, void *), void *data)
{
	struct receiver *r = NULL;
	struct receiver **rl;
	struct dev_rcv_lists *d;

	write_lock_bh(&can_rcvlists_lock);

	d = find_dev_rcv_lists(dev);
	if (!d) {
		printk(KERN_ERR "BUG: receive list not found for "
		       "dev %s, id %03X, mask %03X\n",
		       DNAME(dev), can_id, mask);
		goto out;
	}

	rl = find_rcv_list(&can_id, &mask, d);

	/*
	 * Search the receiver list for the item to delete.  This should
	 * exist, since no receiver may be unregistered that hasn't
	 * been registered before.
	 */

	for (; r = *rl; rl = &r->next) {
		if (r->can_id == can_id && r->mask == mask
		    && r->func == func && r->data == data)
			break;
	}

	/*
	 * Check for bugs in CAN protocol implementations:
	 * If no matching list item was found, r is NULL.
	 */

	if (!r) {
		printk(KERN_ERR "BUG: receive list entry not found for "
		       "dev %s, id %03X, mask %03X\n",
		       DNAME(dev), can_id, mask);
		goto out;
	}

	*rl = r->next;
	kmem_cache_free(rcv_cache, r);
	d->entries--;

	if (can_pstats.rcv_entries > 0)
		can_pstats.rcv_entries--;

	/* remove device structure requested by NETDEV_UNREGISTER */
	if (d->remove_on_zero_entries && !d->entries) {
		/* remove d from the list */
		*d->pprev = d->next;
		d->next->pprev = d->pprev;
		kfree(d);
	}

 out:
	write_unlock_bh(&can_rcvlists_lock);
}
EXPORT_SYMBOL(can_rx_unregister);

static inline void deliver(struct sk_buff *skb, struct receiver *r)
{
	r->func(skb, r->data);
	r->matches++;
}

static int can_rcv_filter(struct dev_rcv_lists *d, struct sk_buff *skb)
{
	struct receiver *r;
	int matches = 0;
	struct can_frame *cf = (struct can_frame *)skb->data;
	canid_t can_id = cf->can_id;

	if (d->entries == 0)
		return 0;

	if (can_id & CAN_ERR_FLAG) {
		/* check for error frame entries only */
		for (r = d->rx[RX_ERR]; r; r = r->next) {
			if (can_id & r->mask) {
				deliver(skb, r);
				matches++;
			}
		}
		return matches;
	}

	/* check for unfiltered entries */
	for (r = d->rx[RX_ALL]; r; r = r->next) {
		deliver(skb, r);
		matches++;
	}

	/* check for can_id/mask entries */
	for (r = d->rx[RX_FIL]; r; r = r->next) {
		if ((can_id & r->mask) == r->can_id) {
			deliver(skb, r);
			matches++;
		}
	}

	/* check for inverted can_id/mask entries */
	for (r = d->rx[RX_INV]; r; r = r->next) {
		if ((can_id & r->mask) != r->can_id) {
			deliver(skb, r);
			matches++;
		}
	}

	/* check filterlists for single non-RTR can_ids */
	if (can_id & CAN_RTR_FLAG)
		return matches;

	if (can_id & CAN_EFF_FLAG) {
		for (r = d->rx[RX_EFF]; r; r = r->next) {
			if (r->can_id == can_id) {
				deliver(skb, r);
				matches++;
			}
		}
	} else {
		can_id &= CAN_SFF_MASK;
		for (r = d->rx_sff[can_id]; r; r = r->next) {
			deliver(skb, r);
			matches++;
		}
	}

	return matches;
}

static int can_rcv(struct sk_buff *skb, struct net_device *dev,
		   struct packet_type *pt)
{
	struct dev_rcv_lists *d;
	struct can_frame *cf = (struct can_frame *)skb->data;
	int matches;

	if (dev->type != ARPHRD_CAN) {
		kfree_skb(skb);
		return 0;
	}

	BUG_ON(skb->len != sizeof(struct can_frame) || cf->can_dlc > 8);

	/* update statistics */
	can_stats.rx_frames++;
	can_stats.rx_frames_delta++;

	read_lock(&can_rcvlists_lock);

	/* deliver the packet to sockets listening on all devices */
	matches = can_rcv_filter(&can_rx_alldev_list, skb);

	/* find receive list for this device */
	d = find_dev_rcv_lists(dev);
	if (d)
		matches += can_rcv_filter(d, skb);

	read_unlock(&can_rcvlists_lock);

	/* free the skbuff allocated by the netdevice driver */
	kfree_skb(skb);

	if (matches > 0) {
		can_stats.matches++;
		can_stats.matches_delta++;
	}

	return 0;
}

/*
 * af_can protocol functions
 */

/**
 * can_proto_register - register CAN transport protocol
 * @cp: pointer to CAN protocol structure
 *
 * Return:
 *  0 on success
 *  -EINVAL invalid (out of range) protocol number
 *  -EBUSY  protocol already in use
 */
int can_proto_register(struct can_proto *cp)
{
	int proto = cp->protocol;

	if (proto < 0 || proto >= CAN_NPROTO) {
		printk(KERN_ERR "can: protocol number %d out of range\n",
		       proto);
		return -EINVAL;
	}
	if (proto_tab[proto]) {
		printk(KERN_ERR "can: protocol %d already registered\n",
		       proto);
		return -EBUSY;
	}
	proto_tab[proto] = cp;

	/* use generic ioctl function if the module doesn't bring its own */
	if (!cp->ops->ioctl)
		cp->ops->ioctl = can_ioctl;

	return 0;
}
EXPORT_SYMBOL(can_proto_register);

/**
 * can_proto_unregister - unregister CAN transport protocol
 * @cp: pointer to CAN protocol structure
 */
void can_proto_unregister(struct can_proto *cp)
{
	int proto = cp->protocol;

	if (!proto_tab[proto]) {
		printk(KERN_ERR "BUG: can: protocol %d is not registered\n",
		       proto);
	}
	proto_tab[proto] = NULL;
}
EXPORT_SYMBOL(can_proto_unregister);

/*
 * af_can notifier to create/remove CAN netdevice specific structs
 */
static int can_notifier(struct notifier_block *nb, unsigned long msg,
			void *data)
{
	struct net_device *dev = (struct net_device *)data;
	struct dev_rcv_lists *d;

	if (dev->type != ARPHRD_CAN)
		return NOTIFY_DONE;

	switch (msg) {

	case NETDEV_REGISTER:

		/*
		 * create new dev_rcv_lists for this device
		 *
		 * N.B. zeroing the struct is the correct initialization
		 * for the embedded structs.
		 */

		d = kzalloc(sizeof(*d), gfp_any());
		if (!d) {
			printk(KERN_ERR
			       "can: allocation of receive list failed\n");
			return NOTIFY_DONE;
		}
		d->dev = dev;

		/* insert d into the list */
		write_lock_bh(&can_rcvlists_lock);
		d->next        = can_rx_dev_list;
		d->pprev       = &can_rx_dev_list;
		can_rx_dev_list    = d;
		if (d->next)
			d->next->pprev = &d->next;
		write_unlock_bh(&can_rcvlists_lock);

		break;

	case NETDEV_UNREGISTER:
		write_lock_bh(&can_rcvlists_lock);

		d = find_dev_rcv_lists(dev);
		if (d) {
			if (d->entries) {
				d->remove_on_zero_entries = 1;
			} else {
				/* remove d from the list */
				*d->pprev = d->next;
				d->next->pprev = d->pprev;
				kfree(d);
			}
		} else
			printk(KERN_ERR "can: notifier: receive list not "
			       "found for dev %s\n", dev->name);

		write_unlock_bh(&can_rcvlists_lock);

		break;
	}

	return NOTIFY_DONE;
}

/*
 * af_can module init/exit functions
 */

static struct packet_type can_packet __read_mostly = {
	.type = __constant_htons(ETH_P_CAN),
	.dev  = NULL,
	.func = can_rcv,
};

static struct net_proto_family can_family_ops __read_mostly = {
	.family = PF_CAN,
	.create = can_create,
};

/* notifier block for netdevice event */
static struct notifier_block can_netdev_notifier __read_mostly = {
	.notifier_call = can_notifier,
};

static __init int can_init(void)
{
	struct net_device *dev;

	printk(banner);

	rcv_cache = kmem_cache_create("can_receiver", sizeof(struct receiver),
				      0, 0, NULL, NULL);
	if (!rcv_cache)
		return -ENOMEM;

	/*
	 * Insert can_rx_alldev_list for reception on all devices.
	 * This struct is zero initialized which is correct for the
	 * embedded receiver list head pointer, the dev pointer,
	 * and the entries counter.
	 */

	write_lock_bh(&can_rcvlists_lock);
	can_rx_alldev_list.pprev = &can_rx_dev_list;
	can_rx_dev_list          = &can_rx_alldev_list;
	write_unlock_bh(&can_rcvlists_lock);

	if (stats_timer) {
		/* the statistics are updated every second (timer triggered) */
		setup_timer(&can_stattimer, can_stat_update, 0);
		mod_timer(&can_stattimer, round_jiffies(jiffies + HZ));
	} else
		can_stattimer.function = NULL;

	can_init_proc();

	/* protocol register */
	sock_register(&can_family_ops);

	/* netdevice notifier register & init currently existing devices */
	read_lock_bh(&dev_base_lock);
	register_netdevice_notifier(&can_netdev_notifier);
	for (dev = dev_base; dev; dev = dev->next)
		can_netdev_notifier.notifier_call(&can_netdev_notifier,
						  NETDEV_REGISTER,
						  dev);
	read_unlock_bh(&dev_base_lock);

	dev_add_pack(&can_packet);

	return 0;
}

static __exit void can_exit(void)
{
	struct dev_rcv_lists *d;

	if (stats_timer)
		del_timer(&can_stattimer);

	can_remove_proc();

	/* protocol unregister */
	dev_remove_pack(&can_packet);
	unregister_netdevice_notifier(&can_netdev_notifier);
	sock_unregister(PF_CAN);

	/* remove can_rx_dev_list */
	write_lock_bh(&can_rcvlists_lock);
	for (d = can_rx_dev_list; d; d = d->next) {
		if (d != &can_rx_alldev_list)
			kfree(d);
	}
	can_rx_dev_list = NULL;
	write_unlock_bh(&can_rcvlists_lock);

	kmem_cache_destroy(rcv_cache);
}

module_init(can_init);
module_exit(can_exit);
