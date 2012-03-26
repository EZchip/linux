/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _ARCH_ARC_HW_IRQ_H
#define _ARCH_ARC_HW_IRQ_H

static inline void ack_bad_irq(int irq)
{
	printk("unexpected IRQ # %d\n", irq);
}

#endif
