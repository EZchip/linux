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

#ifndef __PLAT_PERF_EVENT_H
#define __PLAT_PERF_EVENT_H

#ifdef CONFIG_PERF_EVENTS
/* NPS PMU */
#define NPS_MAX_COUNTERS	2
#define NPS_PMU_COUNTER_SIZE	32
/* Perf Counters Registers */
#define NPS_REG_PCT_COUNT0	0x250
#define NPS_REG_PCT_COUNT1	0x251
#define NPS_REG_PCT_CONFIG0	0x254
#define NPS_REG_PCT_CONFIG1	0x255
#define NPS_REG_PCT_CONTROL	0x258

/* Perf Counter Config Register */
struct nps_reg_pct_config {
	union {
		struct {
			unsigned int ie:1, r:3, ofsel:2, ae:1, e1:1,
			e0:1, km:1, um:1, gl:1, tid:4, n:1, condition:15;
		};
		unsigned int value;
	};
};

/* Perf Counter Control Register */
struct nps_reg_pct_control {
	union {
		struct {
			unsigned int r:16, n:1, condition:15;
		};
		unsigned int value;
	};
};
#define perf_control_save(pflags)				\
	do {							\
		typecheck(unsigned long, pflags);		\
		pflags = read_aux_reg(NPS_REG_PCT_CONTROL);	\
		write_aux_reg(NPS_REG_PCT_CONTROL, 0);		\
	} while (0)

#define perf_control_restore(pflags)				\
	do {							\
		typecheck(unsigned long, pflags);		\
		write_aux_reg(NPS_REG_PCT_CONTROL, pflags);	\
	} while (0)
#else
#define perf_control_save(pflags)
#define perf_control_restore(pflags)
#endif /* CONFIG_PERF_EVENTS */

#endif /* __PLAT_PERF_EVENT_H */
