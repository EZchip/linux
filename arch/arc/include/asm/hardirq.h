/******************************************************************************
 * Copyright ARC International (www.arc.com) 2007-2009
 *
 * Vineetg: Feb 2008
 *  -Remove local_irq_count( ) from irq_stat no longer required by
 *  generic code
 *****************************************************************************/
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
 *  linux/include/asm-arc/hardirq.h
 *
 *  Taken from m68knommu
 *
 *  Copyright (C)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Authors: Amit Bhor, Sameer Dhavale
 */
#ifndef _ASM_ARC_HARDIRQ_H
#define _ASM_ARC_HARDIRQ_H

#include <linux/threads.h>
#include <linux/irq.h>

typedef struct {
    unsigned int __softirq_pending;
} ____cacheline_aligned irq_cpustat_t;

#include <linux/irq_cpustat.h>  /* Standard mappings for irq_cpustat_t above */

#endif /* _ASM_ARC_HARDIRQ_H */
