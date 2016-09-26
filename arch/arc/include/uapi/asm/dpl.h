/*
* Copyright (c) 2016, Mellanox Technologies. All rights reserved.
*
* This software is available to you under a choice of one of two
* licenses.  You may choose to be licensed under the terms of the GNU
* General Public License (GPL) Version 2, available from the file
* COPYING in the main directory of this source tree, or the
* OpenIB.org BSD license below:
*
*     Redistribution and use in source and binary forms, with or
*     without modification, are permitted provided that the following
*     conditions are met:
*
*      - Redistributions of source code must retain the above
*        copyright notice, this list of conditions and the following
*        disclaimer.
*
*      - Redistributions in binary form must reproduce the above
*        copyright notice, this list of conditions and the following
*        disclaimer in the documentation and/or other materials
*        provided with the distribution.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
* NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
* BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
* ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
* CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

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
#define DPL_SET_PCT_REG	_IOW(DPL_BASE, 5, struct dpl_aux_reg)
#define DPL_GET_PCT_REG	_IOR(DPL_BASE, 6, struct dpl_aux_reg)


#endif /* __PLAT_DPL_H_ */
