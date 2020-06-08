/*
 * slcan.c - serial line CAN interface driver (using tty line discipline)
 *
 * This file is derived from linux/drivers/net/slip.c
 *
 * Therefore it has the same (strange?) behaviour not to unregister the
 * netdevice when detaching the tty. Is there any better solution?
 *
 * Do not try to attach, detach and re-attach a tty for this reason ...
 *
 * slip.c Authors  : Laurence Culhane <loz@holmes.demon.co.uk>
 *                   Fred N. van Kempen <waltje@uwalt.nl.mugnet.org>
 * slcan.c Author  : Oliver Hartkopp <oliver.hartkopp@volkswagen.de>
 * backport to 2.4 : Lionel Saugrain <lionel.saugrain@sogeti.com>
 *
 * Copyright (c) 2007-2009 Volkswagen Group Electronic Research
 * Copyright (c) 2008-2009 Schneider Electric SA
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * The provided data structures and external interfaces from this code
 * are not restricted to be used by modules with a GPL compatible license.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307. You can also get it
 * at http://www.gnu.org/licenses/gpl.html
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

#include <linux/version.h>
#include <linux/module.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/termios.h>
#include <linux/bitops.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/in.h>
#include <linux/tty.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/rtnetlink.h>
#include <linux/if_arp.h>
#include <linux/if_ether.h>
#include <linux/if_slip.h>
#include <linux/delay.h>
#include <linux/init.h>

#include <linux/can.h>

#include <linux/can/version.h> /* for RCSID. Removed by mkpatch script */
RCSID("$Id: slcan.c 971 2009-05-05 08:49:03Z hartkopp $");

/*
 * Backported to Kernel 2.4 from kernel 2.6 slcan.c source based on release:
 * Id: slcan.c 856 2008-10-27 09:24:55Z hartkopp
 *
 */

static __initdata const char banner[] =
KERN_INFO "slcan: serial line CAN interface driver\n";

MODULE_DESCRIPTION("serial line CAN interface");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Oliver Hartkopp <oliver.hartkopp@volkswagen.de>, Lionel Saugrain <Lionel.Saugrain@Sogeti.com>");

#ifdef CONFIG_CAN_DEBUG_DEVICES
static int debug;
module_param(debug, int, S_IRUGO);
#define DBG(args...)       (debug & 1 ? \
			       (printk(KERN_DEBUG "slcan %s: ", __func__), \
				printk(args)) : 0)
#else
#define DBG(args...)
#endif

#ifndef N_SLCAN
#error Your kernel does not support tty line discipline N_SLCAN
#endif
/*
 * As there is currently no line discipline N_SLCAN in the mainstream kernel
 * you will have to modify two kernel includes & recompile the kernel.
 *
 * Add this in include/asm/termios.h after the definition of N_HCI:
 *        #define N_SLCAN         16
 *
 * Increment NR_LDICS in include/linux/tty.h from 16 to 17
 *
 * HACK for precompiled Kernels:
 *
 * In order to use the slcan driver without rebuilding the kernel, the slcan
 * driver must be compiled to use an existing line discipline.
 * The N_MOUSE line discipline is documented to be free for custom use and
 * using it *should* not cause any side effect.
 *
 * Then, before compiling the slcan driver, add a -DN_SLCAN=N_MOUSE  
 * compilation option in its Makefile. The slcan_attach tool must(!!) also be
 * rebuild to use the right value for N_SLCAN. This workaround will allow  
 * to use the slcan driver with an existing kernel.
 */

#define SLC_CHECK_TRANSMIT
#define SLCAN_MAGIC 0x53CA

int maxdev = 10; /* MAX number of SLCAN channels */

MODULE_PARM(maxdev, "i");
MODULE_PARM_DESC(maxdev, "Maximum number of slcan interfaces");

static struct tty_ldisc	slc_ldisc;

/* maximum rx buffer len: extended CAN frame with timestamp */
#define SLC_MTU (sizeof("T1111222281122334455667788EA5F\r")+1)

struct slcan {
	int			magic;

	/* Various fields. */
	struct tty_struct	*tty;		/* ptr to TTY structure	     */
	struct net_device	*dev;		/* easy for intr handling    */
	spinlock_t		lock;

