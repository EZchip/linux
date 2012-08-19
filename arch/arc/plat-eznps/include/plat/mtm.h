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

#ifndef _PLAT_EZNPS_MTM_H
#define _PLAT_EZNPS_MTM_H

#ifdef CONFIG_EZNPS_MTM_EXT
#include <plat/ctop.h>
#include <plat/smp.h>

/* Core auxiliary registers */
#define AUX_REG_MT_CTRL			(CTOP_AUX_BASE + 0x20)
#define AUX_REG_HW_COMPLY		(CTOP_AUX_BASE + 0x24)
#define AUX_REG_TSI1			(CTOP_AUX_BASE + 0x50)

/* MTM registers */
#define MTM_CFG(cpu)			nps_mtm_reg_addr(cpu, 0x81)
#define MTM_THR_INIT(cpu)		nps_mtm_reg_addr(cpu, 0x92)
#define MTM_THR_INIT_STS(cpu)		nps_mtm_reg_addr(cpu, 0x93)

#define get_thread(map) map.thread
#define eznps_max_cpus 4096

void mtm_enable_core(unsigned int cpu);
int mtm_enable_thread(int cpu);
#else

#define get_thread(map) 0
#define eznps_max_cpus 256
#define mtm_enable_core(cpu)
#define mtm_enable_thread(cpu) 1

#endif /* CONFIG_EZNPS_MTM_EXT */

#endif /* _PLAT_EZNPS_MTM_H */
