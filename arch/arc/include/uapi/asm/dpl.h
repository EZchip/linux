/*******************************************************************************

  Copyright(c) 2015 EZchip Technologies.

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

#ifndef __PLAT_DPL_H_
#define __PLAT_DPL_H_

#include <linux/ioctl.h>

struct dpl_aux_reg {
	unsigned int address;
	unsigned int value;
};

struct dpl_reg_db {
	unsigned int address;
	unsigned int value;
};

#define DPL_BASE	0xe2
#define DPL_SET_AUX_REG	_IOW(DPL_BASE, 1, struct dpl_aux_reg)
#define DPL_GET_AUX_REG	_IOR(DPL_BASE, 2, struct dpl_aux_reg)
#define DPL_SET_REG_DB	_IOW(DPL_BASE, 3, struct dpl_reg_db)
#define DPL_GET_REG_DB	_IOR(DPL_BASE, 4, struct dpl_reg_db)


#endif /* __PLAT_DPL_H_ */
