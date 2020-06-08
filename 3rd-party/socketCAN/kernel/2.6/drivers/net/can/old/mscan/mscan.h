/*
 * $Id: mscan.h 440 2007-07-25 10:45:40Z wolf $
 *
 * DESCRIPTION:
 *  Definitions of consts/structs to drive the Freescale MSCAN.
 *
 * AUTHOR:
 *  Andrey Volkov <avolkov@varma-el.com>
 *
 * COPYRIGHT:
 *  2004-2006, Varma Electronics Oy
 *
 * LICENCE:
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef __MSCAN_H__
#define __MSCAN_H__

#include <linux/autoconf.h>
#include <asm/types.h>

/* MSCAN control register 0 (CANCTL0) bits */
#define MSCAN_RXFRM		0x80
#define MSCAN_RXACT		0x40
#define MSCAN_CSWAI		0x20
#define MSCAN_SYNCH		0x10
#define MSCAN_TIME		0x08
#define MSCAN_WUPE		0x04
#define MSCAN_SLPRQ		0x02
#define MSCAN_INITRQ		0x01

/* MSCAN control register 1 (CANCTL1) bits */
#define MSCAN_CANE		0x80
#define MSCAN_CLKSRC		0x40
#define MSCAN_LOOPB		0x20
#define MSCAN_LISTEN		0x10
#define MSCAN_WUPM		0x04
#define MSCAN_SLPAK		0x02
#define MSCAN_INITAK		0x01

#ifdef	CONFIG_PPC_MPC52xx
#define MSCAN_CLKSRC_BUS	0
#define MSCAN_CLKSRC_XTAL	MSCAN_CLKSRC
#else
#define MSCAN_CLKSRC_BUS	MSCAN_CLKSRC
#define MSCAN_CLKSRC_XTAL	0
#endif

/* MSCAN receiver flag register (CANRFLG) bits */
#define MSCAN_WUPIF		0x80
#define MSCAN_CSCIF		0x40
#define MSCAN_RSTAT1		0x20
#define MSCAN_RSTAT0		0x10
#define MSCAN_TSTAT1		0x08
#define MSCAN_TSTAT0		0x04
#define MSCAN_OVRIF		0x02
#define MSCAN_RXF		0x01
#define MSCAN_ERR_IF 		(MSCAN_OVRIF | MSCAN_CSCIF)
#define MSCAN_RSTAT_MSK		(MSCAN_RSTAT1 | MSCAN_RSTAT0)
#define MSCAN_TSTAT_MSK		(MSCAN_TSTAT1 | MSCAN_TSTAT0)
#define MSCAN_STAT_MSK		(MSCAN_RSTAT_MSK | MSCAN_TSTAT_MSK)

#define MSCAN_STATE_BUS_OFF	(MSCAN_RSTAT1 | MSCAN_RSTAT0 | \
				 MSCAN_TSTAT1 | MSCAN_TSTAT0)
#define MSCAN_STATE_TX(canrflg)	(((canrflg)&MSCAN_TSTAT_MSK)>>2)
#define MSCAN_STATE_RX(canrflg)	(((canrflg)&MSCAN_RSTAT_MSK)>>4)
#define MSCAN_STATE_ACTIVE	0
#define MSCAN_STATE_WARNING	1
#define MSCAN_STATE_PASSIVE	2
#define MSCAN_STATE_BUSOFF	3

/* MSCAN receiver interrupt enable register (CANRIER) bits */
#define MSCAN_WUPIE		0x80
#define MSCAN_CSCIE		0x40
#define MSCAN_RSTATE1		0x20
#define MSCAN_RSTATE0		0x10
#define MSCAN_TSTATE1		0x08
#define MSCAN_TSTATE0		0x04
#define MSCAN_OVRIE		0x02
#define MSCAN_RXFIE		0x01

/* MSCAN transmitter flag register (CANTFLG) bits */
#define MSCAN_TXE2		0x04
#define MSCAN_TXE1		0x02
#define MSCAN_TXE0		0x01
#define MSCAN_TXE		(MSCAN_TXE2 | MSCAN_TXE1 | MSCAN_TXE0)

