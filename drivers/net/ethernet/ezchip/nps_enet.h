/*******************************************************************************

  EZchip Network Linux driver
  Copyright(c) 2012 EZchip Technologies.

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

*******************************************************************************/

#ifndef _NPS_ENET_H
#define _NPS_ENET_H
#include <linux/etherdevice.h>

/* driver status definitions  */
#define	EZ_NET_DEV_OFF			0
#define	EZ_NET_DEV_ON			1
#define	EZ_NET_BRODCAST_MODE	0
#define	EZ_NET_PROMISC_MODE		1

/* driver global definitions*/
#define EZ_NET_ETH_PKT_SZ		(ETH_FRAME_LEN+20)
#define EZ_NET_LAN_BUFFER_SIZE	0x600 /* 1.5K */
#define EZ_NET_REG_SIZE			4

/* GEMAC config register definitions */
#define GEMAC_CFG_INIT_VALUE	0x70C8501F
#define GEMAC_CFG_REG_BASE		0xC0003000

/* TX CTL register definitions */
#define TX_CTL_BUSY				0x00008000
#define TX_CTL_ENABLE			0x00001000
#define TX_CTL_ERROR			0x00004000
#define TX_CTL_RESET			0x00000800
#define TX_CTL_LENGTH_MASK		0x000007FF

/* RX CTL register definitions */
#define RX_CTL_BUSY				0x00008000
#define RX_CTL_ENABLE			0x00001000
#define RX_CTL_ERROR			0x00004000
#define RX_CTL_CRC				0x00002000
#define RX_CTL_RESET			0x00000800
#define RX_CTL_LENGTH_MASK		0x000007FF

/* LAN register definitions  */
#define REG_GEMAC_TX_CTL     	(unsigned int *)(GEMAC_CFG_REG_BASE + 0x00)
#define REG_GEMAC_TXBUF_STS  	(unsigned int *)(GEMAC_CFG_REG_BASE + 0x04)
#define REG_GEMAC_TXBUF_DATA	(unsigned int *)(GEMAC_CFG_REG_BASE + 0x08)
#define REG_GEMAC_RX_CTL     	(unsigned int *)(GEMAC_CFG_REG_BASE + 0x10)
#define REG_GEMAC_RXBUF_STS  	(unsigned int *)(GEMAC_CFG_REG_BASE + 0x14)
#define REG_GEMAC_RXBUF_DATA 	(unsigned int *)(GEMAC_CFG_REG_BASE + 0x18)
#define REG_GEMAC_MAC_CFG    	(unsigned int *)(GEMAC_CFG_REG_BASE + 0x40)

/* driver private data structure */
struct ez_net_priv {
	int		msg_enable;
	int		status; /* EZ_NET_DEV_OFF or EZ_NET_DEV_ON */
	unsigned char	address[ETH_ALEN]; /* mac address */
	int		down_event;
	int		packet_len;
	int		rx_mode;
};

struct nps_enet_info {
	void (*board_enable_ctrl)(void);
	void (*board_disable_ctrl)(void);
	int (*board_irq_alloc)(struct net_device *netdev);
	void (*board_free_irq)(void *dev_id);
	void (*board_enable_tx_irq)(void);
	void (*board_disable_tx_irq)(void);
	unsigned long rx_enable_ctrl;
};

#endif /* _NPS_ENET_H */