	/* These are pointers to the malloc()ed frame buffers. */
	unsigned char		rbuff[SLC_MTU];	/* receiver buffer	     */
	int			rcount;         /* received chars counter    */
	unsigned char		xbuff[SLC_MTU];	/* transmitter buffer	     */
	unsigned char		*xhead;         /* pointer to next XMIT byte */
	int			xleft;          /* bytes left in XMIT queue  */

	/* SLCAN interface statistics. */
	struct net_device_stats stats;

	unsigned long		flags;		/* Flag values/ mode etc     */
#define SLF_INUSE		0		/* Channel in use            */
#define SLF_ERROR		1               /* Parity, etc. error        */

	unsigned char		leased;
	kdev_t			line;
	pid_t			pid;
};

typedef struct slcan_ctrl {
	struct slcan		ctrl;		/* SLCAN things		*/
	struct net_device	dev;		/* the device		*/
} slcan_ctrl_t;
static slcan_ctrl_t	**slcan_ctrls;

/************************************************************************
 *			SLCAN ENCAPSULATION FORMAT		  	 *
 ************************************************************************/

/*
 * A CAN frame has a can_id (11 bit standard frame format OR 29 bit extended
 * frame format) a data length code (can_dlc) which can be from 0 to 8
 * and up to <can_dlc> data bytes as payload.
 * Additionally a CAN frame may become a remote transmission frame if the
 * RTR-bit is set. This causes another ECU to send a CAN frame with the
 * given can_id.
 *
 * The SLCAN ASCII representation of these different frame types is:
 * <type> <id> <dlc> <data>*
 *
 * Extended frames (29 bit) are defined by capital characters in the type.
 * RTR frames are defined as 'r' types - normal frames have 't' type:
 * t => 11 bit data frame
 * r => 11 bit RTR frame
 * T => 29 bit data frame
 * R => 29 bit RTR frame
 *
 * The <id> is 3 (standard) or 8 (extended) bytes in ASCII Hex (base64).
 * The <dlc> is a one byte ASCII number ('0' - '8')
 * The <data> section has at much ASCII Hex bytes as defined by the <dlc>
 *
 * Examples:
 *
 * t1230 : can_id 0x123, can_dlc 0, no data
 * t4563112233 : can_id 0x456, can_dlc 3, data 0x11 0x22 0x33
 * T12ABCDEF2AA55 : extended can_id 0x12ABCDEF, can_dlc 2, data 0xAA 0x55
 * r1230 : can_id 0x123, can_dlc 0, no data, remote transmission request
 *
 */

/************************************************************************
 *			STANDARD SLCAN DECAPSULATION		  	 *
 ************************************************************************/

static int asc2nibble(char c)
{

	if ((c >= '0') && (c <= '9'))
		return c - '0';

	if ((c >= 'A') && (c <= 'F'))
		return c - 'A' + 10;

	if ((c >= 'a') && (c <= 'f'))
		return c - 'a' + 10;

	return 16; /* error */
}