/* MSCAN transmitter interrupt enable register (CANTIER) bits */
#define MSCAN_TXIE2		0x04
#define MSCAN_TXIE1		0x02
#define MSCAN_TXIE0		0x01
#define MSCAN_TXIE		(MSCAN_TXIE2 | MSCAN_TXIE1 | MSCAN_TXIE0)

/* MSCAN transmitter message abort request (CANTARQ) bits */
#define MSCAN_ABTRQ2		0x04
#define MSCAN_ABTRQ1		0x02
#define MSCAN_ABTRQ0		0x01

/* MSCAN transmitter message abort ack (CANTAAK) bits */
#define MSCAN_ABTAK2		0x04
#define MSCAN_ABTAK1		0x02
#define MSCAN_ABTAK0		0x01

/* MSCAN transmit buffer selection (CANTBSEL) bits */
#define MSCAN_TX2		0x04
#define MSCAN_TX1		0x02
#define MSCAN_TX0		0x01

/* MSCAN ID acceptance control register (CANIDAC) bits */
#define MSCAN_IDAM1		0x20
#define MSCAN_IDAM0		0x10
#define MSCAN_IDHIT2		0x04
#define MSCAN_IDHIT1		0x02
#define MSCAN_IDHIT0		0x01

#define MSCAN_AF_32BIT		0x00
#define MSCAN_AF_16BIT		MSCAN_IDAM0
#define MSCAN_AF_8BIT		MSCAN_IDAM1
#define MSCAN_AF_CLOSED		(MSCAN_IDAM0|MSCAN_IDAM1)
#define MSCAN_AF_MASK		(~(MSCAN_IDAM0|MSCAN_IDAM1))

/* MSCAN Miscellaneous Register (CANMISC) bits */
#define MSCAN_BOHOLD		0x01

#ifdef	CONFIG_PPC_MPC52xx
#define _MSCAN_RESERVED_(n,num)	u8	_res##n[num]
#define _MSCAN_RESERVED_DSR_SIZE	2
#else
#define _MSCAN_RESERVED_(n,num)
#define _MSCAN_RESERVED_DSR_SIZE	0
#endif

