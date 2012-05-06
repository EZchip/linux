/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARC_IRQ_H
#define __ASM_ARC_IRQ_H

#include <linux/irqreturn.h>
#include <plat_irq.h>   // Board Specific IRQ assignments

#define irq_canonicalize(i)    (i)

#define IRQ_FLG_LOCK    (0x0001)        /* handler is not replaceable   */
#define IRQ_FLG_REPLACE (0x0002)        /* replace existing handler     */
#define IRQ_FLG_STD     (0x8000)        /* internally used              */

#ifndef __ASSEMBLY__

extern int  get_hw_config_num_irq(void);

#endif

#endif
