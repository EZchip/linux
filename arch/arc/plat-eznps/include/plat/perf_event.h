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
