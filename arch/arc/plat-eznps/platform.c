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

#include <linux/init.h>
#include <linux/io.h>
#include <asm/mach_desc.h>
#include <plat/smp.h>
#include <plat/time.h>

/*
 * Early Platform Initialization called from setup_arch()
 */
static void __init eznps_plat_early_init(void)
{
	pr_info("[plat-eznps]: registering early dev resources\n");

#ifdef CONFIG_SMP
	eznps_init_early_smp();
#endif
}

/*----------------------- Machine Descriptions ------------------------------
 *
 * Machine description is simply a set of platform/board specific callbacks
 * This is not directly related to DeviceTree based dynamic device creation,
 * however as part of early device tree scan, we also select the right
 * callback set, by matching the DT compatible name.
 */

static const char *eznps_compat[] __initconst = {
	"ezchip,arc-nps",
	NULL,
};

MACHINE_START(NPS, "nps")
	.dt_compat	= eznps_compat,
	.init_early	= eznps_plat_early_init,
#ifdef CONFIG_SMP
	.init_time	= eznps_counter_setup,
	.init_smp	= eznps_smp_init_cpu,  /* for each CPU */
#endif
MACHINE_END
