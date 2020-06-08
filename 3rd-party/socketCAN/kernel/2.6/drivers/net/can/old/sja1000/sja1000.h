/*
 * $Id: sja1000.h 505 2007-09-30 13:32:41Z hartkopp $
 *
 * sja1000.h -  Philips SJA1000 network device driver
 *
 * Copyright (c) 2003 Matthias Brukner, Trajet Gmbh, Rebenring 33,
 * 38106 Braunschweig, GERMANY
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

#ifndef SJA1000_H
#define SJA1000_H

#define SJA1000_IO_SIZE_BASIC   0x20
#define SJA1000_IO_SIZE_PELICAN 0x80 /* unused */

#define CHIP_NAME	"sja1000"

#define DRV_NAME_LEN	30 /* for "<chip_name>-<hal_name>" */

#define PROCBASE          "driver" /* /proc/ ... */

#define DEFAULT_HW_CLK	16000000
#define DEFAULT_SPEED	500 /* kBit/s */

#define CAN_NETDEV_NAME	"can%d"

#define TX_TIMEOUT      (50*HZ/1000) /* 50ms */ 
#define RESTART_MS      100  /* restart chip on persistent errors in 100ms */
#define MAX_BUS_ERRORS  200  /* prevent from flooding bus error interrupts */

/* SJA1000 registers - manual section 6.4 (Pelican Mode) */
#define REG_MOD		0x00
#define REG_CMR		0x01
#define REG_SR		0x02
#define REG_IR		0x03
#define REG_IER		0x04
#define REG_ALC		0x0B
#define REG_ECC		0x0C
#define REG_EWL		0x0D
#define REG_RXERR	0x0E
#define REG_TXERR	0x0F
#define REG_ACCC0	0x10
#define REG_ACCC1	0x11
#define REG_ACCC2	0x12
#define REG_ACCC3	0x13
#define REG_ACCM0	0x14
#define REG_ACCM1	0x15
#define REG_ACCM2	0x16
#define REG_ACCM3	0x17
#define REG_RMC		0x1D
#define REG_RBSA	0x1E

/* Common registers - manual section 6.5 */
#define REG_BTR0	0x06
#define REG_BTR1	0x07
#define REG_OCR		0x08
#define REG_CDR		0x1F

#define REG_FI		0x10
#define SFF_BUF		0x13
#define EFF_BUF		0x15

#define FI_FF		0x80
#define FI_RTR		0x40

#define REG_ID1		0x11
#define REG_ID2		0x12
#define REG_ID3		0x13
#define REG_ID4		0x14

#define CAN_RAM		0x20

/* mode register */
#define MOD_RM		0x01
#define MOD_LOM		0x02
#define MOD_STM		0x04
#define MOD_AFM		0x08
#define MOD_SM		0x10

/* commands */
#define CMD_SRR		0x10
#define CMD_CDO		0x08
#define CMD_RRB		0x04
#define CMD_AT		0x02
#define CMD_TR		0x01

/* interrupt sources */
#define IRQ_BEI		0x80
#define IRQ_ALI		0x40
#define IRQ_EPI		0x20
#define IRQ_WUI		0x10
#define IRQ_DOI		0x08
#define IRQ_EI		0x04
#define IRQ_TI		0x02
#define IRQ_RI		0x01
#define IRQ_ALL		0xFF
#define IRQ_OFF		0x00

/* status register content */
#define SR_BS		0x80
#define SR_ES		0x40
#define SR_TS		0x20
#define SR_RS		0x10
#define SR_TCS		0x08
#define SR_TBS		0x04
#define SR_DOS		0x02
#define SR_RBS		0x01

#define SR_CRIT (SR_BS|SR_ES)

/* ECC register */
#define ECC_DIR		0x20
#define ECC_SEG		0x1F
#define ECC_ERR		6

/* bus timing */
#define MAX_TSEG1	15
#define MAX_TSEG2	 7
#define SAMPLE_POINT	75
#define JUMPWIDTH     0x40

/* CAN private data structure */

struct can_priv {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)
	struct net_device_stats	stats;
#endif
	struct can_device_stats	can_stats;
	long			open_time;
	int			clock;
	int			hw_regs;
	int			restart_ms;
	int			debug;
	int			speed;
	int			btr;
	int			rx_probe;
	struct timer_list       timer;
	int			state;
	struct sk_buff		*echo_skb;
};

#define STATE_UNINITIALIZED	0
#define STATE_PROBE		1
#define STATE_ACTIVE		2
#define STATE_ERROR_ACTIVE	3
#define STATE_ERROR_PASSIVE	4
#define STATE_BUS_OFF		5
#define STATE_RESET_MODE	6

void can_proc_create(const char *drv_name);
void can_proc_remove(const char *drv_name);

#endif /* SJA1000_H */
