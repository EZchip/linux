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

#ifndef _PLAT_EZNPS_TIME_H
#define _PLAT_EZNPS_TIME_H

#include <linux/clocksource.h>

void arc_platform_timer_init(void);
void arc_platform_periodic_timer_setup(unsigned int limit);
cycle_t arc_platform_read_timer1(struct clocksource *cs);

#ifdef CONFIG_SMP
unsigned long long rtc64(unsigned int cpu);
#endif

#endif /* _PLAT_EZNPS_TIME_H */
