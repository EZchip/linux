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

#ifndef _NPS_WDT_H
#define _NPS_WDT_H

/* timeouts */
#define NPS_WDT_TIMEOUT_MIN		1
#define NPS_WDT_TIMEOUT_MAX		5
#define NPS_WDT_DEFAULT_TIMEOUT		NPS_WDT_TIMEOUT_MAX

/* register magic values */
#define NPS_WDT_CFG_CODE_UNLOCK		0x2acce55
#define NPS_WDT_SRV_RELOAD		0x412e522e

/* register definitions  */
#define NPS_WDT_REG_SOC_WDOG_CFG	0x0
#define NPS_WDT_REG_SOC_WDOG_LOAD	0x4
#define NPS_WDT_REG_SOC_WDOG_CNTVAL	0xC
#define NPS_WDT_REG_SOC_WDOG_SRV	0x10
#define NPS_WDT_REG_SOC_WDOG_CFGLOCK	0x14

#define SECS_TO_WDOG_TICKS(x, w) ((w->clock_frequency) * (x))
#define WDOG_TICKS_TO_SECS(x, w) ((w->clock_frequency) / (x))

struct nps_wdt {
	void __iomem *regs_base;
	unsigned int clock_frequency;
	struct watchdog_device wdt_device;
};

struct nps_wdt_soc_wdog_cfg {
	union {
		struct {
			u32
			reserved:27,
			boot_en_val:1,
			rst_msk:1,
			irq_msk:1,
			boot_en_msk:1,
			en:1;
		};

		u32 value;
	};
};

struct nps_wdt_soc_wdog_cfglock {
	union {
		struct {
			u32
			status:4,
			code:28;
		};

		u32 value;
	};
};

static inline void nps_wdt_reg_set(struct nps_wdt *drvdata,
					s32 reg, s32 value)
{
	iowrite32be(value, drvdata->regs_base + reg);
}

static inline u32 nps_wdt_reg_get(struct nps_wdt *drvdata, s32 reg)
{
	return ioread32be(drvdata->regs_base + reg);
}

#endif /* _NPS_WDT_H */
