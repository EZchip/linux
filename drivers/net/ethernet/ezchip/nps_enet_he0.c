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

#include <linux/etherdevice.h>
#include <linux/interrupt.h>
#include "nps_enet.h"
#include "nps_enet_irq.h"

#define IRQ_LAN_RX_LINE			7
#define IRQ_LAN_TX_LINE			8

/******************************************************************
 * LAN IRQ board handling
 ******************************************************************/

static irqreturn_t he0_board_rx_irq(int irq, void *dev_id)
{
	unsigned int rx_ctrl;
	struct net_device *netdev = dev_id;
	struct ez_net_priv *netPriv = netdev_priv(netdev);

	if (unlikely(!netdev || netPriv->status == EZ_NET_DEV_OFF))
		return IRQ_NONE;

	rx_ctrl = __raw_readl(REG_GEMAC_RX_CTL);

	ez_net_rx_irq_handler(netdev, rx_ctrl);

	return IRQ_HANDLED;
}

static irqreturn_t he0_board_tx_irq(int irq, void *dev_id)
{
	unsigned int tx_ctrl;
	struct net_device *netdev = dev_id;
	struct ez_net_priv *netPriv = netdev_priv(netdev);

	if (unlikely(!netdev || netPriv->status == EZ_NET_DEV_OFF))
		return IRQ_NONE;

	tx_ctrl = __raw_readl(REG_GEMAC_TX_CTL);
	ez_net_tx_irq_handler(netdev, tx_ctrl);

	return IRQ_HANDLED;
}

static irqreturn_t he0_board_irq_hanlder(int irq, void *dev_id)
{
	if (irq == IRQ_LAN_RX_LINE)
		return he0_board_rx_irq(irq, dev_id);

	if (irq == IRQ_LAN_TX_LINE)
		return he0_board_tx_irq(irq, dev_id);

	return IRQ_NONE;
}
static int he0_board_irq_alloc(struct net_device *netdev)
{
	struct ez_net_priv *netPriv;
	int err;

	netPriv = netdev_priv(netdev);

	/* irq Rx allocation */
	err = request_irq(IRQ_LAN_RX_LINE, he0_board_irq_hanlder,
			 0 , "eth0-rx", netdev);
	if (err) {
		netif_err(netPriv, probe, netdev,
			"EZnet: Fail to allocate IRQ %d - err %d\n",
			IRQ_LAN_RX_LINE, err);
		return err;
	}

	/* irq Tx allocation */
	err = request_irq(IRQ_LAN_TX_LINE, he0_board_irq_hanlder,
			0 , "eth0-tx", netdev);
	if (err) {
		netif_err(netPriv, probe, netdev,
			"EZnet: Fail to allocate IRQ %d - err %d\n",
			IRQ_LAN_TX_LINE, err);
		return err;
	}

	return 0;
}

static void he0_board_free_irq(void *dev_id)
{
	free_irq(IRQ_LAN_RX_LINE, dev_id);
	free_irq(IRQ_LAN_TX_LINE, dev_id);
}

static void he0_board_enable_tx_irq(void)
{
	enable_irq(IRQ_LAN_TX_LINE);
}

static void he0_board_disable_tx_irq(void)
{
	disable_irq_nosync(IRQ_LAN_TX_LINE);
}

void he0_board_global_init(struct nps_enet_info *global_p)
{
	global_p->board_irq_alloc = he0_board_irq_alloc;
	global_p->board_free_irq = he0_board_free_irq;
	global_p->board_enable_tx_irq = he0_board_enable_tx_irq;
	global_p->board_disable_tx_irq = he0_board_disable_tx_irq;
}
