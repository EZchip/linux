/*
 * Copyright(c) 2015 EZchip Technologies.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 */

#include <linux/init.h>
#include <asm/mach_desc.h>

static const char *eznps_compat[] __initconst = {
	"ezchip,arc-nps",
	NULL,
};

MACHINE_START(NPS, "nps")
	.dt_compat	= eznps_compat,
MACHINE_END