/* Send one completely decapsulated can_frame to the network layer */
static void slc_bump(struct slcan *sl)
{
	struct net_device_stats *stats = sl->dev->get_stats(sl->dev);
	struct sk_buff *skb;
	struct can_frame cf;
	int i, dlc_pos, tmp;
	char cmd = sl->rbuff[0];

	if ((cmd != 't') && (cmd != 'T') && (cmd != 'r') && (cmd != 'R'))
		return;

	if (cmd & 0x20) /* tiny chars 'r' 't' => standard frame format */
		dlc_pos = 4; /* dlc position tiiid */
	else
		dlc_pos = 9; /* dlc position Tiiiiiiiid */

	if (!((sl->rbuff[dlc_pos] >= '0') && (sl->rbuff[dlc_pos] < '9')))
		return;

	cf.can_dlc = sl->rbuff[dlc_pos] & 0x0F; /* get can_dlc */

	sl->rbuff[dlc_pos] = 0; /* terminate can_id string */

	cf.can_id = simple_strtoul(sl->rbuff+1, NULL, 16);

	if (!(cmd & 0x20)) /* NO tiny chars => extended frame format */
		cf.can_id |= CAN_EFF_FLAG;

	if ((cmd | 0x20) == 'r') /* RTR frame */
		cf.can_id |= CAN_RTR_FLAG;

	*(u64 *) (&cf.data) = 0; /* clear payload */

	for (i = 0, dlc_pos++; i < cf.can_dlc; i++) {

		tmp = asc2nibble(sl->rbuff[dlc_pos++]);
		if (tmp > 0x0F)
			return;
		cf.data[i] = (tmp << 4);
		tmp = asc2nibble(sl->rbuff[dlc_pos++]);
		if (tmp > 0x0F)
			return;
		cf.data[i] |= tmp;
	}


	skb = dev_alloc_skb(sizeof(struct can_frame));
	if (!skb)
		return;

	skb->dev = sl->dev;
	skb->protocol = htons(ETH_P_CAN);
	skb->pkt_type = PACKET_BROADCAST;
	skb->ip_summed = CHECKSUM_UNNECESSARY;
	memcpy(skb_put(skb, sizeof(struct can_frame)),
	       &cf, sizeof(struct can_frame));
	netif_rx(skb);

	sl->dev->last_rx = jiffies;
	stats->rx_packets++;
	stats->rx_bytes += cf.can_dlc;
}

/* parse tty input stream */
static void slcan_unesc(struct slcan *sl, unsigned char s)
{
	struct net_device_stats *stats = sl->dev->get_stats(sl->dev);

	if ((s == '\r') || (s == '\a')) { /* CR or BEL ends the pdu */
		if (!test_and_clear_bit(SLF_ERROR, &sl->flags) &&
		    (sl->rcount > 4))  {
			slc_bump(sl);
		}
		sl->rcount = 0;
	} else {
		if (!test_bit(SLF_ERROR, &sl->flags))  {
			if (sl->rcount < SLC_MTU)  {
				sl->rbuff[sl->rcount++] = s;
				return;
			} else {
				stats->rx_over_errors++;
				set_bit(SLF_ERROR, &sl->flags);
			}
		}
	}
}

/************************************************************************
 *			STANDARD SLCAN ENCAPSULATION		  	 *
 ************************************************************************/

/* Encapsulate one can_frame and stuff into a TTY queue. */
static void slc_encaps(struct slcan *sl, struct can_frame *cf)
{
	struct net_device_stats *stats = sl->dev->get_stats(sl->dev);
	int actual, idx, i;
	char cmd;

	if (cf->can_id & CAN_RTR_FLAG)
		cmd = 'R'; /* becomes 'r' in standard frame format */
	else
		cmd = 'T'; /* becomes 't' in standard frame format */

	if (cf->can_id & CAN_EFF_FLAG)
		sprintf(sl->xbuff, "%c%08X%d", cmd,
			cf->can_id & CAN_EFF_MASK, cf->can_dlc);
	else
		sprintf(sl->xbuff, "%c%03X%d", cmd | 0x20,
			cf->can_id & CAN_SFF_MASK, cf->can_dlc);

	idx = strlen(sl->xbuff);

	for (i = 0; i < cf->can_dlc; i++)
		sprintf(&sl->xbuff[idx + 2*i], "%02X", cf->data[i]);

	DBG("ASCII frame = '%s'\n", sl->xbuff);

	strcat(sl->xbuff, "\r"); /* add terminating character */

	/* Order of next two lines is *very* important.
	 * When we are sending a little amount of data,
	 * the transfer may be completed inside driver.write()
	 * routine, because it's running with interrupts enabled.
	 * In this case we *never* got WRITE_WAKEUP event,
	 * if we did not request it before write operation.
	 *       14 Oct 1994  Dmitry Gorodchanin.
	 */
	sl->tty->flags |= (1 << TTY_DO_WRITE_WAKEUP);

	actual = sl->tty->driver.write(sl->tty, 0, sl->xbuff,
				       strlen(sl->xbuff));

#ifdef SLC_CHECK_TRANSMIT
	sl->dev->trans_start = jiffies;
#endif
	sl->xleft = strlen(sl->xbuff) - actual;
	sl->xhead = sl->xbuff + actual;
	stats->tx_bytes += cf->can_dlc;
}

