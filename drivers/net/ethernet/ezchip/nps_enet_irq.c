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

#include <linux/io.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include "nps_enet.h"
#include "nps_enet_irq.h"

static void read_rx_fifo(unsigned int *dest, int length)
{
	int i, k;
	unsigned int buf;

	k = array_int_len(length);

	/* memory address is aligned */
	if (((int)dest & 0x3) == 0) {
		for (i = 0; i < k; i++)
			dest[i] = __raw_readl(REG_GEMAC_RXBUF_DATA);
	} else { /* not aligned */
		for (i = 0; i < k; i++) {
			buf = __raw_readl(REG_GEMAC_RXBUF_DATA);
			memcpy(dest+i, &buf, EZ_NET_REG_SIZE);
		}
	}
}

static void clean_rx_fifo(int frame_len)
{
	int i, k;

	k = array_int_len(frame_len);

	/* Empty Rx FIFO buffer by reading all words */
	for (i = 0; i < k; i++)
		__raw_readl(REG_GEMAC_RXBUF_DATA);
}

static bool is_valid_enter_frame(struct net_device *netdev, char *bufRecv)
{
	struct ez_net_priv *netPriv = netdev_priv(netdev);

	/* Check destination MAC address */
	if (compare_ether_addr(bufRecv, netPriv->address) == 0)
		return true;

	/* Value 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF for broadcast */
	if (is_broadcast_ether_addr(bufRecv) == true)
		return true;

	/* Promisc mode */
	if (netPriv->rx_mode == EZ_NET_PROMISC_MODE)
		return true;

	return false;
}

void ez_net_rx_irq_handler(struct net_device *netdev, unsigned int rx_ctrl)
{
	struct sk_buff *skb;
	int frame_len;

	frame_len = rx_ctrl & RX_CTL_LENGTH_MASK;

	/* Check that the hardware finished updating the ctrl
	 * in slow hardware this will prevent race conditions on the ctrl
	 */
	if ((rx_ctrl & RX_CTL_BUSY) == 0)
		goto rx_irq_finish;

	/* Check RX error */
	if ((rx_ctrl & RX_CTL_ERROR) != 0) {
		netdev->stats.rx_errors++;
		goto rx_irq_error;
	}

	/* Check RX crc error */
	if ((rx_ctrl & RX_CTL_CRC) != 0) {
		netdev->stats.rx_crc_errors++;
		netdev->stats.rx_dropped++;
		goto rx_irq_error;
	}

	/* Check Frame length (MAX 1.5k Min 64b) */
	if (unlikely(frame_len > EZ_NET_LAN_BUFFER_SIZE
			|| frame_len < ETH_ZLEN)) {
		netdev->stats.rx_dropped++;
		goto rx_irq_error;
	}

	skb = netdev_alloc_skb_ip_align(netdev, (frame_len + 32));

	/* Check skb allocation */
	if (unlikely(skb == NULL)) {
		netdev->stats.rx_errors++;
		netdev->stats.rx_dropped++;
		goto rx_irq_error;
	}

	read_rx_fifo((unsigned int *)skb->data, frame_len);

	if (is_valid_enter_frame(netdev, skb->data) == false) {
		netdev->stats.rx_dropped++;
		dev_kfree_skb_irq(skb);
	} else {
		skb_put(skb, frame_len);
		skb->dev = netdev;
		skb->protocol = eth_type_trans(skb, netdev);
		skb->ip_summed = CHECKSUM_UNNECESSARY;
		netif_rx(skb);
		netdev->stats.rx_packets++;
		netdev->stats.rx_bytes += frame_len;
	}

	goto rx_irq_frame_done;

rx_irq_error:
	/* Clean RX fifo */
	clean_rx_fifo(frame_len);

rx_irq_frame_done:
	/* Clean RX CTL register */
	__raw_writel(eznet_global_p->rx_enable_ctrl, REG_GEMAC_RX_CTL);

rx_irq_finish:
	return;
}

void ez_net_tx_irq_handler(struct net_device *netdev, unsigned int tx_ctrl)
{
	struct ez_net_priv *netPriv = netdev_priv(netdev);
	/*
	 * This is because we don't want to get TX end interrupt
	 * until we send a frame ( TX_CTL CT must be 0  )
	 */
	eznet_global_p->board_disable_tx_irq();

	/* Check Tx error */
	if (unlikely(tx_ctrl & TX_CTL_ERROR) != 0) {
		netdev->stats.tx_errors++;
		/* clean TX CTL error */
		tx_ctrl &= ~(TX_CTL_ERROR);
		__raw_writel(tx_ctrl, REG_GEMAC_TX_CTL);
	} else {
		netdev->stats.tx_packets++;
		netdev->stats.tx_bytes += netPriv->packet_len;
	}

	/* In ez_net_start_xmit we disabled sending frames*/
	netif_wake_queue(netdev);
}
