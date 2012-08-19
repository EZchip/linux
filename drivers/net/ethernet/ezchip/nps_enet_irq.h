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

#ifndef _NPS_ENET_IRQ_H
#define _NPS_ENET_IRQ_H

#include <linux/netdevice.h>
#include <linux/interrupt.h>

extern struct nps_enet_info *eznet_global_p;

#define array_int_len(x) ((x + 3) >> 2)

void he0_board_global_init(struct nps_enet_info *global_p);
void he1_board_global_init(struct nps_enet_info *global_p);

void ez_net_rx_irq_handler(struct net_device *netdev, unsigned int rx_ctrl);
void ez_net_tx_irq_handler(struct net_device *netdev, unsigned int tx_ctrl);

#endif /* _NPS_ENET_IRQ_H */