/*
 * Called by the driver when there's room for more data.  If we have
 * more packets to send, we send them here.
 */
static void slcan_write_wakeup(struct tty_struct *tty)
{
	int actual;
	struct slcan *sl = (struct slcan *) tty->disc_data;
	struct net_device_stats *stats = sl->dev->get_stats(sl->dev);

	/* First make sure we're connected. */
	if (!sl || sl->magic != SLCAN_MAGIC || !netif_running(sl->dev))
		return;

	if (sl->xleft <= 0)  {
		/* Now serial buffer is almost free & we can start
		 * transmission of another packet */
		stats->tx_packets++;
		tty->flags &= ~(1 << TTY_DO_WRITE_WAKEUP);
		netif_wake_queue(sl->dev);
		return;
	}

	actual = tty->driver.write(tty, 0, sl->xhead, sl->xleft);

	sl->xleft -= actual;
	sl->xhead += actual;
}

static void slc_tx_timeout(struct net_device *dev)
{
	struct slcan *sl = (struct slcan*)(dev->priv);

	spin_lock(&sl->lock);

	if (netif_queue_stopped(dev)) {
		if (!netif_running(dev))
			goto out;

		/* May be we must check transmitter timeout here ?
		 *      14 Oct 1994 Dmitry Gorodchanin.
		 */
#ifdef SLC_CHECK_TRANSMIT
		if (time_before(jiffies, dev->trans_start + 20 * HZ))  {
			/* 20 sec timeout not reached */
			goto out;
		}
		printk(KERN_WARNING "%s: transmit timed out, %s?\n",
		       dev->name,
		       (sl->tty->driver.chars_in_buffer(sl->tty) || sl->xleft)

		       ? "bad line quality" : "driver error");
		sl->xleft = 0;
		sl->tty->flags &= ~(1 << TTY_DO_WRITE_WAKEUP);
		netif_wake_queue(sl->dev);
#endif
	}
 out:
	spin_unlock(&sl->lock);
}


/******************************************
 *   Routines looking at netdevice side.
 ******************************************/

/* Send a can_frame to a TTY queue. */
static int slc_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct slcan *sl = (struct slcan*)(dev->priv);

	if (skb->len != sizeof(struct can_frame))
		goto out;

	spin_lock(&sl->lock);
	if (!netif_running(dev))  {
		spin_unlock(&sl->lock);
		printk(KERN_WARNING "%s: xmit: iface is down\n", dev->name);
		goto out;
	}

	if (sl->tty == NULL) {
		spin_unlock(&sl->lock);
		goto out;
	}

	netif_stop_queue(sl->dev);
	slc_encaps(sl, (struct can_frame *) skb->data); /* encaps & send */
	spin_unlock(&sl->lock);

 out:
	kfree_skb(skb);
	return 0;
}


/* Netdevice UP -> DOWN routine */
static int slc_close(struct net_device *dev)
{
	struct slcan *sl = (struct slcan*)(dev->priv);

	spin_lock_bh(&sl->lock);
	if (sl->tty) {
		/* TTY discipline is running. */
		sl->tty->flags &= ~(1 << TTY_DO_WRITE_WAKEUP);
	}
	netif_stop_queue(dev);
	sl->rcount   = 0;
	sl->xleft    = 0;
	spin_unlock_bh(&sl->lock);

	return 0;
}

/* Netdevice DOWN -> UP routine */
static int slc_open(struct net_device *dev)
{
	struct slcan *sl = (struct slcan*)(dev->priv);

	if (sl->tty == NULL)
		return -ENODEV;

	sl->flags &= (1 << SLF_INUSE);
	netif_start_queue(dev);
	return 0;
}

/* Netdevice get statistics request */
static struct net_device_stats *slc_get_stats(struct net_device *dev)
{
	struct slcan *sl = (struct slcan*)(dev->priv);

	return (&sl->stats);
}