/* Structure of the hardware registers */
struct mscan_regs {
	/* (see doco S12MSCANV3/D)		  MPC5200    MSCAN */
	u8 canctl0;				/* + 0x00     0x00 */
	u8 canctl1;				/* + 0x01     0x01 */
	_MSCAN_RESERVED_(1, 2);			/* + 0x02          */
	u8 canbtr0;				/* + 0x04     0x02 */
	u8 canbtr1;				/* + 0x05     0x03 */
	_MSCAN_RESERVED_(2, 2);			/* + 0x06          */
	u8 canrflg;				/* + 0x08     0x04 */
	u8 canrier;				/* + 0x09     0x05 */
	_MSCAN_RESERVED_(3, 2);			/* + 0x0a          */
	u8 cantflg;				/* + 0x0c     0x06 */
	u8 cantier;				/* + 0x0d     0x07 */
	_MSCAN_RESERVED_(4, 2);			/* + 0x0e          */
	u8 cantarq;				/* + 0x10     0x08 */
	u8 cantaak;				/* + 0x11     0x09 */
	_MSCAN_RESERVED_(5, 2);			/* + 0x12          */
	u8 cantbsel;				/* + 0x14     0x0a */
	u8 canidac;				/* + 0x15     0x0b */
	u8 reserved;				/* + 0x16     0x0c */
	_MSCAN_RESERVED_(6, 5);			/* + 0x17          */
#ifndef CONFIG_PPC_MPC52xx
	u8 canmisc;				/*            0x0d */
#endif
	u8 canrxerr;				/* + 0x1c     0x0e */
	u8 cantxerr;				/* + 0x1d     0x0f */
	_MSCAN_RESERVED_(7, 2);			/* + 0x1e          */
	u16 canidar1_0;				/* + 0x20     0x10 */
	_MSCAN_RESERVED_(8, 2);			/* + 0x22          */
	u16 canidar3_2;				/* + 0x24     0x12 */
	_MSCAN_RESERVED_(9, 2);			/* + 0x26          */
	u16 canidmr1_0;				/* + 0x28     0x14 */
	_MSCAN_RESERVED_(10, 2);		/* + 0x2a          */
	u16 canidmr3_2;				/* + 0x2c     0x16 */
	_MSCAN_RESERVED_(11, 2);		/* + 0x2e          */
	u16 canidar5_4;				/* + 0x30     0x18 */
	_MSCAN_RESERVED_(12, 2);		/* + 0x32          */
	u16 canidar7_6;				/* + 0x34     0x1a */
	_MSCAN_RESERVED_(13, 2);		/* + 0x36          */
	u16 canidmr5_4;				/* + 0x38     0x1c */
	_MSCAN_RESERVED_(14, 2);		/* + 0x3a          */
	u16 canidmr7_6;				/* + 0x3c     0x1e */
	_MSCAN_RESERVED_(15, 2);		/* + 0x3e          */
	struct {
		u16 idr1_0;			/* + 0x40     0x20 */
		 _MSCAN_RESERVED_(16, 2);	/* + 0x42          */
		u16 idr3_2;			/* + 0x44     0x22 */
		 _MSCAN_RESERVED_(17, 2);	/* + 0x46          */
		u16 dsr1_0;			/* + 0x48     0x24 */
		 _MSCAN_RESERVED_(18, 2);	/* + 0x4a          */
		u16 dsr3_2;			/* + 0x4c     0x26 */
		 _MSCAN_RESERVED_(19, 2);	/* + 0x4e          */
		u16 dsr5_4;			/* + 0x50     0x28 */
		 _MSCAN_RESERVED_(20, 2);	/* + 0x52          */
		u16 dsr7_6;			/* + 0x54     0x2a */
		 _MSCAN_RESERVED_(21, 2);	/* + 0x56          */
		u8 dlr;				/* + 0x58     0x2c */
		 u8:8;				/* + 0x59     0x2d */
		 _MSCAN_RESERVED_(22, 2);	/* + 0x5a          */
		u16 time;			/* + 0x5c     0x2e */
	} rx;
	 _MSCAN_RESERVED_(23, 2);		/* + 0x5e          */
	struct {
		u16 idr1_0;			/* + 0x60     0x30 */
		 _MSCAN_RESERVED_(24, 2);	/* + 0x62          */
		u16 idr3_2;			/* + 0x64     0x32 */
		 _MSCAN_RESERVED_(25, 2);	/* + 0x66          */
		u16 dsr1_0;			/* + 0x68     0x34 */
		 _MSCAN_RESERVED_(26, 2);	/* + 0x6a          */
		u16 dsr3_2;			/* + 0x6c     0x36 */
		 _MSCAN_RESERVED_(27, 2);	/* + 0x6e          */
		u16 dsr5_4;			/* + 0x70     0x38 */
		 _MSCAN_RESERVED_(28, 2);	/* + 0x72          */
		u16 dsr7_6;			/* + 0x74     0x3a */
		 _MSCAN_RESERVED_(29, 2);	/* + 0x76          */
		u8 dlr;				/* + 0x78     0x3c */
		u8 tbpr;			/* + 0x79     0x3d */
		 _MSCAN_RESERVED_(30, 2);	/* + 0x7a          */
		u16 time;			/* + 0x7c     0x3e */
	} tx;
	 _MSCAN_RESERVED_(31, 2);		/* + 0x7e          */
} __attribute__ ((packed));

#undef _MSCAN_RESERVED_
#define MSCAN_REGION 	sizeof(struct mscan)

#define MSCAN_WATCHDOG_TIMEOUT	((500*HZ)/1000)

struct mscan_platform_data {
	u8 clock_src;		/* MSCAN_CLKSRC_BUS or MSCAN_CLKSRC_XTAL */
	u32 clock_frq;		/* can ref. clock, in Hz */
};

struct net_device *alloc_mscandev(void);
/* @clock_src:
	1 = The MSCAN clock source is the onchip Bus Clock.
	0 = The MSCAN clock source is the chip Oscillator Clock.
*/
extern int register_mscandev(struct net_device *dev, int clock_src);
extern void unregister_mscandev(struct net_device *dev);

#endif				/* __MSCAN_H__ */
