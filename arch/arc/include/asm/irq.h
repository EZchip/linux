/******************************************************************************
 * Copyright Codito Technologies (www.codito.com) Oct 01, 2004
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *****************************************************************************/

/*
 * include/asm-arc/irq.h
 *
 * Amit Shah: initial write from asm-armnommu/
 */

#ifndef __ASM_ARC_IRQ_H
#define __ASM_ARC_IRQ_H

#include <asm/ptrace.h>
#include <plat_irq.h>   // Board Specific IRQ assignments

#define irq_canonicalize(i)    (i)

#define IRQ_FLG_LOCK    (0x0001)        /* handler is not replaceable   */
#define IRQ_FLG_REPLACE (0x0002)        /* replace existing handler     */
#define IRQ_FLG_STD     (0x8000)        /* internally used              */

#ifndef __ASSEMBLY__

#define disable_irq_nosync(i) disable_irq(i)

extern void disable_irq(unsigned int);
extern void enable_irq(unsigned int);
extern int  get_hw_config_num_irq(void);

#endif

#endif