/* Netdevice register callback */
static int slc_init(struct net_device *dev)
{
	/*
	 *	Finish setting up the DEVICE info.
	 */

	dev->mtu		= sizeof(struct can_frame);
	dev->hard_start_xmit	= slc_xmit;
#ifdef SL_CHECK_TRANSMIT
	dev->tx_timeout		= slc_tx_timeout;
	dev->watchdog_timeo	= 20*HZ;
#endif
	dev->open		= slc_open;
	dev->stop		= slc_close;
	dev->get_stats	        = slc_get_stats;

	dev->hard_header_len	= 0;
	dev->addr_len		= 0;
	dev->type		= ARPHRD_CAN;
	dev->tx_queue_len	= 10;

	SET_MODULE_OWNER(dev);

	/* New-style flags. */
	dev->flags		= IFF_NOARP;
	dev->features           = NETIF_F_NO_CSUM;

	return 0;
}

/******************************************
 * Routines looking at TTY side.
 ******************************************/

static int slcan_receive_room(struct tty_struct *tty)
{
	return 65536;  /* We can handle an infinite amount of data. :-) */
}

/*
 * Handle the 'receiver data ready' interrupt.
 * This function is called by the 'tty_io' module in the kernel when
 * a block of SLCAN data has been received, which can now be decapsulated
 * and sent on to some IP layer for further processing. This will not
 * be re-entered while running but other ldisc functions may be called
 * in parallel
 */

static void slcan_receive_buf(struct tty_struct *tty,
			      const unsigned char *cp, char *fp, int count)
{
	struct slcan *sl = (struct slcan *) tty->disc_data;
	struct net_device_stats *stats = sl->dev->get_stats(sl->dev);

	if (!sl || sl->magic != SLCAN_MAGIC ||
	    !netif_running(sl->dev))
		return;

	/* Read the characters out of the buffer */
	while (count--) {
		if (fp && *fp++) {
			if (!test_and_set_bit(SLF_ERROR, &sl->flags))
				stats->rx_errors++;
			cp++;
			continue;
		}
		slcan_unesc(sl, *cp++);
	}
}

/************************************
 *  slcan_open helper routines.
 ************************************/

/* Collect hanged up channels */

static void slc_sync(void)
{
	int i;

	for (i = 0; i < maxdev; i++) {
		slcan_ctrl_t *slp = slcan_ctrls[i];
		if (slp == NULL)
			break;

		if (slp->ctrl.tty || slp->ctrl.leased)
			continue;
		if (slp->dev.flags&IFF_UP)
			dev_close(&slp->dev);
	}
}


/* Find a free SLCAN channel, and link in this `tty' line. */
static struct slcan *slc_alloc(kdev_t line)
{
	int i;
	int sel = -1;
	int score = -1;
	slcan_ctrl_t *slp = NULL;
	struct slcan       *sl;

	if (slcan_ctrls == NULL)
		return NULL;	/* Master array missing ! */

	for (i = 0; i < maxdev; i++) {
		slp = slcan_ctrls[i];
		if (slp == NULL)
			break;

		if (slp->ctrl.leased) {
			if (slp->ctrl.line != line)
				continue;
			if (slp->ctrl.tty)
				return NULL;

			/* Clear ESCAPE & ERROR flags */
			slp->ctrl.flags &= (1 << SLF_INUSE);
			return &slp->ctrl;
		}

		if (slp->ctrl.tty)
			continue;

		if (current->pid == slp->ctrl.pid) {
			if (slp->ctrl.line == line && score < 3) {
				sel = i;
				score = 3;
				continue;
			}
			if (score < 2) {
				sel = i;
				score = 2;
			}
			continue;
		}
		if (slp->ctrl.line == line && score < 1) {
			sel = i;
			score = 1;
			continue;
		}

		if (score < 0) {
			sel = i;
			score = 0;
		}
	}


	if (sel >= 0) {
		i = sel;
		slp = slcan_ctrls[i];
		if (score > 1) {
			slp->ctrl.flags &= (1 << SLF_INUSE);
			return &slp->ctrl;
		}
	}

	/* Sorry, too many, all slots in use */
	if (i >= maxdev)
		return NULL;

