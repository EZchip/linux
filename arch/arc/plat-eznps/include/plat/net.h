/*******************************************************************************

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

#ifndef __PLAT_NET_H
#define __PLAT_NET_H

#include <linux/socket.h>

#ifdef CONFIG_EZNPS_NET
/*
 * this MAC address is defined in network driver
 * and used in platform in order to initialize
 * it with value given from command line
 */
extern struct sockaddr mac_addr;

#endif

#endif /* __PLAT_NET_H */
