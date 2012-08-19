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

#include <linux/netdevice.h>
#include <linux/interrupt.h>
#include "nps_enet.h"
#include "nps_enet_irq.h"

#define IRQ_LAN_RXTX_LINE			7

static void he1_board_disable_control(void)
{
	unsigned int ctrl;

	/* Disable Tx ctrl enable */
	ctrl = __raw_readl(REG_GEMAC_TX_CTL);
	ctrl &= ~TX_CTL_ENABLE;
	__raw_writel(ctrl, REG_GEMAC_TX_CTL);

	/* Disable Rx ctrl enable */
	ctrl = __raw_readl(REG_GEMAC_RX_CTL);
	ctrl &= ~RX_CTL_ENABLE;
	__raw_writel(ctrl, REG_GEMAC_RX_CTL);
}

static void he1_board_enable_control(void)
{
	unsigned int rx_ctrl;

	/* enable Rx
	 * Tx will be enabled when we send a frame
	 */
	rx_ctrl = __raw_readl(REG_GEMAC_RX_CTL);
	rx_ctrl = rx_ctrl | RX_CTL_ENABLE;
	__raw_writel(rx_ctrl, REG_GEMAC_RX_CTL);
}

/******************************************************************
 * LAN IRQ board handling
 ******************************************************************/

static irqreturn_t he1_board_irq_hanlder(int irq, void *dev_id)
{
	struct net_device *netdev = dev_id;
	struct ez_net_priv *netPriv = netdev_priv(netdev);
	unsigned int rx_ctrl, tx_ctrl;

	if (unlikely(!netdev || netPriv->status == EZ_NET_DEV_OFF))
		return IRQ_NONE;

	/* Check for Rx ready */
	rx_ctrl = __raw_readl(REG_GEMAC_RX_CTL);
	if (rx_ctrl & RX_CTL_BUSY) {
		ez_net_rx_irq_handler(netdev, rx_ctrl);
	}

	/* Check for Tx done */
	tx_ctrl = __raw_readl(REG_GEMAC_TX_CTL);
	if (((tx_ctrl & TX_CTL_BUSY) == 0) && (tx_ctrl & TX_CTL_ENABLE)) {
		ez_net_tx_irq_handler(netdev, tx_ctrl);
	}

	return IRQ_HANDLED;
}

static int he1_board_irq_alloc(struct net_device *netdev)
{
	struct ez_net_priv *netPriv;
	int ret_val;

	/* LAN irq allocation */
	ret_val = request_irq(IRQ_LAN_RXTX_LINE, he1_board_irq_hanlder,
			0 , "eth0", netdev);
	if (ret_val) {
		netPriv = netdev_priv(netdev);
		netif_err(netPriv, probe, netdev,
			"EZnet: Fail to allocate IRQ %d - err %d\n",
			IRQ_LAN_RXTX_LINE, ret_val);
	}

	return ret_val;
}

static void he1_board_free_irq(void *dev_id)
{
	free_irq(IRQ_LAN_RXTX_LINE, dev_id);
}

static void he1_board_enable_tx_irq(void)
{
	unsigned int tx_ctrl;

	/* Need some delay after setting TX_CTL_BUSY
	 * 100 micro Seconds is an empiric value
	 * since 1 micro was not enough
	 * in addition to the ~1500 cycles (about 50 micro) between writes.
	 * without this delay compiler optimization write TX_CTL_ENABLE
	 * at the same time as TX_CTL_BUSY.
	 */
	udelay(100);

	tx_ctrl = __raw_readl(REG_GEMAC_TX_CTL);
	tx_ctrl |= TX_CTL_ENABLE;
	__raw_writel(tx_ctrl, REG_GEMAC_TX_CTL);
}

static void he1_board_disable_tx_irq(void)
{
	unsigned int tx_ctrl;

	tx_ctrl = __raw_readl(REG_GEMAC_TX_CTL);
	tx_ctrl &= ~TX_CTL_ENABLE;
	__raw_writel(tx_ctrl, REG_GEMAC_TX_CTL);
}

void he1_board_global_init(struct nps_enet_info *global_p)
{
	global_p->board_enable_ctrl = he1_board_enable_control;
	global_p->board_disable_ctrl = he1_board_disable_control;
	global_p->board_irq_alloc = he1_board_irq_alloc;
	global_p->board_free_irq = he1_board_free_irq;
	global_p->board_enable_tx_irq = he1_board_enable_tx_irq;
	global_p->board_disable_tx_irq = he1_board_disable_tx_irq;
	global_p->rx_enable_ctrl = RX_CTL_ENABLE;
}