	if (slp) {
		if (test_bit(SLF_INUSE, &slp->ctrl.flags)) {
			unregister_netdevice(&slp->dev);
		}
	} else if ((slp = (slcan_ctrl_t *)kmalloc(sizeof(slcan_ctrl_t),
						  GFP_KERNEL)) == NULL)
		return NULL;

	memset(slp, 0, sizeof(slcan_ctrl_t));

	sl = &slp->ctrl;

	/* Initialize channel control data */
	sl->magic       = SLCAN_MAGIC;
	sl->dev	      	= &slp->dev;

	spin_lock_init(&sl->lock);

	sprintf(slp->dev.name, "slc%d", i);
	slp->dev.base_addr    = i;
	slp->dev.priv         = (void*)sl;
	slp->dev.init         = slc_init;

	slcan_ctrls[i] 	      = slp;
	return &slp->ctrl;
}

/*
 * Open the high-level part of the SLCAN channel.
 * This function is called by the TTY module when the
 * SLCAN line discipline is called for.  Because we are
 * sure the tty line exists, we only have to link it to
 * a free SLCAN channel...
 *
 * Called in process context serialized from other ldisc calls.
 */

static int slcan_open(struct tty_struct *tty)
{
	struct slcan *sl;
	int err;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	/* RTnetlink lock is misused here to serialize concurrent
	   opens of slcan channels. There are better ways, but it is
	   the simplest one.
	*/
	rtnl_lock();

	/* Collect hanged up channels. */
	slc_sync();

	sl = (struct slcan *) tty->disc_data;

	err = -EEXIST;
	/* First make sure we're not already connected. */
	if (sl && sl->magic == SLCAN_MAGIC)
		goto err_exit;

	/* OK.  Find a free SLCAN channel to use. */
	err = -ENFILE;

	if ((sl = slc_alloc(tty->device)) == NULL)
		goto err_exit;

	sl->tty = tty;
	tty->disc_data = sl;
	sl->line = tty->device;

	sl->pid = current->pid;

	/* FIXME: already done before we were called - seems this can go */
	if (tty->driver.flush_buffer)
		tty->driver.flush_buffer(tty);

	if (!test_bit(SLF_INUSE, &sl->flags)) {
		/* Perform the low-level SLCAN initialization. */
		sl->rcount   = 0;
		sl->xleft    = 0;

		set_bit(SLF_INUSE, &sl->flags);

		err = register_netdevice(sl->dev);
		if (err)
			goto err_free_chan;
	}

	/* Done.  We have linked the TTY line to a channel. */
	rtnl_unlock();

	return sl->dev->base_addr;

 err_free_chan:
	sl->tty = NULL;
	tty->disc_data = NULL;
	clear_bit(SLF_INUSE, &sl->flags);

 err_exit:
	rtnl_unlock();

	/* Count references from TTY module */
	return err;
}

/*

FIXME: 1,2 are fixed 3 was never true anyway.

Let me to blame a bit.
1. TTY module calls this funstion on soft interrupt.
2. TTY module calls this function WITH MASKED INTERRUPTS!
3. TTY module does not notify us about line discipline
shutdown,

Seems, now it is clean. The solution is to consider netdevice and
line discipline sides as two independent threads.

By-product (not desired): slc? does not feel hangups and remains open.
It is supposed, that user level program (dip, diald, slattach...)
will catch SIGHUP and make the rest of work.

I see no way to make more with current tty code. --ANK
*/

/*
 * Close down a SLCAN channel.
 * This means flushing out any pending queues, and then returning. This
 * call is serialized against other ldisc functions.
 */
static void slcan_close(struct tty_struct *tty)
{
	struct slcan *sl = (struct slcan *) tty->disc_data;

	/* First make sure we're connected. */
	if (!sl || sl->magic != SLCAN_MAGIC || sl->tty != tty)
		return;

	tty->disc_data = NULL;
	sl->tty = NULL;
	if (!sl->leased)
		sl->line = 0;

	/* Count references from TTY module */
}

/* Perform I/O control on an active SLCAN channel. */
static int slcan_ioctl(struct tty_struct *tty, void *file, int cmd, void *arg)
{
	struct slcan *sl = (struct slcan *) tty->disc_data;
	unsigned int tmp;

	/* First make sure we're connected. */
	if (!sl || sl->magic != SLCAN_MAGIC)
		return -EINVAL;

	switch (cmd) {
	case SIOCGIFNAME:
		tmp = strlen(sl->dev->name) + 1;
  		if (copy_to_user(arg, sl->dev->name, tmp))
			return -EFAULT;
		return 0;

	case SIOCSIFHWADDR:
		return -EINVAL;

		/* Allow stty to read, but not set, the serial port */
	case TCGETS:
	case TCGETA:
		return n_tty_ioctl(tty, (struct file *) file, cmd,
				   (unsigned long) arg);

	default:
		return -ENOIOCTLCMD;
	}
}

/************************************
 * general slcan module init/exit
 ************************************/

static int __init slcan_init_ctrl_dev(void)
{
	int status;

	if (maxdev < 4)
		maxdev = 4; /* Sanity */

	printk(banner);
	printk(KERN_INFO "slcan: %d dynamic interface channels.\n", maxdev);

	slcan_ctrls = (slcan_ctrl_t **) kmalloc(sizeof(void*)*maxdev,
						GFP_KERNEL);
	if (slcan_ctrls == NULL) {
		printk(KERN_ERR "slcan: can't allocate slcan device array!\n");
		return -ENOMEM;
	}

	/* Clear the pointer array, we allocate devices when we need them */
	memset(slcan_ctrls, 0, sizeof(void*)*maxdev);

	/* Fill in our line protocol discipline, and register it */
	memset(&slc_ldisc, 0, sizeof(slc_ldisc));
	slc_ldisc.magic  = TTY_LDISC_MAGIC;
	slc_ldisc.name   = "slcan";
	slc_ldisc.flags  = 0;
	slc_ldisc.open   = slcan_open;
	slc_ldisc.close  = slcan_close;
	slc_ldisc.read   = NULL;
	slc_ldisc.write  = NULL;
	slc_ldisc.ioctl  = (int (*)(struct tty_struct *, struct file *,
				    unsigned int, unsigned long)) slcan_ioctl;
	slc_ldisc.poll   = NULL;
	slc_ldisc.receive_buf = slcan_receive_buf;
	slc_ldisc.receive_room = slcan_receive_room;
	slc_ldisc.write_wakeup = slcan_write_wakeup;

	status = tty_register_ldisc(N_SLCAN, &slc_ldisc);
	if (status != 0)  {
		printk(KERN_ERR "slcan: can't register line discipline\n");

	}
	return status;
}

#ifdef MODULE

int
init_module(void)
{
	return slcan_init_ctrl_dev();
}

void
cleanup_module(void)
{
	int i;

	if (slcan_ctrls != NULL) {
		unsigned long timeout = jiffies + HZ;
		int busy = 0;

		/* First of all: check for active disciplines and hangup them.
		 */
		do {
			if (busy)
				yield();

			busy = 0;
			local_bh_disable();
			for (i = 0; i < maxdev; i++) {
				struct slcan_ctrl *slc = slcan_ctrls[i];
				if (!slc)
					continue;
				spin_lock(&slc->ctrl.lock);
				if (slc->ctrl.tty) {
					busy++;
					tty_hangup(slc->ctrl.tty);
				}
				spin_unlock(&slc->ctrl.lock);
			}
			local_bh_enable();
		} while (busy && time_before(jiffies, timeout));

		busy = 0;
		for (i = 0; i < maxdev; i++) {
			struct slcan_ctrl *slc = slcan_ctrls[i];
			if (slc) {
				unregister_netdev(&slc->dev);
				if (slc->ctrl.tty) {
					printk(
						"%s: tty discipline is still running\n",
						slc->dev.name);
					/* Pin module forever */
					MOD_INC_USE_COUNT;
					busy++;
					continue;
				}
				kfree(slc);
				slcan_ctrls[i] = NULL;
			}
		}
		if (!busy) {
			kfree(slcan_ctrls);
			slcan_ctrls = NULL;
		}
	}
	if ((i = tty_register_ldisc(N_SLCAN, NULL)))
	{
		printk("slcan: can't unregister line discipline (err = %d)\n",
		       i);
	}
}
#endif /* MODULE */
